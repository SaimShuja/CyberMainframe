# Web Interface – CyberMainframe

The CyberMainframe web interface is a **single‑page application** served by the ESP32 at the root URL (`http://192.168.4.1/`). It provides a real‑time multiplayer lobby, team selection, game start controls, and live match status via **WebSockets**. The interface is fully responsive and works on any modern browser (desktop, tablet, or phone).

All HTML, CSS, and JavaScript are embedded in the ESP32 firmware as a header file (`index_html.h`), optimised for minimal footprint.

---

## Overview

The web interface communicates with the ESP32 using a **WebSocket** connection (`ws://192.168.4.1/ws`). All game state updates (lobby changes, game ticks, match end) are sent as **JSON messages** over the WebSocket. The client sends actions (e.g., join lobby, select colour, start match, deploy pattern) via JSON commands or binary messages.

**Key features:**
- **Lobby** – players join, see who is online, select team colours.
- **Host controls** – choose game mode, start the match, return to lobby.
- **Live game view** – shows player stats (HP, DNA%, Dominance), energy, invader indicators.
- **Pattern deployment** – click on the displayed grid to deploy a pattern (glider, block, blinker, gun).

---

## WebSocket Protocol

### Connection Lifecycle
1. **Client connects** – the ESP32 assigns a unique client ID and sends a `IAM` message.
2. **Join lobby** – client sends `{"action":"join_lobby"}` to become a player.
3. **Lobby updates** – the server broadcasts `LOBBY_UPDATE` messages to all clients.
4. **Match start** – the host sends `{"action":"start_match"}`; server broadcasts `START_COUNTDOWN`.
5. **Game active** – server sends `GAME_TICK` messages every 150 ms with player stats.
6. **Game over** – server broadcasts `GAME_OVER` with the winner ID.
7. **Return to lobby** – host sends `{"action":"return_to_lobby"}`; server resets state.

### Message Format
All messages are **JSON strings** except for pattern deployment, which uses **binary WebSocket frames** (3 bytes: seedId, x, y).

---

## JSON Message Reference

### Client → Server (Actions)

| **Action** | **Payload** | **Description** |
|------------|-------------|-----------------|
| `join_lobby` | `{}` | Registers the client as a player. Automatically assigns a slot. |
| `select_mode` | `{"mode": 0|1|2}` | Host selects game mode: `0`=Core Meltdown, `1`=Genetic Takeover, `2`=Critical Mass. |
| `select_color` | `{"color": 0|1|2|3|4}` | Player selects a team colour (`0`=None, `1`=Red, `2`=Blue, `3`=Green, `4`=Yellow). |
| `start_match` | `{}` | Host starts the countdown (requires ≥2 players with colours). |
| `return_to_lobby` | `{}` | Host ends the match and returns to the lobby. |

### Server → Client (Events)

| **Status** | **Payload Example** | **Description** |
|------------|---------------------|-----------------|
| `IAM` | `{"status":"IAM","id":123456}` | Sent on connection; provides the client’s WebSocket ID. |
| `LOBBY_UPDATE` | `{"status":"LOBBY_UPDATE","currentMode":0,"hostId":123456,"terminals":[{"id":123456,"color":1}, ...]}` | Broadcast to all clients when the lobby changes. |
| `LOBBY_ERROR` | `{"status":"LOBBY_ERROR","message":"Cannot execute deployment: Minimum 2 players must pick teams."}` | Sent when host tries to start with insufficient teams. |
| `START_COUNTDOWN` | `{"status":"START_COUNTDOWN","mode":1}` | Broadcast to all clients; the game will start after 3 seconds. |
| `LAUNCH_GAME` | `{"status":"LAUNCH_GAME"}` | Broadcast when the match actually begins (after countdown). |
| `GAME_TICK` | `{"status":"GAME_TICK","mode":1,"nodes":[{"id":123456,"color":1,"ep":45,"dead":false,"hp":87,"dna":94,"v":2,"eq":0}, ...]}` | Sent every 150 ms during the match. Each node contains: `id` (client ID), `color` (team), `ep` (energy), `dead` (bool), `hp` (health), `dna` (purity), `v` (invader ID), `eq` (dominance). |
| `GAME_OVER` | `{"status":"GAME_OVER","winner":2,"hostId":123456}` | Broadcast when the match ends. `winner` is the team colour ID (0 for draw). |

### Binary Pattern Deployment (Client → Server)
- **Frame format:** 3 bytes
  - **Byte 0:** `seedId` – 1=Glider, 2=Block, 3=Blinker, 4=Gun
  - **Byte 1:** `x` – X coordinate (0‑99)
  - **Byte 2:** `y` – Y coordinate (0‑55)

---

## UI Flow

### 1. Lobby Screen
- Displays a list of online players and their team colours.
- Colour selection buttons (Red, Blue, Green, Yellow) – only available if colour is not taken.
- Game mode selection (dropdown or buttons) – only visible to the host.
- **Start Match** button – host only; enabled when at least 2 players have chosen a colour.
- The host is indicated with a badge.

### 2. Game in Progress
- The main arena is displayed on the **FPGA‑driven LCD**; the web interface shows a **simplified grid** or a set of controls.
- Player statistics are updated in real time (bars for HP/DNA/Dominance, energy counter).
- Players can click on the grid to deploy patterns (if energy allows).
- A timer counts down from 45 seconds.

### 3. Game Over
- The winner is displayed prominently (colour name and colour swatch).
- A **Return to Lobby** button is shown (host only) to reset the session.

---

## Implementation Notes

- **Embedded HTML:** The entire HTML page is compressed into a single `index_html.h` file. It includes inline CSS and JavaScript.
- **WebSocket Library:** The ESP32 uses `AsyncWebSocket` from the `ESPAsyncWebServer` library.
- **Canvas Grid:** The JavaScript creates a canvas element that mirrors the 100×56 grid. Clicking on the canvas sends a binary WebSocket message with the pattern and coordinates.
- **Responsive:** The UI uses CSS flexbox and scales well on different screen sizes.
- **Reconnection:** If the WebSocket disconnects, the client automatically attempts to reconnect and re‑joins the lobby.

---

## Development & Customisation

The web interface is statically served; changes to the HTML/CSS/JS require regenerating the `index_html.h` file and re‑uploading the firmware.

To modify the interface:

1. **Edit the HTML file** in `web/index.html` – this is the single page served to all clients.
2. **Regenerate the header** using the provided Python script:
   ```bash
   cd tools
   python html_to_header.py
   ```
   The script reads `web/index.html` and writes `firmware/esp32/include/index_html.h` with a C++ raw string literal. It automatically handles escaping and includes the necessary Arduino directives (`#include <Arduino.h>` and `PROGMEM`).
3. **Rebuild the ESP32 firmware** – upload the new binary to your ESP32. The updated web interface will be served immediately after the device reboots.

### Important Notes
- The Python script is located in `tools/html_to_header.py`. Ensure it has execute permissions if needed.
- The script uses **relative paths** from the repository root, so you can run it from anywhere as long as you are inside the repository.
- After editing the HTML, always regenerate the header **before** building the firmware – otherwise changes will not be included.
- If you add external assets (CSS, JS), embed them inline in `index.html` to keep a single file. The ESP32 serves only this page; no additional files are hosted.

### Troubleshooting
- If the script fails, verify that `web/index.html` exists and that the output directory (`firmware/esp32/include/`) is writable.
- The generated `index_html.h` uses a **raw string literal** (`R"=====(...)====="`). If your compiler does not support C++11, switch to a traditional escaped string by modifying the `generate_header()` function in the script.

---

## Example WebSocket Exchange

**Client connects:**
```json
{"status":"IAM","id":17892345}
```

**Client joins lobby:**
```json
{"action":"join_lobby"}
```

**Server broadcasts lobby update:**
```json
{"status":"LOBBY_UPDATE","currentMode":0,"hostId":17892345,"terminals":[{"id":17892345,"color":0}]}
```

**Client selects Red:**
```json
{"action":"select_color","color":1}
```

**Host starts match:**
```json
{"action":"start_match"}
```

**Server begins countdown:**
```json
{"status":"START_COUNTDOWN","mode":1}
```

**During match, server ticks:**
```json
{"status":"GAME_TICK","mode":1,"nodes":[{"id":17892345,"color":1,"ep":38,"dead":false,"hp":72,"dna":88,"v":2,"eq":0},{"id":..."}]}
```

**Game over:**
```json
{"status":"GAME_OVER","winner":1,"hostId":17892345}
```

---

## Security & Limitations

- No authentication – any device on the same network can join.
- Maximum 4 players due to hardware constraints.
- The web interface assumes a reliable network; packet loss may cause temporary desync (but the game engine recovers on the next tick).
- Pattern deployment is rate‑limited by the energy system.

---
For more details on the communication protocol, refer to the [SPI Protocol](spi_protocol.md) and the ESP32 source code.