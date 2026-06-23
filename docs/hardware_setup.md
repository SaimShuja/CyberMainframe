# Hardware Setup Guide – CyberMainframe

This document describes the physical connections required to build the **CyberMainframe** system. The design uses an **ESP32** (WiFi/WebSocket server) and a **Gowin FPGA** (video rendering engine) communicating via SPI, with the FPGA driving an 800×480 RGB LCD.

---

## 1. Required Hardware

| Component | Recommended Model | Notes |
|-----------|-------------------|-------|
| **ESP32** | ESP32 D32 Pro (or any ESP32 with SPI pins) | Handles WiFi, WebSocket, game logic |
| **FPGA**  | Gowin Tang Nano GW1N‑1 (or Tang Nano 9K / 4K) | Drives LCD, renders game grid |
| **LCD**   | 800×480 RGB 24‑bit parallel (e.g., 4.3‑inch) | Requires HSYNC, VSYNC, DE, and RGB buses |
| **Oscillator** | 24 MHz active crystal oscillator | Feeds FPGA’s `ext_clk_24m` pin |
| **Power** | 5 V DC adapter (for ESP32) | FPGA can be powered via USB‑C on Tang Nano |
| **Wires** | Jumper wires (female‑to‑female) | For SPI and LCD connections |

> **Note:** The Tang Nano GW1N‑1 comes with an onboard 24 MHz oscillator and USB‑C power. If using a different FPGA board, ensure it has a 24 MHz clock source and compatible I/O voltages (3.3 V).

---

## 2. SPI Connection – ESP32 ↔ FPGA

The ESP32 and FPGA communicate over a 4‑wire SPI bus. The ESP32 acts as the **master**, the FPGA as the **slave**.

| **Signal** | **ESP32 Pin** | **FPGA Physical Pin** | **FPGA Net** | **Purpose** |
|------------|---------------|------------------------|--------------|-------------|
| `esp_spi_cs` | GPIO 15 (HSPI CS) | Pin 24 | IOB16B | Hardware Chip Select (active‑low) |
| `esp_sclk`   | GPIO 14 (HSPI SCLK) | Pin 22 | IOB14B | SPI Clock (20 MHz) |
| `esp_spi_mosi`| GPIO 13 (HSPI MOSI) | Pin 23 | IOB16A | Master Out, Slave In (data) |
| `GND`        | Any GND pin       | Any GND pin | –       | Common ground reference |

> **Important:** Both devices operate at **3.3 V logic** – direct connection is safe. If your FPGA or ESP32 uses 5 V, you must use level shifters.

### Alternative ESP32 Pins

If you are not using an ESP32 D32 Pro, you can map the SPI signals to any other ESP32 pins. The firmware (`main.cpp`) defines:

```cpp
const int HSPI_CS  = 15;   // Chip Select
const int HSPI_CLK = 14;   // Clock
const int HSPI_MOSI = 13;  // MOSI
```

Adjust these values in the firmware to match your physical wiring.

---

## 3. LCD Interface – FPGA to Panel

The FPGA’s top‑level module (`top.v`) exports the LCD signals. On the Tang Nano, these are **already routed to the onboard FPC connector** – simply plug in your LCD’s FPC cable. No manual wiring is required.

For reference, the FPGA pin mapping is as follows (these are defined in the constraint file `cyber_mainframe.cst`):

| **LCD Signal** | **FPGA Pin** | **FPGA Net** | **Purpose** |
|----------------|--------------|--------------|-------------|
| Pixel Clock    | Pin 11       | `lcd_pixel_clk` | 33 MHz pixel clock |
| HSYNC          | Pin 10       | `lcd_hsync`     | Horizontal sync |
| VSYNC          | Pin 46       | `lcd_vsync`     | Vertical sync |
| DE (Data Enable) | Pin 5     | `lcd_video_de`  | Data enable |
| Red [4:0]      | Pins 31–27   | `lcd_r[4:0]`    | 5‑bit red |
| Green [5:0]    | Pins 40–32   | `lcd_g[5:0]`    | 6‑bit green |
| Blue [4:0]     | Pins 45–41   | `lcd_b[4:0]`    | 5‑bit blue |

> **Note:** All these pins are **3.3 V** outputs. If your LCD requires 5 V logic, use a level shifter – though most FPC‑based LCDs are 3.3 V compatible.

---

## 4. Power & Additional Connections

| **Signal** | **FPGA Pin** | **Purpose** |
|------------|--------------|-------------|
| `ext_clk_24m` | Pin 35 | Input from 24 MHz oscillator (or onboard) |
| `ext_rst_n`   | Pin 14 | Active‑low reset button (pull‑up to 3.3 V) |
| `psram_cs_n`  | Unused (tied HIGH internally) | PSRAM chip‑select – forced inactive |
| **GND**       | Multiple pins | All grounds must be common with ESP32 |

### Power Sequence
1. Connect the ESP32 to USB (5 V) – it powers itself.
2. Power the FPGA via its USB‑C port (or 5 V input).
3. Ensure the LCD receives its required supply (often 3.3 V or 5 V – check datasheet).

---

## 5. Block Diagram

```text
                  ESP32 D32 Pro
                 ┌─────────────────┐
                 │  WiFi AP        │
                 │  WebSocket Srv  │
                 │  Game Engine    │
                 │                 │
                 │  SPI Master     │
                 └───────┬─────────┘
                         │ CS, SCLK, MOSI, GND
                         ▼
                   Tang Nano GW1N-1
                 ┌─────────────────┐
                 │  SPI Slave Rx   │
                 │  Frame Buffer   │
                 │  Compositor     │
                 │  LCD Timing     │
                 └───────┬─────────┘
                         │ RGB, HSYNC, VSYNC, DE, PCLK
                         ▼
                    800×480 LCD
```

---

## 6. Tuning and Testing

- **Check SPI:** Use a logic analyser to verify that CS, SCLK, and MOSI are active after the ESP32 boots.
- **FPGA Boot:** The Tang Nano LED should indicate programming success (refer to its manual).
- **LCD Display:** After programming the FPGA, you should see the boot splash screen (defined in `mainframe_init.hex`) on the LCD.

---

## 7. Compatibility Notes

- This design works with **any ESP32** that supports HSPI (most do, normal SPI works as well). Adjust GPIO numbers in `main.cpp` accordingly.
- The FPGA code is written for **Gowin** devices (GW1N series). It can be ported to other FPGAs (e.g., Lattice, Xilinx) with minor changes to the PLL and pin constraints.
- The LCD timing is configured for **800×480 at 33 MHz** (60 Hz refresh). If using a different resolution, update `lcd_timing.v`.
---

## 8. Next Steps

After wiring:

1. Program the FPGA with the generated bitstream.
2. Upload the ESP32 firmware via PlatformIO or Arduino IDE.
3. Power up and join the WiFi AP (`CyberMainframe_AP`).
4. Open a browser at `192.168.4.1` and enjoy the game!

For detailed pin mapping and constraints, refer to the FPGA constraint file `top.cst` in the `firmware/fpga/src/` folder.