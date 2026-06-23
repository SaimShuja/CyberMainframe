/**
 * @file spi_grid_rx.v
 * @brief SPI slave receiver for the Cyber-Mainframe FPGA display controller.
 *
 * This module parses the 5612-byte SPI packet sent by the ESP32. It extracts:
 *   - 5600 bytes of playfield data (cell colours) for the framebuffer.
 *   - 9 HUD configuration bytes (game mode, player statuses, timer, invader IDs).
 *   - Command flags for hardware clear/fill and winner colour ID.
 *
 * The receiver operates as a state machine that looks for the header (0xAA),
 * then counts bytes, and validates the footer (0xBB). HUD registers are
 * updated only after a complete, valid packet is received.
 *
 * @author  Saim Shujah
 * @date    2026-06-23
 * @version 1.0
 */

module spi_grid_rx (
    // ------------------------------------------------------------------------
    // SPI Interface
    // ------------------------------------------------------------------------
    input  wire        spi_clk,          // SPI clock from ESP32
    input  wire        spi_mosi,         // SPI data (master-out, slave-in)
    input  wire        spi_cs,           // SPI chip-select (active-low)

    // ------------------------------------------------------------------------
    // Framebuffer Write Interface
    // ------------------------------------------------------------------------
    output reg [12:0]  rx_addr,          // Address within the 5600-cell playfield
    output reg [3:0]   rx_data,          // 4-bit cell colour (0=empty, 1-4=teams)
    output reg         data_toggle,      // Toggles on each new valid data write

    // ------------------------------------------------------------------------
    // Live HUD Configuration Registers (updated on valid packet reception)
    // ------------------------------------------------------------------------
    output reg [1:0]   hud_game_mode,    // 0=Meltdown, 1=Takeover, 2=Critical Mass
    output reg [6:0]   hud_p1_status,    // Player 1 status (0-100)
    output reg [6:0]   hud_p2_status,    // Player 2 status (0-100)
    output reg [6:0]   hud_p3_status,    // Player 3 status (0-100)
    output reg [6:0]   hud_p4_status,    // Player 4 status (0-100)
    output reg [5:0]   hud_time_mins,    // Match minutes (0-59)
    output reg [5:0]   hud_time_secs,    // Match seconds (0-59)
    output reg [15:0]  hud_invader_composite,  // Direct 16-bit payload (bytes 7 & 8)

    // ------------------------------------------------------------------------
    // Hardware Override Commands (latched on valid packet)
    // ------------------------------------------------------------------------
    output reg         cmd_clear_match,  // Asserted when byte 5602 bit 7 = 1
    output reg         cmd_fill_winner,  // Asserted when byte 5602 bit 6 = 1
    output reg [2:0]   winner_color_id   // Extracted from byte 5609 bits [5:3]
);

    // =========================================================================
    // SPI RECEIVER STATE MACHINE
    // =========================================================================

    // State encoding: four states for packet parsing.
    localparam STATE_IDLE      = 2'b00;   // Waiting for header (0xAA)
    localparam STATE_FRAME_ID  = 2'b01;   // Receiving frame counter (ignored)
    localparam STATE_DATA      = 2'b10;   // Receiving payload (5600 data bytes + 9 HUD bytes)
    localparam STATE_FOOTER    = 2'b11;   // Waiting for footer (0xBB)

    reg [1:0]  state = STATE_IDLE;
    reg [2:0]  bit_cnt = 3'd0;           // Bit counter within a byte (0..7)
    reg [7:0]  shift_reg = 8'h00;        // Shift register for incoming serial data
    reg [12:0] byte_cnt = 13'd0;         // Total byte counter (0..5611)

    // -------------------------------------------------------------------------
    // HUD Buffer Registers – prevent flicker by updating only on valid footer.
    // -------------------------------------------------------------------------
    reg [1:0]  mode_buf;
    reg [6:0]  p1_buf; reg [6:0] p2_buf; reg [6:0] p3_buf; reg [6:0] p4_buf;
    reg [5:0]  min_buf; reg [5:0] sec_buf;

    // 16-bit tracking cache for the two invader bytes (byte 7 and byte 8).
    reg [15:0] invader_composite_buf;

    // Extraction buffers for command and winner ID, latched on footer.
    reg        clear_buf;
    reg        fill_buf;
    reg [2:0]  winner_buf;

    // Convenience wire: the complete assembled byte (MSB-first).
    wire [7:0] complete_byte = {shift_reg[6:0], spi_mosi};

    // =========================================================================
    // MAIN ALWAYS BLOCK – SPI Reception and State Machine
    // =========================================================================

    always @(posedge spi_clk or posedge spi_cs) begin
        if (spi_cs) begin
            // Chip-select high: reset all state machines and buffers.
            state       <= STATE_IDLE;
            bit_cnt     <= 3'd0;
            byte_cnt    <= 13'd0;
            data_toggle <= 1'b0;
        end else begin
            // Shift in the incoming bit (MSB-first).
            shift_reg <= {shift_reg[6:0], spi_mosi};

            if (bit_cnt == 3'd7) begin
                // A full byte has been assembled.
                bit_cnt <= 3'd0;

                case (state)
                    // -----------------------------------------------------------------
                    // STATE_IDLE: wait for the header magic number (0xAA)
                    // -----------------------------------------------------------------
                    STATE_IDLE: begin
                        if (complete_byte == 8'hAA) begin
                            state <= STATE_FRAME_ID;   // Header detected, move to frame ID
                        end
                    end

                    // -----------------------------------------------------------------
                    // STATE_FRAME_ID: receive the frame counter byte (ignored here)
                    // -----------------------------------------------------------------
                    STATE_FRAME_ID: begin
                        byte_cnt <= 13'd0;
                        state    <= STATE_DATA;        // Now ready for payload
                    end

                    // -----------------------------------------------------------------
                    // STATE_DATA: receive 5600 playfield bytes + 9 HUD bytes (total 5609)
                    // -----------------------------------------------------------------
                    STATE_DATA: begin
                        if (byte_cnt < 13'd5600) begin
                            // This is a playfield cell byte (only lower 4 bits are used).
                            rx_addr     <= byte_cnt;
                            rx_data     <= complete_byte[3:0];
                            data_toggle <= ~data_toggle;   // Pulse for each write
                        end else begin
                            // Byte index 5600 to 5608 are HUD configuration bytes.
                            case (byte_cnt)
                                13'd5600: begin   // Byte 0: game mode + flags
                                    mode_buf  <= complete_byte[1:0];   // Bits 1:0 = mode
                                    clear_buf <= complete_byte[7];     // Bit 7 = clear command
                                    fill_buf  <= complete_byte[6];     // Bit 6 = fill command
                                end

                                13'd5601: p1_buf <= complete_byte[6:0];   // Byte 1: Player 1 status
                                13'd5602: p2_buf <= complete_byte[6:0];   // Byte 2: Player 2 status
                                13'd5603: p3_buf <= complete_byte[6:0];   // Byte 3: Player 3 status
                                13'd5604: p4_buf <= complete_byte[6:0];   // Byte 4: Player 4 status

                                13'd5605: min_buf <= complete_byte[5:0];   // Byte 5: Minutes
                                13'd5606: sec_buf <= complete_byte[5:0];   // Byte 6: Seconds

                                13'd5607: begin  // Byte 7: invader IDs for P0/P1 (or winner ID in fill mode)
                                    invader_composite_buf[7:0] <= complete_byte;
                                    // Also capture winner ID (bits [5:3]) – used only during fill mode.
                                    winner_buf <= complete_byte[5:3];
                                end

                                13'd5608: begin  // Byte 8: invader IDs for P2/P3 (or dummy in fill mode)
                                    invader_composite_buf[15:8] <= complete_byte;
                                end
                            endcase
                        end

                        // After byte 5608, move to footer state.
                        if (byte_cnt == 13'd5608) begin
                            state <= STATE_FOOTER;
                        end else begin
                            byte_cnt <= byte_cnt + 1'b1;
                        end
                    end

                    // -----------------------------------------------------------------
                    // STATE_FOOTER: verify the footer magic number (0xBB)
                    // -----------------------------------------------------------------
                    STATE_FOOTER: begin
                        if (complete_byte == 8'hBB) begin
                            // Valid packet received – update all HUD registers atomically.
                            hud_game_mode          <= mode_buf;
                            hud_p1_status          <= p1_buf;
                            hud_p2_status          <= p2_buf;
                            hud_p3_status          <= p3_buf;
                            hud_p4_status          <= p4_buf;
                            hud_time_mins          <= min_buf;
                            hud_time_secs          <= sec_buf;
                            hud_invader_composite  <= invader_composite_buf;

                            // Latch hardware override commands.
                            cmd_clear_match        <= clear_buf;
                            cmd_fill_winner        <= fill_buf;
                            winner_color_id        <= winner_buf;
                        end
                        // Return to IDLE to wait for the next packet.
                        state <= STATE_IDLE;
                    end
                endcase
            end else begin
                // Not yet at 8 bits; increment bit counter.
                bit_cnt <= bit_cnt + 1'b1;
            end
        end
    end

endmodule