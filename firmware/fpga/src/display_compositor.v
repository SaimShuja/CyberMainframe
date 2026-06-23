/**
 * @file display_compositor.v
 * @brief Graphics compositor for the Cyber-Mainframe game display.
 *
 * This module renders the final RGB output for the LCD panel. It combines:
 *   - A 100x56 playfield with 8x8 pixel sprites (cells and walls).
 *   - A HUD overlay with player status bars, timer, and invader indicators.
 *   - Dynamic colouring based on game mode (especially for GENETIC_TAKEOVER).
 *
 * The compositor operates in real-time, driven by the video timing controller.
 * It reads cell data from the framebuffer (via grid_rd_addr) and overlays HUD
 * elements in the lower 32-pixel band.
 *
 * @author  Saim Shujah
 * @date    2026-06-23
 * @version 1.0
 */

module display_compositor (
    // ------------------------------------------------------------------------
    // Pixel Coordinate and Video Enable
    // ------------------------------------------------------------------------
    input wire [9:0]  pixel_x,          // Current pixel X coordinate (0..799)
    input wire [9:0]  pixel_y,          // Current pixel Y coordinate (0..479)
    input wire        video_de,         // Data enable – active during visible area

    // ------------------------------------------------------------------------
    // Framebuffer Memory Interface
    // ------------------------------------------------------------------------
    output wire [12:0] grid_rd_addr,    // 13-bit address into the 5600-cell RAM
    input wire [3:0]   grid_rd_data,    // Cell value: 0=dead, 1..4=team colours, 5=wall

    // ------------------------------------------------------------------------
    // HUD Data from SPI Receiver (updated each frame)
    // ------------------------------------------------------------------------
    input wire [1:0]   game_mode,       // 0=Meltdown, 1=Takeover, 2=Critical Mass
    input wire [6:0]   p1_status,       // Player 1 status (0..100%)
    input wire [6:0]   p2_status,
    input wire [6:0]   p3_status,
    input wire [6:0]   p4_status,
    input wire [5:0]   time_minutes,    // Match minutes (0..59)
    input wire [5:0]   time_seconds,    // Match seconds (0..59)
    input wire [15:0]  invader_composite, // 16-bit packed invader IDs (4 bits each)

    // ------------------------------------------------------------------------
    // RGB Outputs (to LCD pins)
    // ------------------------------------------------------------------------
    output reg [4:0]  final_r,
    output reg [5:0]  final_g,
    output reg [4:0]  final_b
);

    // =========================================================================
    // LAYOUT ARCHITECTURE PARAMETERS
    // =========================================================================

    // The playfield occupies the top 448 pixels (56 rows × 8 pixels high).
    localparam V_FIELD_BOTTOM = 10'd448;

    // The HUD starts at pixel row 456, leaving an 8‑pixel gap below the field.
    localparam HUD_START_Y    = 10'd456;

    // The HUD is 16 pixels high (two rows of 8-pixel characters), spanning rows 456–471.

    // =========================================================================
    // 1. PLAYING FIELD SPRITE INDEX GENERATION
    // =========================================================================

    // Convert pixel coordinates to cell coordinates (each cell is 8x8 pixels).
    wire [6:0] cell_col = pixel_x / 8;
    wire [5:0] cell_row = pixel_y / 8;

    // Compute the RAM address for the current cell.
    // Only valid for pixels inside the playfield (Y < 448, X < 800).
    assign grid_rd_addr = (pixel_y < V_FIELD_BOTTOM && pixel_x < 800) ?
                          ((cell_row * 100) + cell_col) : 13'd0;

    // Pixel position within the current 8x8 sprite.
    wire [2:0] sprite_x = pixel_x[2:0];
    wire [2:0] sprite_y = pixel_y[2:0];

    // =========================================================================
    // 2. 8x8 HARDWARE SPRITE PALETTE MATRICES
    // =========================================================================

    /**
     * @brief 8x8 sprite for a cell (team node).
     * @return 2-bit code: 0=background, 1=outline, 2=accent, 3=core.
     */
    function [1:0] get_node_pixel;
        input [2:0] x; input [2:0] y;
        case (y)
            3'd0: get_node_pixel = (x==2||x==3||x==4||x==5) ? 2'd1 : 2'd0;
            3'd1: get_node_pixel = (x==1||x==6) ? 2'd1 : ((x==2||x==5) ? 2'd2 : 2'd0);
            3'd2: get_node_pixel = (x==0||x==7) ? 2'd1 : ((x==1||x==6) ? 2'd2 : 2'd3);
            3'd3: get_node_pixel = (x==0||x==7) ? 2'd1 : ((x==1||x==6) ? 2'd2 : 2'd3);
            3'd4: get_node_pixel = (x==0||x==7) ? 2'd1 : ((x==1||x==6) ? 2'd2 : 2'd3);
            3'd5: get_node_pixel = (x==0||x==7) ? 2'd1 : ((x==1||x==6) ? 2'd2 : 2'd3);
            3'd6: get_node_pixel = (x==1||x==6) ? 2'd1 : ((x==2||x==5) ? 2'd2 : 2'd0);
            3'd7: get_node_pixel = (x==2||x==3||x==4||x==5) ? 2'd1 : 2'd0;
        endcase
    endfunction

    /**
     * @brief 8x8 sprite for a wall cell.
     * @return 2-bit code: 0=background, 1=border, 2=inner pattern.
     */
    function [1:0] get_wall_pixel;
        input [2:0] x; input [2:0] y;
        case (y)
            3'd0, 3'd7: get_wall_pixel = 2'd1;
            3'd1, 3'd6: get_wall_pixel = (x==0||x==7) ? 2'd1 : 2'd2;
            default:    get_wall_pixel = (x==0||x==7) ? 2'd1 : ((x==1||x==6) ? 2'd2 : ((x==3||x==4) ? 2'd1 : 2'd0));
        endcase
    endfunction

    // =========================================================================
    // 3. TEXT & PROGRESS BAR LAYOUT MAPPING ENGINE
    // =========================================================================

    // Font row selection: each character is 7 pixels high (rows 0..6).
    wire [2:0] font_row_select = (pixel_y - HUD_START_Y) >> 1;

    // Pixel column within the 8‑pixel wide character.
    wire [2:0] font_col_select = pixel_x[2:0];

    // Character column index (0..99) based on 8‑pixel pitch.
    wire [6:0] text_column     = pixel_x / 8;

    /**
     * @brief 7x8 font ROM for HUD characters.
     * @param char_idx  Character index (0..18)
     * @param row       Row within the character (0..6)
     * @return 8‑bit bitmap (MSB is leftmost pixel).
     */
    function [7:0] get_font_line;
        input [4:0] char_idx; input [2:0] row;
        case (char_idx)
            5'd0:  case(row) 0:get_font_line=8'h3E; 1:get_font_line=8'h66; 2:get_font_line=8'h6E; 3:get_font_line=8'h7E; 4:get_font_line=8'h76; 5:get_font_line=8'h66; 6:get_font_line=8'h3E; default:get_font_line=8'h00; endcase // '0'
            5'd1:  case(row) 0:get_font_line=8'h18; 1:get_font_line=8'h38; 2:get_font_line=8'h18; 3:get_font_line=8'h18; 4:get_font_line=8'h18; 5:get_font_line=8'h18; 6:get_font_line=8'h7E; default:get_font_line=8'h00; endcase // '1'
            5'd2:  case(row) 0:get_font_line=8'h3E; 1:get_font_line=8'h66; 2:get_font_line=8'h06; 3:get_font_line=8'h1C; 4:get_font_line=8'h30; 5:get_font_line=8'h62; 6:get_font_line=8'h7E; default:get_font_line=8'h00; endcase // '2'
            5'd3:  case(row) 0:get_font_line=8'h3E; 1:get_font_line=8'h66; 2:get_font_line=8'h06; 3:get_font_line=8'h1E; 4:get_font_line=8'h06; 5:get_font_line=8'h66; 6:get_font_line=8'h3E; default:get_font_line=8'h00; endcase // '3'
            5'd4:  case(row) 0:get_font_line=8'h0C; 1:get_font_line=8'h1C; 2:get_font_line=8'h3C; 3:get_font_line=8'h6C; 4:get_font_line=8'h7E; 5:get_font_line=8'h0C; 6:get_font_line=8'h0C; default:get_font_line=8'h00; endcase // '4'
            5'd5:  case(row) 0:get_font_line=8'h7E; 1:get_font_line=8'h60; 2:get_font_line=8'h7C; 3:get_font_line=8'h06; 4:get_font_line=8'h06; 5:get_font_line=8'h66; 6:get_font_line=8'h3E; default:get_font_line=8'h00; endcase // '5'
            5'd6:  case(row) 0:get_font_line=8'h3E; 1:get_font_line=8'h60; 2:get_font_line=8'h7C; 3:get_font_line=8'h66; 4:get_font_line=8'h66; 5:get_font_line=8'h66; 6:get_font_line=8'h3E; default:get_font_line=8'h00; endcase // '6'
            5'd7:  case(row) 0:get_font_line=8'h7E; 1:get_font_line=8'h66; 2:get_font_line=8'h06; 3:get_font_line=8'h0C; 4:get_font_line=8'h18; 5:get_font_line=8'h18; 6:get_font_line=8'h18; default:get_font_line=8'h00; endcase // '7'
            5'd8:  case(row) 0:get_font_line=8'h3E; 1:get_font_line=8'h66; 2:get_font_line=8'h66; 3:get_font_line=8'h3E; 4:get_font_line=8'h66; 5:get_font_line=8'h66; 6:get_font_line=8'h3E; default:get_font_line=8'h00; endcase // '8'
            5'd9:  case(row) 0:get_font_line=8'h3E; 1:get_font_line=8'h66; 2:get_font_line=8'h66; 3:get_font_line=8'h3E; 4:get_font_line=8'h06; 5:get_font_line=8'h06; 6:get_font_line=8'h3E; default:get_font_line=8'h00; endcase // '9'
            5'd10: case(row) 0:get_font_line=8'h7C; 1:get_font_line=8'h66; 2:get_font_line=8'h66; 3:get_font_line=8'h7C; 4:get_font_line=8'h60; 5:get_font_line=8'h60; 6:get_font_line=8'h60; default:get_font_line=8'h00; endcase // 'P'
            5'd11: case(row) 0:get_font_line=8'h7E; 1:get_font_line=8'h60; 2:get_font_line=8'h60; 3:get_font_line=8'h7C; 4:get_font_line=8'h60; 5:get_font_line=8'h60; 6:get_font_line=8'h7E; default:get_font_line=8'h00; endcase // 'E'
            5'd12: case(row) 0:get_font_line=8'h7E; 1:get_font_line=8'h60; 2:get_font_line=8'h60; 3:get_font_line=8'h60; 4:get_font_line=8'h60; 5:get_font_line=8'h60; 6:get_font_line=8'h60; default:get_font_line=8'h00; endcase // 'F'
            5'd13: case(row) 0:get_font_line=8'h7C; 1:get_font_line=8'h66; 2:get_font_line=8'h66; 3:get_font_line=8'h66; 4:get_font_line=8'h66; 5:get_font_line=8'h66; 6:get_font_line=8'h7C; default:get_font_line=8'h00; endcase // 'D'
            5'd14: case(row) 0:get_font_line=8'h00; 1:get_font_line=8'h18; 2:get_font_line=8'h18; 3:get_font_line=8'h00; 4:get_font_line=8'h18; 5:get_font_line=8'h18; 6:get_font_line=8'h00; default:get_font_line=8'h00; endcase // ':'
            5'd15: case(row) 0:get_font_line=8'h3C; 1:get_font_line=8'h24; 2:get_font_line=8'h24; 3:get_font_line=8'h24; 4:get_font_line=8'h24; 5:get_font_line=8'h24; 6:get_font_line=8'h3C; default:get_font_line=8'h00; endcase // '['
            5'd16: case(row) 0:get_font_line=8'h3C; 1:get_font_line=8'h18; 2:get_font_line=8'h18; 3:get_font_line=8'h18; 4:get_font_line=8'h18; 5:get_font_line=8'h18; 6:get_font_line=8'h3C; default:get_font_line=8'h00; endcase // ']'
            5'd17: case(row) 0:get_font_line=8'h66; 1:get_font_line=8'h66; 2:get_font_line=8'h12; 3:get_font_line=8'h0C; 4:get_font_line=8'h24; 5:get_font_line=8'h66; 6:get_font_line=8'h66; default:get_font_line=8'h00; endcase // '%'
            5'd18: case(row) 0:get_font_line=8'hFF; 1:get_font_line=8'hFF; 2:get_font_line=8'hFF; 3:get_font_line=8'hFF; 4:get_font_line=8'hFF; 5:get_font_line=8'hFF; 6:get_font_line=8'hFF; default:get_font_line=8'hFF; endcase // Solid '█'
            default: get_font_line = 8'h00;
        endcase
    endfunction

    // -------------------------------------------------------------------------
    // HUD Layout Decoder – sets current_char, bar zone flags, and bar owner.
    // -------------------------------------------------------------------------

    reg [4:0] current_char;        // Character index for the current pixel position
    reg       is_inside_bar_zone;  // True when pixel lies within a progress bar area
    reg [2:0] bar_owner;           // 1..4 identifies which player's bar

    always @(*) begin
        current_char = 5'd31;      // Default: blank
        is_inside_bar_zone = 1'b0;
        bar_owner = 3'd0;

        // Only process HUD area (two rows of 8-pixel characters).
        if (pixel_y >= HUD_START_Y && pixel_y < (HUD_START_Y + 16)) begin
            case (text_column)
                // ----- Edge blocks -----
                7'd0, 7'd99: current_char = 5'd18; // '█'

                // ----- PLAYER 1 CLUSTER (Cols 2-14) -----
                7'd2:  current_char = 5'd10; // 'P'
                7'd3:  current_char = 5'd1;  // '1'
                7'd4:  current_char = (p1_status == 0) ? 5'd13 : 5'd15; // 'D' if dead, else '['
                7'd5, 7'd6, 7'd7, 7'd8, 7'd9, 7'd10: begin
                    is_inside_bar_zone = (p1_status > 0);
                    bar_owner = 3'd1;
                    if (p1_status == 0) current_char = 5'd11; // 'E' (part of "DEAD")
                end
                7'd11: current_char = (p1_status == 0) ? 5'd13 : 5'd16; // ']'
                7'd12: current_char = {1'b0, (p1_status / 10) % 10}; // Tens digit
                7'd13: current_char = {1'b0, p1_status % 10};        // Units digit
                7'd14: current_char = 5'd17; // '%'

                // ----- PLAYER 2 CLUSTER (Cols 18-30) -----
                7'd18: current_char = 5'd10; // 'P'
                7'd19: current_char = 5'd2;  // '2'
                7'd20: current_char = (p2_status == 0) ? 5'd13 : 5'd15;
                7'd21, 7'd22, 7'd23, 7'd24, 7'd25, 7'd26: begin
                    is_inside_bar_zone = (p2_status > 0);
                    bar_owner = 3'd2;
                    if (p2_status == 0) current_char = 5'd11;
                end
                7'd27: current_char = (p2_status == 0) ? 5'd13 : 5'd16;
                7'd28: current_char = {1'b0, (p2_status / 10) % 10};
                7'd29: current_char = {1'b0, p2_status % 10};
                7'd30: current_char = 5'd17;

                // ----- CENTRAL MATCH TIMER CLUSTER (Cols 45-51) -----
                7'd45: current_char = {1'b0, (time_minutes / 10) % 10};
                7'd46: current_char = {1'b0, time_minutes % 10};
                7'd47: current_char = 5'd14; // ':'
                7'd48: current_char = {1'b0, (time_seconds / 10) % 10};
                7'd49: current_char = {1'b0, time_seconds % 10};

                // ----- PLAYER 3 CLUSTER (Cols 66-78) -----
                7'd66: current_char = 5'd10;
                7'd67: current_char = 5'd3;
                7'd68: current_char = (p3_status == 0) ? 5'd13 : 5'd15;
                7'd69, 7'd70, 7'd71, 7'd72, 7'd73, 7'd74: begin
                    is_inside_bar_zone = (p3_status > 0);
                    bar_owner = 3'd3;
                    if (p3_status == 0) current_char = 5'd11;
                end
                7'd75: current_char = (p3_status == 0) ? 5'd13 : 5'd16;
                7'd76: current_char = {1'b0, (p3_status / 10) % 10};
                7'd77: current_char = {1'b0, p3_status % 10};
                7'd78: current_char = 5'd17;

                // ----- PLAYER 4 CLUSTER (Cols 84-96) -----
                7'd84: current_char = 5'd10;
                7'd85: current_char = 5'd4;
                7'd86: current_char = (p4_status == 0) ? 5'd13 : 5'd15;
                7'd87, 7'd88, 7'd89, 7'd90, 7'd91, 7'd92: begin
                    is_inside_bar_zone = (p4_status > 0);
                    bar_owner = 3'd4;
                    if (p4_status == 0) current_char = 5'd11;
                end
                7'd93: current_char = (p4_status == 0) ? 5'd13 : 5'd16;
                7'd94: current_char = {1'b0, (p4_status / 10) % 10};
                7'd95: current_char = {1'b0, p4_status % 10};
                7'd96: current_char = 5'd17;
            endcase
        end
    end

    // Retrieve the font bitmap for the selected character.
    wire [7:0] active_font_byte = get_font_line(current_char, font_row_select);
    // Pixel is on if the corresponding bit is '1' (MSB is leftmost).
    wire hud_text_pixel = active_font_byte[7 - font_col_select];

    // Temporary signals for bar drawing.
    reg [6:0] current_bar_pct;
    reg [6:0] segment_start_col;

    // =========================================================================
    // 3.5 DYNAMIC 4-BIT NIBBLE INVADER DATA SLICES
    // =========================================================================

    // Extract each player's invader ID from the 16-bit composite.
    wire [3:0] p1_dna_invader = invader_composite[3:0];
    wire [3:0] p2_dna_invader = invader_composite[7:4];
    wire [3:0] p3_dna_invader = invader_composite[11:8];
    wire [3:0] p4_dna_invader = invader_composite[15:12];

    // Active invader ID for the currently processed bar.
    reg [3:0]  active_dna_id;

    // =========================================================================
    // 4. DYNAMIC COLOR REGISTRY PIPE MUX
    // =========================================================================

    // This always block determines the RGB value for every pixel.
    // It first checks for arena (playfield) pixels, then HUD.
    always @(*) begin
        // Defaults: black output.
        final_r = 5'h00;
        final_g = 6'h00;
        final_b = 5'h00;
        current_bar_pct = 7'b0;
        segment_start_col = 7'b0;
        active_dna_id = 4'd0;

        if (video_de) begin
            // ============================================================
            // LAYER 1: ARENA MAP GRAPHICS (top 448 rows)
            // ============================================================
            if (pixel_y < V_FIELD_BOTTOM) begin
                // ---- Wall cells (ID = 5) ----
                if (grid_rd_data == 4'd5) begin
                    case (get_wall_pixel(sprite_x, sprite_y))
                        2'd1:    begin final_r = 5'h08; final_g = 6'h10; final_b = 5'h08; end // Dark green border
                        2'd2:    begin final_r = 5'h16; final_g = 6'h2D; final_b = 5'h16; end // Lighter green fill
                        2'd3:    begin final_r = 5'h1F; final_g = 6'h2A; final_b = 5'h00; end // Yellow accent
                        default: begin final_r = 5'h00; final_g = 6'h00; final_b = 5'h00; end
                    endcase
                end
                // ---- Team cells (ID = 1..4) ----
                else if (grid_rd_data >= 4'd1 && grid_rd_data <= 4'd4) begin
                    case (get_node_pixel(sprite_x, sprite_y))
                        2'd1: begin // Outline (neon trace)
                            if (grid_rd_data == 4'd1)      begin final_r = 5'h10; final_g = 6'h00; final_b = 5'h00; end // Red
                            else if (grid_rd_data == 4'd2) begin final_r = 5'h00; final_g = 6'h05; final_b = 5'h10; end // Blue
                            else if (grid_rd_data == 4'd3) begin final_r = 5'h00; final_g = 6'h15; final_b = 5'h05; end // Green
                            else                           begin final_r = 5'h15; final_g = 6'h1F; final_b = 5'h00; end // Yellow
                        end
                        2'd2: begin // Accent (core glow)
                            if (grid_rd_data == 4'd1)      begin final_r = 5'h1F; final_g = 6'h10; final_b = 5'h15; end // Pink
                            else if (grid_rd_data == 4'd2) begin final_r = 5'h00; final_g = 6'h3A; final_b = 5'h1F; end // Cyan
                            else if (grid_rd_data == 4'd3) begin final_r = 5'h0A; final_g = 6'h3F; final_b = 5'h02; end // Lime
                            else                           begin final_r = 5'h1F; final_g = 6'h3F; final_b = 5'h00; end // Yellow
                        end
                        2'd3:    begin final_r = 5'h1F; final_g = 6'h3F; final_b = 5'h1F; end // Bright white core
                        default: begin final_r = 5'h00; final_g = 6'h00; final_b = 5'h00; end
                    endcase
                end
                // ---- Empty background (dead cells) ----
                else begin
                    // Subtle checkerboard pattern for depth.
                    if (cell_col[0] ^ cell_row[0]) begin
                        final_r = 5'h00;
                        final_g = (sprite_x == 0 || sprite_y == 0) ? 6'h08 : 6'h02;
                        final_b = 5'h00;
                    end else begin
                        final_r = 5'h00; final_g = 6'h00; final_b = 5'h00;
                    end
                end
            end

            // ============================================================
            // LAYER 2: INTERACTIVE HUD MODULES (lower 16 pixels)
            // ============================================================
            else if (pixel_y >= HUD_START_Y && pixel_y < (HUD_START_Y + 16)) begin
                // ---- Text characters (white) ----
                if (hud_text_pixel) begin
                    final_r = 5'h1F;
                    final_g = 6'h3F;
                    final_b = 5'h1F;
                end
                // ---- Progress bars ----
                else if (is_inside_bar_zone) begin
                    // Determine which player's bar and extract its data.
                    case (bar_owner)
                        3'd1: begin current_bar_pct = p1_status; segment_start_col = 7'd5;  active_dna_id = p1_dna_invader; end
                        3'd2: begin current_bar_pct = p2_status; segment_start_col = 7'd21; active_dna_id = p2_dna_invader; end
                        3'd3: begin current_bar_pct = p3_status; segment_start_col = 7'd69; active_dna_id = p3_dna_invader; end
                        3'd4: begin current_bar_pct = p4_status; segment_start_col = 7'd87; active_dna_id = p4_dna_invader; end
                        default: begin current_bar_pct = 0; segment_start_col = 0; active_dna_id = 4'd0; end
                    endcase

                    // Compute relative X position within the bar (6 character-widths = 48 pixels).
                    if (pixel_x - (segment_start_col * 8) < ((current_bar_pct * 48) / 100)) begin
                        // ---- Fill (coloured) ----
                        // In GENETIC_TAKEOVER mode (2'b01), the bar colour shifts to the invader's colour.
                        if (game_mode == 2'b01 && active_dna_id != 4'd0) begin
                            case (active_dna_id)
                                4'd1:    begin final_r = 5'h1F; final_g = 6'h10; final_b = 5'h15; end // Invaded by P1 → Pink
                                4'd2:    begin final_r = 5'h00; final_g = 6'h3A; final_b = 5'h1F; end // Invaded by P2 → Cyan
                                4'd3:    begin final_r = 5'h0A; final_g = 6'h3F; final_b = 5'h02; end // Invaded by P3 → Lime
                                default: begin final_r = 5'h1F; final_g = 6'h3F; final_b = 5'h00; end // Invaded by P4 → Yellow
                            endcase
                        end else begin
                            // Standard team colour.
                            if (bar_owner == 3'd1)      begin final_r = 5'h1F; final_g = 6'h10; final_b = 5'h15; end // P1 Pink
                            else if (bar_owner == 3'd2) begin final_r = 5'h00; final_g = 6'h3A; final_b = 5'h1F; end // P2 Cyan
                            else if (bar_owner == 3'd3) begin final_r = 5'h0A; final_g = 6'h3F; final_b = 5'h02; end // P3 Lime
                            else                        begin final_r = 5'h1F; final_g = 6'h3F; final_b = 5'h00; end // P4 Yellow
                        end
                    end else begin
                        // ---- Empty bar background (dark grid) ----
                        final_r = 5'h02;
                        final_g = 6'h05;
                        final_b = 5'h02;
                    end
                end
            end
        end
    end

endmodule