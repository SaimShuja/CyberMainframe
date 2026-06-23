# Cyber‑Mainframe – Multiplayer Cellular‑Automaton Game

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![PlatformIO](https://img.shields.io/badge/platformio-ESP32-orange)](https://platformio.org/)
[![Gowin](https://img.shields.io/badge/FPGA-Gowin-brightgreen)](https://www.gowinsemi.com/)

**Cyber‑Mainframe** is a tactical real‑time multiplayer game where 2‑4 players compete on a 100×56 cell grid. Deploy patterns (gliders, blocks, blasters) to expand your territory and conquer opponents. The game runs on an **ESP32** (WiFi AP + WebSocket server) and a **Gowin FPGA** driving a high‑speed LCD display.

## ✨ Features
- 🎮 **Three Game Modes** – Core Meltdown (HP), Genetic Takeover (DNA purity), Critical Mass (dominance).
- 🌐 **Web‑Based Lobby** – Players join via browser, pick teams, and start matches.
- 🔌 **Real‑time SPI Streaming** – 5600‑cell grid updated every 150 ms via SPI.
- ⚡ **Hardware Accelerated** – FPGA handles auto‑clear/fill and pixel rendering.
- 🖥️ **Rich HUD** – Status bars, timer, invader indicators rendered on‑screen.

## 🧰 Hardware Requirements
- ESP32 development board (e.g., NodeMCU‑32S)
- Gowin FPGA board (e.g., Tang Nano 9K or similar)
- 800×480 LCD panel with 24‑bit RGB interface
- 24 MHz oscillator, power supply, cables

## 🔌 Pin Connections
See [hardware_setup.md](docs/hardware_setup.md) for full pin mapping.

## 🚀 Quick Start
1. **ESP32 Firmware** – open the PlatformIO project in `firmware/esp32/` and upload.
2. **FPGA Bitstream** – use Gowin EDA to synthesise `firmware/fpga/project.gpr`.
3. **Power up** – the ESP32 creates a WiFi AP `CyberMainframe_AP`.
4. **Connect** – any device with a browser to `192.168.4.1` and join the game.
5. **Play** – assign teams, start match, deploy patterns by clicking on the grid.

## 📁 Repository Structure
- `firmware/esp32/` – ESP32 Arduino/PlatformIO source.
- `firmware/fpga/` – Verilog RTL and constraints for the Gowin FPGA.
- `web/` – static HTML/CSS/JS for the game lobby and remote control.
- `docs/` – detailed hardware, SPI protocol, and FPGA architecture guides.

## 📖 Documentation
- [SPI Protocol Definition](docs/spi_protocol.md)
- [FPGA Architecture Overview](docs/fpga_architecture.md)
- [Web Interface Guide](docs/web_interface.md)

## 🤝 Contributing
Contributions are welcome! Please read our [Contributing Guidelines](CONTRIBUTING.md) and open an issue or pull request.

## 📄 License
Distributed under the MIT License. See [LICENSE](LICENSE) for more information.
