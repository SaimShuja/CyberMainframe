/**
 * @file mainframe_ram.v
 * @brief Dual-port block RAM for the Cyber-Mainframe game grid.
 *
 * This module implements a 5600-word × 4-bit synchronous dual-port RAM.
 *   - Port A: Read-only, used by the display compositor to fetch cell colours
 *             for the current pixel being rendered.
 *   - Port B: Read/write, connected to the SPI receiver for live updates from
 *             the ESP32, and also used by the hardware override state machine
 *             for fast clear/fill operations.
 *
 * The memory is initialised at power‑up from a hex file (`mainframe_init.hex`),
 * which can contain a splash screen or default game layout.
 *
 * @author  Saim Shujah
 * @date    2026-06-23
 * @version 1.0
 */

module mainframe_ram (
    // ------------------------------------------------------------------------
    // Clock
    // ------------------------------------------------------------------------
    input wire        clk,               // 33 MHz system clock

    // ------------------------------------------------------------------------
    // Port A: LCD Presentation Engine Link (Read-Only)
    // ------------------------------------------------------------------------
    input wire [12:0] addr_a,            // 13‑bit address (0..5599)
    output reg [3:0]  dout_a,            // 4‑bit data read (cell colour ID)

    // ------------------------------------------------------------------------
    // Port B: ESP32 SPI Synchronisation Link (Read/Write)
    // ------------------------------------------------------------------------
    input wire [12:0] addr_b,            // 13‑bit address (0..5599)
    input wire [3:0]  din_b,             // 4‑bit data to write
    input wire        we_b,              // Write-enable (active‑high)
    output reg [3:0]  dout_b             // 4‑bit data read
);

    // =========================================================================
    // MEMORY ARRAY DECLARATION
    // =========================================================================

    // 5600 cells: 100 columns × 56 rows.
    // Each cell stores a 4‑bit colour ID:
    //   0  = empty/dead
    //   1  = Player 1 (Red)
    //   2  = Player 2 (Blue)
    //   3  = Player 3 (Green)
    //   4  = Player 4 (Yellow)
    //   5  = Wall
    (* ram_style = "block" *) reg [3:0] matrix_mem [0:5599];

    // =========================================================================
    // INITIALISATION
    // =========================================================================

    // Load the initial contents from a hexadecimal file at synthesis.
    // This file should contain 5600 hex digits (0‑F), one per cell.
    // It can define a boot splash screen, a default arena layout, or test pattern.
    initial begin
        $readmemh("mainframe_init.hex", matrix_mem);
    end

    // =========================================================================
    // PORT A: DEDICATED SYNCHRONOUS SCANLINE READ PATH
    // =========================================================================

    // Port A is read‑only for the display compositor.
    // It reads the cell colour at the current pixel address each clock cycle.
    always @(posedge clk) begin
        dout_a <= matrix_mem[addr_a];
    end

    // =========================================================================
    // PORT B: DEDICATED SYNCHRONOUS SPI PIPELINE STROBE PATH
    // =========================================================================

    // Port B serves the SPI receiver and the hardware override controller.
    // - When we_b is high, the memory cell at addr_b is updated with din_b.
    // - When we_b is low, the cell content is read out on dout_b.
    // This dual‑role supports both live game updates and fast clear/fill loops.
    always @(posedge clk) begin
        if (we_b) begin
            matrix_mem[addr_b] <= din_b;   // Write
        end else begin
            dout_b <= matrix_mem[addr_b];  // Read
        end
    end

endmodule