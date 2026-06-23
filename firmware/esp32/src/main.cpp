/**
 * @file main.cpp
 * @brief Tactical Cyber-Mainframe Core System – ESP32 firmware for a multiplayer 
 *        cellular automaton game with FPGA-driven display.
 * 
 * This firmware implements a WiFi access point, WebSocket server, and SPI 
 * communication with an FPGA to render a 100x56 pixel game grid. Players join 
 * via a web interface, select teams, and deploy patterns in a real-time 
 * simulation (Game of Life variant). Three game modes alter victory conditions.
 * 
 * @author  Saim Shujah
 * @date    2026-06-23
 * @version 1.0
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <index_html.h>
#include <SPI.h>

// ============================================================================
// HARDWARE PIN CONFIGURATIONS
// ============================================================================

const int HARDWARE_LED = 5;       ///< On‑board LED pin
const int LED_ON  = LOW;          ///< Active-low LED logic
const int LED_OFF = HIGH;

// SPI pins for FPGA communication
#define HSPI_CS   15
#define HSPI_CLK  14
#define HSPI_MISO 12
#define HSPI_MOSI 13

SPIClass hspi(HSPI);              ///< HSPI bus instance

// ============================================================================
// GAME PLAYFIELD DIMENSIONS
// ============================================================================

const int GRID_WIDTH  = 100;      ///< Number of columns
const int GRID_HEIGHT = 56;       ///< Number of rows
const int MAP_SIZE    = GRID_WIDTH * GRID_HEIGHT;   ///< Total cells

uint8_t gameGrid[MAP_SIZE];       ///< Current cell states (0 = empty, 1‑4 = team colors)
uint8_t frameCounter = 0;         ///< Incremented each SPI frame

// ============================================================================
// NETWORK CONFIGURATIONS
// ============================================================================

const char *ssid = "CyberMainframe_AP";   ///< AP SSID
const byte DNS_PORT = 53;                 ///< DNS port for captive portal

// ============================================================================
// ENUMS AND DATA STRUCTURES
// ============================================================================

/** Current session state */
enum SessionState {
  LOBBY,          ///< Waiting for players to join and select teams
  COUNTDOWN,      ///< Countdown before match starts
  GAME_ACTIVE,    ///< Match in progress
  GAME_OVER       ///< Match ended
};

/** Game mode affects scoring and simulation rules */
enum GameMode {
  CORE_MELTDOWN,    ///< Protect your core; enemy cells near it drain HP
  GENETIC_TAKEOVER, ///< Maintain DNA purity; invaders reduce purity
  CRITICAL_MASS     ///< Dominate by having largest cell population
};

/** Team colors (also used as cell values) */
enum TeamColor {
  NONE   = 0,
  RED    = 1,
  BLUE   = 2,
  GREEN  = 3,
  YELLOW = 4
};

/** 2D coordinate on the grid */
struct Coordinate {
  int x;
  int y;
};

// Removed 'const' to allow runtime anchor reassignment based on active player counts
Coordinate CORE_ANCHORS[5] = {
  {0, 0},   // Index 0 (NONE)
  {5, 5},   // Index 1 (RED Base Default)
  {95, 5},  // Index 2 (BLUE Base Default)
  {5, 50},  // Index 3 (GREEN Base Default)
  {95, 50}  // Index 4 (YELLOW Base Default)
};

/** Player state */
struct Player {
  uint32_t  clientId;     ///< WebSocket client ID
  IPAddress ipAddress;    ///< Client IP address
  bool      isHost;       ///< True if player is the match host
  TeamColor color;        ///< Assigned team color
  bool      isOnline;     ///< Currently connected via WebSocket
  bool      isDead;       ///< Eliminated from the match
  int       energy;       ///< Energy to spend on pattern deployment
  int       hp;           ///< Health (CORE_MELTDOWN)
  int       dnaPct;       ///< DNA purity percentage (GENETIC_TAKEOVER)
  int       dnaInvader;   ///< Dominant invader color in core vicinity
  int       dominance;    ///< Population dominance percentage (CRITICAL_MASS)
};

// ============================================================================
// GLOBAL ENGINE REGULATORS
// ============================================================================

SessionState currentSession = LOBBY;      ///< Current game state
GameMode     selectedMode  = CORE_MELTDOWN;
TeamColor    winningTeam   = NONE;
uint32_t     hostClientId  = 0;

Player players[4];                        ///< Up to 4 players
int    playercount = 0;                   ///< Number of players with assigned color
bool   colorTaken[5] = {false, false, false, false, false};

// Clock cadence registries
unsigned long countdownStartTime = 0;
const unsigned long countdownLimit = 3000;    ///< 3‑second countdown
unsigned long lastEngineTick = 0;
const unsigned long tickPeriod = 150;         ///< Simulation tick every 150 ms
unsigned long lastEnergyPulse = 0;
const unsigned long energyPeriod = 1000;      ///< Energy regeneration every second
unsigned long matchStartTime = 0;
const unsigned long matchLimit = 45000;       ///< Match duration 45 seconds

AsyncWebServer    server(80);
AsyncWebSocket    ws("/ws");
DNSServer         dnsServer;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void processClientMessage(AsyncWebSocketClient *client, String msg);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len);
void triggerGameOver(TeamColor winner);
void broadcastLobbyState();
void broadcastEngineTick();

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Convert a TeamColor enum to a human‑readable string.
 */
const char* getTeamColorName(int c) {
  switch (c) {
    case NONE:   return "NEUTRAL/NONE";
    case RED:    return "RED_ROOTKIT";
    case BLUE:   return "BLUE_BLUEPRINT";
    case GREEN:  return "GREEN_GHOST";
    case YELLOW: return "YELLOW_AGENT";
    default:     return "INVALID_COLOR";
  }
}

/**
 * @brief Compute the starting spawn point for a player based on total players
 *        and the player's index.
 * 
 * @param totalPlayers  Number of active players (2‑4)
 * @param playerIndex   Index of this player (0‑3)
 * @return Coordinate    Spawn position on the grid
 */
Coordinate getPlayerSpawnPoint(int totalPlayers, int playerIndex) {
  Coordinate p = {5, 5};   // fallback (top-left)

  if (totalPlayers == 2) {
    if (playerIndex == 0) p = {5, 5};      // Top Left
    if (playerIndex == 1) p = {95, 50};    // Bottom Right
  }
  else if (totalPlayers == 3) {
    if (playerIndex == 0) p = {5, 5};      // Top Left
    if (playerIndex == 1) p = {95, 5};     // Top Right
    if (playerIndex == 2) p = {50, 50};    // Bottom Mid
  }
  else if (totalPlayers >= 4) {
    if (playerIndex == 0) p = {5, 5};      // Top Left
    if (playerIndex == 1) p = {95, 5};     // Top Right
    if (playerIndex == 2) p = {5, 50};     // Bottom Left
    if (playerIndex == 3) p = {95, 50};    // Bottom Right
  }

  return p;
}

// ============================================================================
// SPI COMMUNICATION WITH FPGA
// ============================================================================

/**
 * @brief Initialise the HSPI bus for FPGA communication.
 */
void initHardwareSPI() {
  Serial.printf("[SPI_INIT] Mapping HSPI Bus Lines -> CLK:%d, MISO:%d, MOSI:%d, CS:%d\n",
                HSPI_CLK, HSPI_MISO, HSPI_MOSI, HSPI_CS);
  pinMode(HSPI_CS, OUTPUT);
  digitalWrite(HSPI_CS, HIGH);
  hspi.begin(HSPI_CLK, HSPI_MISO, HSPI_MOSI, HSPI_CS);
  hspi.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
}

/**
 * @brief Stream the entire game state to the FPGA over SPI.
 * 
 * Packet structure per the FPGA interface specification:
 *   - Byte   0: Header Magic (0xAA)
 *   - Byte   1: frameCounter (telemetry)
 *   - Bytes 2–5601: Playfield (5600 bytes, gameGrid)
 *   - Byte 5602: HUD Config Byte 0 (mode + flags)
 *   - Byte 5603: Player 1 Status
 *   - Byte 5604: Player 2 Status
 *   - Byte 5605: Player 3 Status
 *   - Byte 5606: Player 4 Status
 *   - Byte 5607: Time Minutes
 *   - Byte 5608: Time Seconds
 *   - Byte 5609: invaderByte1 (or winner ID in fill mode)
 *   - Byte 5610: invaderByte2 (or dummy in fill mode)
 *   - Byte 5611: Footer Magic (0xBB)
 */
void streamGridToFPGA() {
  digitalWrite(HSPI_CS, LOW);

  // ---- Phase 1: Header (2 Bytes) ----
  // Byte 0: Static Magic Number (0xAA)
  //         FPGA Destination: SPI State Alignment Engine
  //         Function: Resets internal memory pointers; locks frame synchronization.
  hspi.transfer(0xAA);

  // Byte 1: frameCounter
  //         FPGA Destination: Telemetry Diagnostics Register
  //         Function: Tracks dropped frames or stream discontinuities.
  hspi.transfer(frameCounter);

  // ---- Phase 2: Playfield Payload (5600 Bytes) ----
  // Bytes 2..5601: gameGrid[0]..gameGrid[5599]
  //         FPGA Destination: Dual‑Port Cell Block RAM Matrix
  //         Function: Populates cell color data matrix for LAYER 1 (The Arena).
  hspi.transferBytes(gameGrid, NULL, MAP_SIZE);

  // ---- Phase 3: HUD Configuration (9 Bytes, indices 5602..5610) ----
  // Byte 5602: gameModeReg with flags
  //         Bit [1:0] : game_mode[1:0]
  //         Bit [6]   : Fill Flag Modifier (hw_fill_strobe)
  //         Bit [7]   : Clear Flag Modifier (hw_clear_strobe)
  //         FPGA Destination: Core System Engine Command Strobe
  //         Function: • Bit 7 = 1: triggers hardware fast wipe.
  //                   • Bit 6 = 1: triggers post‑match screen fill loop.
  uint8_t gameModeReg = static_cast<uint8_t>(selectedMode) & 0x03;
  hspi.transfer(gameModeReg);

  // Bytes 5603..5606: Player 1..4 Status
  //         Each byte: [6:0] Value (0–100)
  //         FPGA Destination: p1_status[6:0] .. p4_status[6:0]
  //         Function: Progress bar value (HP, DNA%, or Dominance) for each player.
  for (int i = 0; i < 4; i++) {
    uint8_t playerStatus = 0;
    if (selectedMode == CORE_MELTDOWN)
      playerStatus = players[i].hp;
    else if (selectedMode == GENETIC_TAKEOVER)
      playerStatus = players[i].dnaPct;
    else if (selectedMode == CRITICAL_MASS)
      playerStatus = players[i].dominance;
    hspi.transfer(playerStatus & 0x7F);
  }

  // Byte 5607: Time Minutes [5:0] (0–59)
  //         FPGA Destination: hud_time_mins[5:0]
  //         Function: Injected into HUD Character Generator to render match minutes.
  // Byte 5608: Time Seconds [5:0] (0–59)
  //         FPGA Destination: hud_time_secs[5:0]
  //         Function: Injected into HUD Character Generator to render match seconds.
  uint32_t elapsedSecs = (currentSession == GAME_ACTIVE) ? (millis() - matchStartTime) / 1000 : 0;
  uint32_t limitSecs   = matchLimit / 1000;
  uint32_t remainingSecs = (limitSecs > elapsedSecs) ? (limitSecs - elapsedSecs) : 0;
  hspi.transfer((remainingSecs / 60) & 0x3F);
  hspi.transfer((remainingSecs % 60) & 0x3F);

  // Byte 5609: invaderByte1
  //         Standard (live match): [3:0] P1 Invader ID, [7:4] P2 Invader ID
  //         FPGA Destination: hud_invader_composite[7:0] (or game_over_winner_id[2:0] in fill mode)
  //         Function: Drives active player encroachment colors during live match.
  // Byte 5610: invaderByte2
  //         Standard: [3:0] P3 Invader ID, [7:4] P4 Invader ID
  //         FPGA Destination: hud_invader_composite[15:8]
  //         Function: Carries upper player invader IDs during live match.
  uint8_t invaderByte1 = 0;
  uint8_t invaderByte2 = 0;
  for (int i = 0; i < 4; i++) {
    uint8_t inv = (players[i].color != NONE && players[i].dnaInvader != NONE) ? players[i].dnaInvader : 0;
    if (i < 2) {
      invaderByte1 |= (inv << (i * 4));   // P0 low nibble, P1 high nibble
    } else {
      invaderByte2 |= (inv << ((i - 2) * 4)); // P2 low nibble, P3 high nibble
    }
  }
  hspi.transfer(invaderByte1);
  hspi.transfer(invaderByte2);

  // ---- Phase 4: Footer Gatekeeper (1 Byte) ----
  // Byte 5611: Static Magic Number (0xBB)
  //         FPGA Destination: Pipeline Strobe Gatekeeper
  //         Function: Validates packet integrity; commits all modified configurations
  //                   to active pipeline registers on CS rise.
  hspi.transfer(0xBB);

  digitalWrite(HSPI_CS, HIGH);
  frameCounter++;
}

/**
 * @brief Send a special packet at match start to clear the FPGA frame buffer.
 * 
 * Sets Bit 7 of the mode register (Byte 5602) to instruct the FPGA to perform
 * a hardware‑accelerated wipe of its display RAM. All HUD fields are zeroed.
 */
void sendMatchStartClearPacket() {
  // Flush local memory array so game state starts dead fresh
  memset(gameGrid, 0, MAP_SIZE);

  digitalWrite(HSPI_CS, LOW);

  // ---- Header ----
  hspi.transfer(0xAA);               // Byte 0: Header Magic
  hspi.transfer(0x00);               // Byte 1: frameCounter forced to 0

  // ---- Playfield: 5600 zeros ----
  hspi.transferBytes(gameGrid, NULL, MAP_SIZE);  // Bytes 2..5601

  // ---- HUD Config ----
  // Byte 5602: modeReg with Clear Flag (Bit 7 = 1)
  uint8_t modeRegWithClearFlag = (static_cast<uint8_t>(selectedMode) & 0x03) | (1 << 7);
  hspi.transfer(modeRegWithClearFlag);

  // Bytes 5603..5608: Clear all player status and timer fields
  for (int i = 0; i < 6; i++) {
    hspi.transfer(0x00);
  }

  // Byte 5609 & 5610: Clear invader configurations
  hspi.transfer(0x00);               // Byte 5609
  hspi.transfer(0x00);               // Byte 5610

  // ---- Footer ----
  hspi.transfer(0xBB);               // Byte 5611: Footer Magic

  digitalWrite(HSPI_CS, HIGH);
  Serial.println("[SPI_TRANSITION] Dispatched Match Start CLEAR Flag to FPGA.");
}

/**
 * @brief Send a special packet at game over to fill the FPGA display with the
 *        winner's color pattern.
 * 
 * Sets Bit 6 of the mode register (Byte 5602) to instruct the FPGA to perform
 * a hardware fill loop. The winner's color ID is packed into Byte 5609
 * (bits [5:3]) as per the dual‑role specification.
 * 
 * @param winner  The winning team color (NONE if draw)
 */
void sendMatchEndFillPacket(TeamColor winner) {
  digitalWrite(HSPI_CS, LOW);

  // ---- Header ----
  hspi.transfer(0xAA);               // Byte 0: Header Magic
  hspi.transfer(frameCounter);       // Byte 1: current frame counter

  // ---- Playfield: current game matrix ----
  hspi.transferBytes(gameGrid, NULL, MAP_SIZE);  // Bytes 2..5601

  // ---- HUD Config ----
  // Byte 5602: modeReg with Fill Flag (Bit 6 = 1)
  uint8_t modeRegWithFillFlag = (static_cast<uint8_t>(selectedMode) & 0x03) | (1 << 6);
  hspi.transfer(modeRegWithFillFlag);

  // Bytes 5603..5608: Final stats or zero
  if (winner == NONE) {
    // Draw: fill with all zeros
    for (int i = 0; i < 6; i++) {
      hspi.transfer(0x00);
    }
  } else {
    // Send final player status values
    for (int i = 0; i < 4; i++) {
      uint8_t status = (selectedMode == CORE_MELTDOWN) ? players[i].hp :
                       (selectedMode == GENETIC_TAKEOVER) ? players[i].dnaPct :
                       players[i].dominance;
      hspi.transfer(status & 0x7F);   // Bytes 5603..5606
    }
    hspi.transfer(0x00);              // Byte 5607: Minutes = 0
    hspi.transfer(0x00);              // Byte 5608: Seconds = 0
  }

  // Byte 5609: compositeByte7 – inject Winner Color ID into bits [5:3]
  //         FPGA Destination: game_over_winner_id[2:0]
  //         Function: Directs the hardware color fill engine loop.
  uint8_t winnerColorIdx = static_cast<uint8_t>(winner) & 0x07;
  uint8_t compositeByte7 = (winnerColorIdx << 3);
  hspi.transfer(compositeByte7);

  // Byte 5610: Dummy padding byte to keep the total count at 13'd5608
  hspi.transfer(0x00);

  // ---- Footer ----
  hspi.transfer(0xBB);               // Byte 5611: Footer Magic

  digitalWrite(HSPI_CS, HIGH);
  Serial.printf("[SPI_TRANSITION] Dispatched Match End FILL Flag with Winner Color ID (%d) to FPGA.\n", winner);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n================================================================================");
  Serial.println("         [BOOT] INITIALIZING TACTICAL CYBER-MAINFRAME CORESYSTEM...");
  Serial.println("================================================================================");

  pinMode(HARDWARE_LED, OUTPUT);
  digitalWrite(HARDWARE_LED, LED_OFF);
  initHardwareSPI();

  // Initialise player slots
  for (int i = 0; i < 4; i++) {
    players[i].clientId  = 0;
    players[i].ipAddress = IPAddress(0, 0, 0, 0);
    players[i].isOnline  = false;
    players[i].isDead    = false;
    players[i].energy    = 0;
  }

  // Setup WiFi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);
  delay(500);

  IPAddress apIP = WiFi.softAPIP();
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  // WebSocket and HTTP server
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", INDEX_HTML);
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("http://" + WiFi.softAPIP().toString() + "/");
  });

  server.begin();
  Serial.println("[SERVER_INIT] System control engine fully ready.");
}

// ============================================================================
// PATTERN DEPLOYMENT
// ============================================================================

/**
 * @brief Spawn a predefined macro pattern (e.g., glider, block) on the grid.
 * 
 * @param seedId   Pattern identifier (1‑4)
 * @param startX   Top‑left X coordinate
 * @param startY   Top‑left Y coordinate
 * @param color    Team color to paint the cells
 */
void spawnMacroPattern(int seedId, int startX, int startY, uint8_t color) {
  auto writeCell = [&](int dx, int dy) {
    int targetX = startX + dx;
    int targetY = startY + dy;
    if (targetX >= 0 && targetX < GRID_WIDTH && targetY >= 0 && targetY < GRID_HEIGHT) {
      gameGrid[targetY * GRID_WIDTH + targetX] = color;
    }
  };

  switch (seedId) {
    case 1:   // GLIDER
      writeCell(1, 0); writeCell(2, 1); writeCell(0, 2); writeCell(1, 2); writeCell(2, 2);
      break;
    case 2:   // BLOCK
      writeCell(0, 0); writeCell(1, 0); writeCell(0, 1); writeCell(1, 1);
      break;
    case 3:   // BLINKER
      writeCell(0, 0); writeCell(0, 1); writeCell(0, 2);
      break;
    case 4:   // COMPACT GLIDER GUN
      writeCell(0, 1); writeCell(1, 1);
      writeCell(0, 2); writeCell(1, 2);
      writeCell(10, 1); writeCell(10, 2); writeCell(10, 3);
      writeCell(11, 0); writeCell(11, 4);
      writeCell(12, 0); writeCell(12, 4);
      writeCell(13, 2);
      break;
    default:
      break;
  }
}

/**
 * @brief Handle a binary WebSocket message containing a pattern deployment command.
 * 
 * The message is 3 bytes: seedId, x, y.
 * 
 * @param clientId  ID of the deploying client
 * @param seedId    Pattern type
 * @param x         X coordinate (0‑99)
 * @param y         Y coordinate (0‑55)
 */
void handleBinaryInput(uint32_t clientId, uint8_t seedId, uint8_t x, uint8_t y) {
  if (currentSession != GAME_ACTIVE) return;

  int pIdx = -1;
  for (int i = 0; i < 4; i++) {
    if (players[i].isOnline && players[i].clientId == clientId) {
      pIdx = i;
      break;
    }
  }

  if (pIdx == -1 || players[pIdx].color == NONE || players[pIdx].isDead) return;

  // Cost in energy per pattern
  int cost = 999;
  if (seedId == 1) cost = 15;
  else if (seedId == 2) cost = 25;
  else if (seedId == 3) cost = 30;
  else if (seedId == 4) cost = 75;

  if (players[pIdx].energy >= cost) {
    players[pIdx].energy -= cost;
    spawnMacroPattern(seedId, x, y, players[pIdx].color);
  }
}

// ============================================================================
// GAME SIMULATION ENGINE
// ============================================================================

/**
 * @brief Execute one simulation tick (called every 150 ms).
 * 
 * Applies cellular automaton rules (growth, competition, invasion) and
 * updates player statistics based on the current game mode.
 * Also checks for match end conditions.
 */
void runSimulationEngineTick() {
  if (currentSession != GAME_ACTIVE) return;

  // --- Check match time limit ---
  unsigned long runningDuration = millis() - matchStartTime;
  if (runningDuration >= matchLimit) {
    TeamColor winner = NONE;
    int bestScore = -1;

    for (int p = 0; p < 4; p++) {
      if (players[p].color == NONE || players[p].isDead) continue;

      int score = 0;
      if (selectedMode == CORE_MELTDOWN)      score = players[p].hp;
      else if (selectedMode == GENETIC_TAKEOVER) score = players[p].dnaPct;
      else if (selectedMode == CRITICAL_MASS)    score = players[p].dominance;

      // Fallback: if all stats are tied, count their actual cells on the grid
      if (score == 0) {
        for (int i = 0; i < MAP_SIZE; i++)
          if (gameGrid[i] == players[p].color) score++;
      }

      if (score > bestScore) {
        bestScore = score;
        winner = players[p].color;
      }
    }

    triggerGameOver(winner);
    return;
  }

  // --- Energy pulse ---
  bool pulseEP = false;
  if (millis() - lastEnergyPulse >= energyPeriod) {
    lastEnergyPulse = millis();
    pulseEP = true;
  }

  // --- Cellular automaton step ---
  static uint8_t nextGrid[MAP_SIZE];
  memcpy(nextGrid, gameGrid, MAP_SIZE);

  int baseGrowthChance = (selectedMode == CORE_MELTDOWN) ? 22 : 12;
  int structuralOffset = esp_random() % MAP_SIZE;

  for (int i = 0; i < MAP_SIZE; i++) {
    int idx = (i + structuralOffset) % MAP_SIZE;
    uint8_t cellColor = gameGrid[idx];

    if (cellColor != NONE) {
      int y = idx / GRID_WIDTH;
      int x = idx % GRID_WIDTH;

      // Neighbour indices (Von Neumann neighbourhood)
      int neighbors[4] = {
        (x > 0) ? idx - 1 : -1,
        (x < GRID_WIDTH - 1) ? idx + 1 : -1,
        (y > 0) ? idx - GRID_WIDTH : -1,
        (y < GRID_HEIGHT - 1) ? idx + GRID_WIDTH : -1
      };

      for (int n = 0; n < 4; n++) {
        if (neighbors[n] != -1) {
          uint8_t targetCell = gameGrid[neighbors[n]];
          if (targetCell == NONE) {
            // Growth into empty cell
            if ((esp_random() % 100) < baseGrowthChance) {
              nextGrid[neighbors[n]] = cellColor;
            }
          }
          else if (selectedMode == GENETIC_TAKEOVER && targetCell != cellColor) {
            // Invasion: small chance to convert enemy cell
            if ((esp_random() % 100) < 4) {
              nextGrid[neighbors[n]] = cellColor;
            }
          }
        }
      }
    }
  }

  // CORE_MELTDOWN: random annihilation of cells to simulate core decay
  if (selectedMode == CORE_MELTDOWN) {
    for (int h = 0; h < 6; h++) {
      int TargetAnnihilationIdx = esp_random() % MAP_SIZE;
      if (nextGrid[TargetAnnihilationIdx] != NONE)
        nextGrid[TargetAnnihilationIdx] = NONE;
    }
  }

  memcpy(gameGrid, nextGrid, MAP_SIZE);

  // --- Update player statistics ---
  int totalActiveCells = 0;
  int colorCounts[5] = {0, 0, 0, 0, 0};
  for (int i = 0; i < MAP_SIZE; i++) {
    if (gameGrid[i] <= 4 && gameGrid[i] > 0) {
      colorCounts[gameGrid[i]]++;
      totalActiveCells++;
    }
  }

  for (int p = 0; p < 4; p++) {
    if (players[p].color == NONE) continue;

    TeamColor myColor = players[p].color;

    // Energy regeneration
    if (pulseEP && !players[p].isDead && players[p].isOnline) {
      players[p].energy = min(100, players[p].energy + 8);
    }

    Coordinate core = CORE_ANCHORS[myColor];

    // --- Mode-specific updates ---
    if (selectedMode == CORE_MELTDOWN) {
      // Count enemy cells within 1 cell of core
      int threats = 0;
      for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
          int cx = core.x + dx;
          int cy = core.y + dy;
          if (cx >= 0 && cx < GRID_WIDTH && cy >= 0 && cy < GRID_HEIGHT) {
            uint8_t checkingCell = gameGrid[cy * GRID_WIDTH + cx];
            if (checkingCell != NONE && checkingCell != myColor) threats++;
          }
        }
      }
      if (threats > 0) {
        players[p].hp = max(0, players[p].hp - (threats * 2));
      }
      if (players[p].hp <= 0 && !players[p].isDead) {
        players[p].isDead = true;
        // Clear all cells of this color
        for (int i = 0; i < MAP_SIZE; i++) {
          if (gameGrid[i] == myColor) gameGrid[i] = NONE;
        }
      }
    }
    else if (selectedMode == GENETIC_TAKEOVER) {
      // Sample 5x5 area around core to compute DNA purity and invader
      int sampleCount = 0;
      int originalDNA = 0;
      int invaderCounts[5] = {0, 0, 0, 0, 0};

      for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
          int cx = core.x + dx;
          int cy = core.y + dy;
          if (cx >= 0 && cx < GRID_WIDTH && cy >= 0 && cy < GRID_HEIGHT) {
            uint8_t cell = gameGrid[cy * GRID_WIDTH + cx];
            sampleCount++;
            if (cell == myColor) originalDNA++;
            else if (cell != NONE) invaderCounts[cell]++;
          }
        }
      }
      players[p].dnaPct = (sampleCount > 0) ? (originalDNA * 100) / sampleCount : 0;

      int topInvader = NONE;
      int maxInvaderCount = 0;
      for (int c = 1; c <= 4; c++) {
        if (invaderCounts[c] > maxInvaderCount) {
          maxInvaderCount = invaderCounts[c];
          topInvader = c;
        }
      }
      players[p].dnaInvader = topInvader;

      // If DNA is 0 but there are invaders, player dies
      if (players[p].dnaPct == 0 && maxInvaderCount > 0 && !players[p].isDead) {
        players[p].isDead = true;
      }
    }
    else if (selectedMode == CRITICAL_MASS) {
      // Dominance = percentage of total cells owned
      players[p].dominance = (totalActiveCells > 0) ? (colorCounts[myColor] * 100) / totalActiveCells : 0;

      // If player has no cells after the first 3 seconds, they die
      if (colorCounts[myColor] == 0 && (millis() - matchStartTime > 3000) && !players[p].isDead) {
        players[p].isDead = true;
      }
    }
  }

  // --- Check for single survivor ---
  int liveTeams = 0;
  TeamColor winnerCandidate = NONE;
  for (int i = 0; i < 4; i++) {
    if (players[i].color != NONE && !players[i].isDead) {
      liveTeams++;
      winnerCandidate = players[i].color;
    }
  }

  if (liveTeams == 1 && (millis() - matchStartTime > 2000)) {
    triggerGameOver(winnerCandidate);
    return;
  }
  else if (liveTeams == 0) {
    triggerGameOver(NONE);
    return;
  }

  // --- Send updated grid to FPGA ---
  streamGridToFPGA();
}

// ============================================================================
// PLAYFIELD RESET
// ============================================================================

/**
 * @brief Reset the game grid and assign starting positions for all players.
 * 
 * Clears the grid, sets each player's anchor based on total player count,
 * and seeds their core cell on the grid.
 */
void resetAndAnchorPlayfield() {
  memset(gameGrid, 0, MAP_SIZE);

  // 1. Count how many players chose a color team slot
  playercount = 0;
  for (int i = 0; i < 4; i++) {
    if (players[i].clientId != 0 && players[i].isOnline && players[i].color != NONE) {
      playercount++;
    }
  }

  // 2. Assign dynamic locations based on the live pool sizes
  int runningSpawnIdx = 0;
  for (int i = 0; i < 4; i++) {
    if (players[i].color != NONE) {
      TeamColor c = players[i].color;
      players[i].isDead      = false;
      players[i].energy      = 30;
      players[i].hp          = 100;
      players[i].dnaPct      = 100;
      players[i].dnaInvader  = NONE;
      players[i].dominance   = 0;

      // Calculate dynamic positions and update CORE_ANCHORS
      Coordinate spawn = getPlayerSpawnPoint(playercount, runningSpawnIdx);
      CORE_ANCHORS[c] = spawn;

      // Seed the base core color onto the field grid array
      gameGrid[spawn.y * GRID_WIDTH + spawn.x] = static_cast<uint8_t>(c);

      runningSpawnIdx++;
    }
  }
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  dnsServer.processNextRequest();

  if (currentSession == COUNTDOWN) {
    if (millis() - countdownStartTime >= countdownLimit) {
      sendMatchStartClearPacket();
      resetAndAnchorPlayfield();
      currentSession = GAME_ACTIVE;
      matchStartTime = millis();
      lastEngineTick = millis();
      lastEnergyPulse = millis();

      JsonDocument launchDoc;
      launchDoc["status"] = "LAUNCH_GAME";
      String launchMsg;
      serializeJson(launchDoc, launchMsg);
      ws.textAll(launchMsg);
    }
  }
  else if (currentSession == GAME_ACTIVE) {
    if (millis() - lastEngineTick >= tickPeriod) {
      lastEngineTick = millis();
      runSimulationEngineTick();
      broadcastEngineTick();
    }
  }
}

// ============================================================================
// WEBSOCKET EVENT HANDLER
// ============================================================================

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT: {
      IPAddress clientIP = client->remoteIP();
      Serial.printf("[WS_CONNECT] Handshake target: %u | IP: %s\n", client->id(), clientIP.toString().c_str());

      // Check if this IP already has a slot (reconnection)
      int reclaimedIdx = -1;
      for (int i = 0; i < 4; i++) {
        if (players[i].clientId != 0 && players[i].ipAddress == clientIP) {
          reclaimedIdx = i;
          break;
        }
      }

      if (reclaimedIdx != -1) {
        uint32_t displacedId = players[reclaimedIdx].clientId;
        players[reclaimedIdx].clientId = client->id();
        players[reclaimedIdx].isOnline = true;

        if (displacedId == hostClientId || hostClientId == 0) {
          hostClientId = client->id();
        }
      }

      // Send client its ID
      JsonDocument idDoc;
      idDoc["status"] = "IAM";
      idDoc["id"] = client->id();
      String idMsg;
      serializeJson(idDoc, idMsg);
      client->text(idMsg);

      // Re‑sync state if reconnected
      if (reclaimedIdx != -1) {
        if (currentSession == LOBBY)
          broadcastLobbyState();
        else if (currentSession == COUNTDOWN) {
          JsonDocument startDoc;
          startDoc["status"] = "START_COUNTDOWN";
          startDoc["mode"] = (int)selectedMode;
          String startMsg;
          serializeJson(startDoc, startMsg);
          client->text(startMsg);
        }
        else if (currentSession == GAME_ACTIVE) {
          JsonDocument fastLaunch;
          fastLaunch["status"] = "LAUNCH_GAME";
          String flMsg;
          serializeJson(fastLaunch, flMsg);
          client->text(flMsg);
        }
      }
      break;
    }

    case WS_EVT_DISCONNECT: {
      for (int i = 0; i < 4; i++) {
        if (players[i].clientId == client->id()) {
          players[i].isOnline = false;
          break;
        }
      }
      if (currentSession == LOBBY)
        broadcastLobbyState();
      break;
    }

    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo *)arg;
      if (info->final && info->index == 0 && info->len == len) {
        if (info->opcode == WS_TEXT) {
          std::string rawMsg((char *)data, len);
          processClientMessage(client, String(rawMsg.c_str()));
        }
        else if (info->opcode == WS_BINARY) {
          if (len >= 3) {
            handleBinaryInput(client->id(), data[0], data[1], data[2]);
          }
        }
      }
      break;
    }

    default:
      break;
  }
}

// ============================================================================
// MESSAGE PROCESSING
// ============================================================================

/**
 * @brief Process a JSON WebSocket text message from a client.
 */
void processClientMessage(AsyncWebSocketClient *client, String msg) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, msg);
  if (err) return;

  uint32_t clientId = client->id();
  IPAddress clientIP = client->remoteIP();
  String action = doc["action"];

  if (action == "join_lobby") {
    // Check if already in a slot
    bool alreadyExists = false;
    for (int i = 0; i < 4; i++) {
      if (players[i].clientId == clientId) {
        alreadyExists = true;
        break;
      }
    }

    if (!alreadyExists) {
      int targetSlot = -1;
      // Find empty slot
      for (int i = 0; i < 4; i++) {
        if (players[i].clientId == 0) {
          targetSlot = i;
          break;
        }
      }
      if (targetSlot == -1) {
        // All slots taken, try to reclaim a disconnected slot
        for (int i = 0; i < 4; i++) {
          if (!players[i].isOnline) {
            targetSlot = i;
            if (players[i].color != NONE)
              colorTaken[players[i].color] = false;
            break;
          }
        }
      }

      if (targetSlot != -1) {
        players[targetSlot].clientId = clientId;
        players[targetSlot].ipAddress = clientIP;
        players[targetSlot].color = NONE;
        players[targetSlot].isOnline = true;
        if (hostClientId == 0)
          hostClientId = clientId;
      }
    }

    // Ensure host is valid
    bool hostIsOnline = false;
    for (int i = 0; i < 4; i++) {
      if (players[i].clientId == hostClientId && players[i].isOnline) {
        hostIsOnline = true;
        break;
      }
    }
    if (!hostIsOnline) {
      for (int i = 0; i < 4; i++) {
        if (players[i].isOnline && players[i].clientId != 0) {
          hostClientId = players[i].clientId;
          break;
        }
      }
    }
    broadcastLobbyState();
  }
  else if (action == "select_mode") {
    if (clientId == hostClientId) {
      int targetMode = doc["mode"];
      selectedMode = (GameMode)targetMode;
      broadcastLobbyState();
    }
  }
  else if (action == "select_color") {
    int choice = doc["color"];
    for (int i = 0; i < 4; i++) {
      if (players[i].clientId == clientId) {
        if (players[i].color == choice) {
          colorTaken[choice] = false;
          players[i].color = NONE;
        }
        else if (!colorTaken[choice]) {
          if (players[i].color != NONE)
            colorTaken[players[i].color] = false;
          players[i].color = (TeamColor)choice;
          colorTaken[choice] = true;
        }
        break;
      }
    }
    broadcastLobbyState();
  }
  else if (action == "start_match") {
    if (clientId == hostClientId) {
      int readyTeams = 0;
      for (int i = 0; i < 4; i++) {
        if (players[i].clientId != 0 && players[i].isOnline && players[i].color != NONE) {
          readyTeams++;
        }
      }

      if (readyTeams < 2) {
        Serial.printf("[GATEKEEPER] Rejected start request from Host %u. Only %d team(s) assigned. 2 Minimum.\n",
                      clientId, readyTeams);

        JsonDocument rejectDoc;
        rejectDoc["status"] = "LOBBY_ERROR";
        rejectDoc["message"] = "Cannot execute deployment: Minimum 2 players must pick teams.";
        String rejectMsg;
        serializeJson(rejectDoc, rejectMsg);
        client->text(rejectMsg);
        return;
      }

      currentSession = COUNTDOWN;
      countdownStartTime = millis();
      JsonDocument startDoc;
      startDoc["status"] = "START_COUNTDOWN";
      startDoc["mode"] = (int)selectedMode;
      String startMsg;
      serializeJson(startDoc, startMsg);
      ws.textAll(startMsg);
    }
  }
  else if (action == "return_to_lobby") {
    if (clientId == hostClientId) {
      currentSession = LOBBY;
      winningTeam = NONE;
      digitalWrite(HARDWARE_LED, LED_OFF);
      broadcastLobbyState();
    }
  }
}

// ============================================================================
// GAME OVER AND BROADCAST HELPERS
// ============================================================================

/**
 * @brief Trigger game over state, notify clients, and send FPGA fill packet.
 * 
 * @param winner  Winning team color (NONE for draw)
 */
void triggerGameOver(TeamColor winner) {
  currentSession = GAME_OVER;
  winningTeam = winner;
  digitalWrite(HARDWARE_LED, LED_ON);

  if (winner != NONE)
    colorTaken[winner] = false;

  JsonDocument winDoc;
  winDoc["status"] = "GAME_OVER";
  winDoc["winner"] = (int)winningTeam;
  winDoc["hostId"] = (int)hostClientId;

  String winMsg;
  serializeJson(winDoc, winMsg);
  ws.textAll(winMsg);
  sendMatchEndFillPacket(winner);
}

/**
 * @brief Broadcast the current lobby state to all WebSocket clients.
 */
void broadcastLobbyState() {
  if (currentSession != LOBBY) return;

  JsonDocument doc;
  doc["status"] = "LOBBY_UPDATE";
  doc["currentMode"] = (int)selectedMode;
  doc["hostId"] = (int)hostClientId;

  int idx = 0;
  for (int i = 0; i < 4; i++) {
    if (players[i].clientId != 0 && players[i].isOnline) {
      doc["terminals"][idx]["id"] = (int)players[i].clientId;
      doc["terminals"][idx]["color"] = (int)players[i].color;
      idx++;
    }
  }
  String response;
  serializeJson(doc, response);
  ws.textAll(response);
}

/**
 * @brief Broadcast a game tick update (player statistics) to all clients.
 */
void broadcastEngineTick() {
  JsonDocument doc;
  doc["status"] = "GAME_TICK";
  doc["mode"] = (int)selectedMode;

  JsonArray nodes = doc["nodes"].to<JsonArray>();
  for (int i = 0; i < 4; i++) {
    if (players[i].clientId != 0 && players[i].color != NONE) {
      JsonObject node = nodes.add<JsonObject>();
      node["id"] = players[i].clientId;
      node["color"] = static_cast<int>(players[i].color);
      node["ep"] = players[i].energy;
      node["dead"] = players[i].isDead;
      node["hp"] = players[i].hp;
      node["dna"] = players[i].dnaPct;
      node["v"] = static_cast<int>(players[i].dnaInvader);
      node["eq"] = players[i].dominance;
    }
  }
  String response;
  serializeJson(doc, response);
  ws.textAll(response);
}