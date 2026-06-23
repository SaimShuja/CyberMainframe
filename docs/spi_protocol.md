# SPI Protocol Definition – CyberMainframe

This document specifies the exact byte‑level format of the SPI packet streamed from the ESP32 to the FPGA. The packet is **5612 bytes** long and is transmitted every 150 ms during active gameplay.

The packet is divided into four phases:

1. **Header** (2 bytes) – synchronisation and frame counter.
2. **Playfield** (5600 bytes) – the 100×56 cell grid.
3. **HUD Configuration** (9 bytes) – game mode, player statuses, timer, invader IDs, and hardware override commands.
4. **Footer** (1 byte) – validation magic number.

---

## Full Packet Layout

| **Byte Index** | **Phase** | **ESP32 Source Field** | **Bit Mapping** | **FPGA Target** | **Description** |
|:---:|:---:|:---|:---|:---|:---|
| 0 | 1: Header | `0xAA` (static) | [7:0] | SPI State Alignment Engine | Resets internal memory pointers; locks frame synchronisation. |
| 1 | 1: Header | `frameCounter` | [7:0] | Telemetry Diagnostics Register | Tracks dropped frames or stream discontinuities (forced to 0x00 on clear). |
| 2 – 5601 | 2: Playfield | `gameGrid[0]` … `gameGrid[5599]` | [7:0] per byte | Dual‑Port Cell Block RAM | Cell colour data for the 100×56 arena. Each byte’s lower 4 bits are the colour ID (0=dead, 1‑4=teams). (Sends 5600 zeros during clear.) |
| 5602 | 3: HUD | `gameModeReg` with flags | [1:0] = `game_mode[1:0]`<br>[6] = Fill Flag<br>[7] = Clear Flag | `game_mode[1:0]`<br>`hw_fill_strobe`<br>`hw_clear_strobe` | **Core Command Strobe**<br>• Bit 7 = 1 → triggers hardware fast‑wipe (clear).<br>• Bit 6 = 1 → triggers post‑match screen fill loop. |
| 5603 | 3: HUD | Player 1 Status | [6:0] (0‑100) | `p1_status[6:0]` | Progress bar for Player 1 (HP, DNA%, or Dominance depending on mode). |
| 5604 | 3: HUD | Player 2 Status | [6:0] (0‑100) | `p2_status[6:0]` | Progress bar for Player 2. |
| 5605 | 3: HUD | Player 3 Status | [6:0] (0‑100) | `p3_status[6:0]` | Progress bar for Player 3. |
| 5606 | 3: HUD | Player 4 Status | [6:0] (0‑100) | `p4_status[6:0]` | Progress bar for Player 4. |
| 5607 | 3: HUD | Time Minutes | [5:0] (0‑59) | `hud_time_mins[5:0]` | Rendered on the HUD timer. |
| 5608 | 3: HUD | Time Seconds | [5:0] (0‑59) | `hud_time_secs[5:0]` | Rendered on the HUD timer. |
| 5609 | 3: HUD | `invaderByte1` | **Live match:**<br>• [3:0] = P1 Invader ID<br>• [7:4] = P2 Invader ID<br>**Game Over fill:**<br>• [5:3] = Winner Color ID | `hud_invader_composite[7:0]`<br>`game_over_winner_id[2:0]` | **Dual‑Role Register**<br>• Standard mode: drives active player encroachment colours (invader indicators).<br>• Fill mode: injects the winner’s palette index to direct the hardware colour‑fill engine. |
| 5610 | 3: HUD | `invaderByte2` | [3:0] = P3 Invader ID<br>[7:4] = P4 Invader ID | `hud_invader_composite[15:8]` | Carries upper player invader IDs during live match; acts as dummy padding during game‑over fill. |
| 5611 | 4: Footer | `0xBB` (static) | [7:0] | Pipeline Strobe Gatekeeper | Validates packet integrity; commits all modified configurations to active pipeline registers on CS rise. |

---

## Detailed Field Descriptions

### Header (Bytes 0‑1)
- **Byte 0 (0xAA):** Synchronisation magic. The FPGA waits for this byte to reset its internal state machine.
- **Byte 1 (frameCounter):** Incremented by the ESP32 each frame. The FPGA can use this to detect dropped packets.

### Playfield (Bytes 2‑5601)
- The 100×56 grid is stored row‑major (row 0, columns 0‑99; row 1, etc.).
- Each byte’s lower 4 bits are used; the upper 4 bits are ignored.
- Valid colour IDs:  
  `0` = empty, `1` = Red, `2` = Blue, `3` = Green, `4` = Yellow, `5` = Wall (the FPGA also recognises 5 for walls).

### HUD Configuration (Bytes 5602‑5610)

#### Byte 5602 – Game Mode + Hardware Commands
| Bit(s) | Name | Function |
|:---:|:---|:---|
| [1:0] | `game_mode` | 0 = CORE_MELTDOWN, 1 = GENETIC_TAKEOVER, 2 = CRITICAL_MASS |
| [6] | `fill_flag` | When 1, triggers the FPGA’s hardware fill loop (used at game over). |
| [7] | `clear_flag` | When 1, triggers the FPGA’s hardware clear loop (used at match start). |

**Important:** Only one of `fill_flag` or `clear_flag` should be set in a single packet. If both are 0, the FPGA stays in normal passthrough mode (i.e., the playfield data is written to RAM as usual).

#### Bytes 5603‑5606 – Player Statuses
- 7‑bit values (0‑100) displayed as progress bars.
- The interpretation depends on `game_mode`:
  - CORE_MELTDOWN: HP (health points)
  - GENETIC_TAKEOVER: DNA purity percentage
  - CRITICAL_MASS: dominance percentage

#### Bytes 5607‑5608 – Timer
- 6‑bit values (0‑59) for minutes and seconds.
- The HUD character generator renders these as two‑digit numbers.

#### Bytes 5609‑5610 – Invader Composite (16 bits)
- During normal play, these two bytes pack the invader IDs for all four players (4 bits each).  
  The invader ID is the colour of the dominant invading team near that player’s core.  
  If no invader is present, the nibble is 0.
- During the game‑over fill packet (when `fill_flag` is set), byte 5609 **re‑purposes** its bits [5:3] to carry the winner’s colour ID (1‑4).  
  The FPGA uses this ID to fill the entire screen with the winning team’s colour.

### Footer (Byte 5611)
- Must be `0xBB` to validate the packet.
- The FPGA only updates its HUD registers and commits the playfield data when this footer is correctly received.

---

## Packet Timing & Constraints

- **Frequency:** One complete packet is sent every 150 ms (approx. 6.67 Hz).
- **SPI clock:** 20 MHz.
- **Packet size:** 5612 bytes → transmission takes ~2.2 ms at 20 MHz (ignoring CS overhead).

The ESP32 must ensure that the `gameGrid` and HUD values are fully updated before initiating the SPI transaction.

---

## Hardware Override Examples

### Match Start Clear Packet
- Byte 5602: `game_mode` bits [1:0] set to the chosen mode, bit 7 = 1, bit 6 = 0.
- Playfield bytes (2‑5601) are all zero.
- HUD bytes (5603‑5610) are zero (or contain initial values).
- Footer `0xBB`.

### Game Over Fill Packet
- Byte 5602: `game_mode` bits [1:0] set to current mode, bit 6 = 1, bit 7 = 0.
- Playfield bytes contain the final game state (optional – the FPGA will fill over it).
- Bytes 5603‑5608 contain final player stats and zero timer.
- Byte 5609: winner colour ID placed in bits [5:3]; other bits are ignored.
- Byte 5610: dummy (0x00).
- Footer `0xBB`.

---

## FPGA Implementation Notes

The SPI receiver (`spi_grid_rx.v`) latches all HUD registers **only** after a valid footer is received. This prevents partial updates from being displayed. The hardware state machine in `top.v` detects the `clear_flag` and `fill_flag` and initiates a fast loop that writes the entire RAM without needing to receive 5600 bytes of data.

The display compositor (`display_compositor.v`) reads the HUD registers each frame to render the status bars, timer, and invader indicators.