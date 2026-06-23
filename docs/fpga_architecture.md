# FPGA Architecture – CyberMainframe

This document describes the internal architecture of the **Gowin GW1N‑1** FPGA design that powers the CyberMainframe display. The FPGA receives SPI packets from the ESP32, stores the game grid in dual‑port block RAM, and renders a 100×56 arena with a live HUD on an 800×480 RGB LCD at 60 Hz.

The design is fully synchronous, running at **33 MHz**, and is organised into the following modules:

- **`top.v`** – top‑level, instantiates all submodules and hardware state machine.
- **`gowin_rpll.v`** – PLL generating 33 MHz from the 24 MHz input.
- **`spi_grid_rx.v`** – SPI slave receiver, parses the 5612‑byte packet.
- **`cyber_dpram.v`** – dual‑port block RAM (5600 × 4 bits) for the playfield.
- **`lcd_timing.v`** – video timing generator (HSYNC, VSYNC, DE, pixel coordinates).
- **`display_compositor.v`** – pixel‑by‑pixel compositor (arena + HUD).
- **`top.cst`** – physical pin constraints.
- **`mainframe_init.hex`** – initial memory content (boot splash).

---

## 1. Top‑Level Block Diagram

```text
                    +------------------------------------------------------+
                    |                      TOP                             |
                    |  +--------+   +-------------+   +-----------------+  |
      ext_clk_24m ->|  |  PLL   |   | SPI Slave   |   | HW State Machine|  |
      ext_rst_n   ->|  |(gowin_ |   |(spi_grid_rx)|   | (clear/fill)    |  |
                    |  | rpll)  |   |             |   |                 |  |
                    |  +---+----+   +-------+-----+   +--------+--------+  |
                    |      |                |                    |         |
                    |      | clk_33m        | rx_addr, rx_data,  |         |
                    |      |                | data_toggle        | waddr,   |
                    |      |                |                    | wdata, we|
                    |      |                v                    v          |
                    |      |         +-------+-------------------+---+      |
                    |      |         |       DUAL-PORT RAM           |      |
                    |      |         |      (cyber_dpram)            |     |
                    |      |         |  Port A (read)   Port B (wr)  |     |
                    |      |         +-------+--------------+--------+     |
                    |      |                 |              |              |
                    |      |        grid_rd_data         multiplexed       |
                    |      |                 |                             |
                    |      |                 v                             |
                    |      |         +-------+-----------+                 |
                    |      |         |   LCD Timing      |                 |
                    |      |         | (lcd_timing)      |                 |
                    |      |         |                   |                 |
                    |      |         | pixel_x,          |                 |
                    |      |         | pixel_y, video_de |                 |
                    |      |         +-------+-----------+                 |
                    |      |                 |                             |
                    |      |                 v                             |
                    |      |         +-------+--------+                    |
                    |      |         |  Compositor    |                    |
                    |      |         |(display_       |                    |
                    |      |         | compositor)    |                    |
                    |      |         |                |                    |
                    |      |         | final_r,g,b    |                    |
                    |      |         +-------+--------+                    |
                    |      |                 |                             |
                    +------+-----------------+-----------------------------+
                           |                 |
                           v                 v
                    lcd_pixel_clk      lcd_r, lcd_g, lcd_b
                    lcd_hsync, lcd_vsync, lcd_video_de
```

---

## 2. Clock Generation

The FPGA receives a 24 MHz clock from an external oscillator (onboard on the Tang Nano). The PLL (`gowin_rpll.v`) multiplies this to **33 MHz**, which is used as:

- The pixel clock for the LCD (drives `lcd_pixel_clk`).
- The main system clock for all synchronous logic.

The PLL provides a `lock` signal that, together with the physical reset button (`ext_rst_n`), forms the global reset `global_reset_n`. The design is held in reset until both the button is released and the PLL is locked.

---

## 3. SPI Slave Receiver – `spi_grid_rx`

This module implements the SPI slave interface (clock, MOSI, CS) and parses the incoming 5612‑byte packet as defined in the [SPI Protocol](spi_protocol.md). It outputs:

- **`rx_addr`** (13 bits) – address for the playfield RAM (0‑5599).
- **`rx_data`** (4 bits) – cell colour ID.
- **`data_toggle`** – strobe that toggles on each new byte written to RAM.
- **HUD registers** – `game_mode`, player statuses, timer, invader composite.
- **Command flags** – `cmd_clear_match`, `cmd_fill_winner`, `winner_color_id`.

The receiver uses a 4‑state state machine:
1. **IDLE** – waits for header `0xAA`.
2. **FRAME_ID** – receives the frame counter (ignored by the FPGA).
3. **DATA** – counts bytes from 0 to 5608; writes playfield bytes to RAM and latches HUD bytes.
4. **FOOTER** – verifies footer `0xBB`; upon success, updates the live HUD registers atomically.

All HUD registers are buffered to prevent flickering – they are only updated when a valid footer is received.

---

## 4. Dual‑Port RAM – `cyber_dpram`

The playfield is stored in a 5600‑word × 4‑bit block RAM (`* ram_style = "block" *`). It has two ports:

- **Port A** – read‑only, connected to the compositor (`grid_rd_addr`, `grid_rd_data`).
- **Port B** – read/write, connected to the SPI receiver and the hardware state machine.

Port B is multiplexed between the SPI write path and the hardware override state machine. The top‑level `always` block selects the source:

- **Normal passthrough** – SPI writes are enabled when `spi_raw_toggle` is high and `spi_rx_addr < 5600`.
- **Auto‑clear** – the state machine writes `0` to every address.
- **Auto‑fill** – the state machine writes the winner colour ID to every address.

The RAM is initialised from `mainframe_init.hex` at power‑up, providing a boot splash screen or default game layout.

---

## 5. Hardware Override State Machine

The top‑level contains a small state machine that reacts to the `clear_flag` and `fill_flag` bits from the SPI receiver:

```verilog
localparam STATE_PASSTHROUGH = 2'b00;
localparam STATE_AUTO_CLEAR  = 2'b01;
localparam STATE_AUTO_FILL   = 2'b10;
```

When `clear_flag` is asserted, the machine enters `AUTO_CLEAR` and walks through all 5600 addresses, writing `0` to each cell. When `fill_flag` is asserted, it walks through the addresses writing the winner colour ID. After completing the loop (counter reaches 5599), the machine returns to `PASSTHROUGH`.

This hardware acceleration means the ESP32 does not need to stream 5600 zeros or ones; it merely sends a single command byte.

---

## 6. LCD Timing Controller – `lcd_timing`

This module generates the standard video timing signals for an 800×480 display with a 33 MHz pixel clock. It provides:

- **`hsync`** and **`vsync`** – sync pulses for the LCD.
- **`video_de`** – data enable (active during visible area).
- **`pixel_x`** (10 bits) – current horizontal pixel coordinate (0‑799).
- **`pixel_y`** (10 bits) – current vertical pixel coordinate (0‑479).

The timing parameters are configured for 60 Hz refresh (porches, sync pulse widths, etc.). The module also outputs a `frame_done` pulse (unused in this design).

---

## 7. Display Compositor – `display_compositor`

This is the core pixel‑rendering engine. It runs every pixel clock and outputs 24‑bit RGB (5‑6‑5) to the LCD. The compositor operates in two layers:

### Layer 1: Arena (Y < 448)
- Reads the cell colour from the RAM at the address corresponding to the current pixel.
- If the cell is a **wall (ID=5)**, it renders a 8×8 brick pattern.
- If it is a **team cell (ID=1‑4)**, it renders a 8×8 sprite with three colour zones (outline, accent, core).
- If it is **empty (ID=0)**, it draws a subtle checkerboard background.

### Layer 2: HUD (Y between 456 and 471)
- Renders text characters (P1, P2, etc.) using a 7‑row × 8‑column font ROM.
- Draws progress bars for each player, with the bar length proportional to the status value.
- In **GENETIC_TAKEOVER** mode, the bar colour changes to the invader’s colour if an invader is present.
- The timer (minutes:seconds) is rendered at the centre of the HUD.

The font ROM is implemented as a case statement inside the compositor; it includes digits 0‑9, letters P, E, F, D, colon, brackets, percent, and a solid block.

---

## 8. Pin Constraints – `top.cst`

The physical constraints file maps the top‑level ports to the FPGA pins. Key assignments include:

- SPI pins: CS (Pin 24), SCLK (Pin 22), MOSI (Pin 23).
- LCD pins: PCLK (Pin 11), HSYNC (Pin 10), VSYNC (Pin 46), DE (Pin 5), RGB buses.
- Clock and reset: 24 MHz input (Pin 35), reset button (Pin 14).

All I/O are set to 3.3 V and use appropriate drive strengths.

---

## 9. Data Flow Summary

1. The ESP32 constructs a complete packet (5612 bytes) in its internal buffer.
2. It asserts SPI CS and clocks out the bytes at 20 MHz.
3. The FPGA SPI receiver shifts in the data; for bytes 2‑5601, it writes the lower 4 bits into the RAM via Port B.
4. For bytes 5602‑5610, it stores them in internal buffers.
5. When footer `0xBB` arrives, the receiver updates the live HUD registers with the buffered values.
6. The hardware state machine checks the clear/fill flags; if set, it initiates a rapid RAM overwrite loop (overriding any SPI writes that may still be in progress – the SPI transaction is complete before the state machine starts, thanks to CS deassertion).
7. The LCD timing controller continuously generates pixel coordinates.
8. For each visible pixel, the compositor reads the RAM (Port A) at the appropriate address and combines it with the HUD data to produce the final RGB value.
9. The RGB values are driven to the LCD pins at 33 MHz, refreshing the screen 60 times per second.

---

## 10. Memory Initialisation File – `mainframe_init.hex`

This text file contains 5600 hexadecimal digits (one per cell). At power‑up, the RAM is loaded with these values. The default file can contain:

- A splash screen logo.
- A QR Code.
- An empty grid with walls and some pre‑placed patterns for testing.

Currenlty it is set to empty grid.

The format is simple: each line can contain any number of hex digits; the total must be 5600. The addresses are filled in order (row‑major). The FPGA synthesises the file into the BRAM initialisation.

---

## 11. FPGA Implementation Notes

- **Synthesis target:** Gowin GW1N‑1 (or compatible) with 48‑pin QFN package.
- **Resource utilisation:** The design fits comfortably in the GW1N‑1 with 1K LUTs, using the dedicated block RAM for the 5600×4 grid.
- **Timing:** The 33 MHz clock is easily met; the SPI receiver runs at 20 MHz, leaving plenty of slack.
- **Power:** The FPGA core is supplied at 1.2 V (internal regulator on Tang Nano); I/O banks at 3.3 V.

For more details, refer to the Gowin EDA project file (`project.gpr`) and the provided Verilog source files.
---