# CLAUDE.md - RAF RTT Training System

## Project Overview

RAF RTT Training System is a wireless shooting target system for BB/Airsoft training with real-time feedback. It consists of embedded ESP32 firmware (Arduino C++) and a web interface (HTML/CSS/JS). The project documentation is written in **Dutch**.

**Version:** 1.0.0
**License:** Free for personal use

## Repository Structure

```
RAF_RTT_Training_System/
├── master_controller/
│   ├── master_controller.ino   # ESP32-S3 master firmware (~1,425 lines)
│   └── config.h                # Master configuration (network, game, scoring)
├── target_node/
│   ├── target_node.ino         # ESP32 target firmware (~573 lines)
│   └── config.h                # Target configuration (pins, sensor, LEDs)
├── web_interface/
│   └── index.html              # Full SPA web dashboard (~1,385 lines)
├── docs/
│   ├── ARCHITECTURE.md         # System architecture and protocol diagrams
│   ├── INSTALL.md              # 7-part installation guide
│   ├── WIRING.md               # Hardware schematics and pin diagrams
│   └── SHOPPING_LIST.md        # Bill of materials (~€175-315)
└── README.md                   # Project overview and quick start
```

## Tech Stack

### Embedded (Arduino C++)
- **ESP32-S3** — Master controller (WiFi AP + WebSocket server + ESP-NOW coordinator)
- **ESP32 WROOM-32** (x8) — Target nodes (piezo sensor + LEDs + buzzer)
- **Libraries:** WiFi.h, esp_now.h, AsyncTCP, ESPAsyncWebServer, ArduinoJson, FastLED, Preferences

### Web Interface
- **Vanilla HTML/CSS/JS** — Single-file SPA, no build tools or frameworks
- **WebSocket** — Real-time bidirectional communication on port 81 (`ws://<ip>:81/ws`)
- A compact fallback version of the UI is embedded in `master_controller.ino` as `PROGMEM`

## Architecture

```
[Web Browser] <--WebSocket--> [ESP32-S3 Master] <--ESP-NOW--> [ESP32 Target 1..8]
                (port 81/ws)     (WiFi AP)        (<5ms)       (piezo + LED + buzzer)
```

- **Master Controller** runs a WiFi Access Point (`RAF RTT Training` / `shoot2score`), serves the web interface on port 80, and manages game state
- **Target Nodes** communicate with the master via ESP-NOW using compact binary structs (8 bytes each direction)
- **Web clients** connect via WebSocket and exchange JSON messages for game control and real-time updates

### Communication Protocols

**ESP-NOW (Master <-> Targets):** Binary struct-based, 8-byte messages
- `MasterMessage`: cmd, targetId, r, g, b, sound, param1, param2
- `TargetMessage`: type, targetId, intensity (uint16), timestamp (uint32), status

**WebSocket (Master <-> Browser):** JSON events
- Server sends: `state`, `hit`, `timer`, `score`, `targetUpdate`, `gameEnd`, `highscores`
- Client sends: `start`, `pause`, `stop`, `reset`, `setMode`, `activateTarget`, `getState`, `getHighscores`, `clearHighscores`

## Build & Upload

There is **no automated build system**. Firmware is compiled and uploaded via Arduino IDE:

1. Install Arduino IDE with ESP32 board support
2. Install libraries: FastLED, ArduinoJson (via Library Manager); ESPAsyncWebServer, AsyncTCP (manual install)
3. **Master:** Select "ESP32S3 Dev Module", open `master_controller/master_controller.ino`, upload
4. **Targets:** Select "ESP32 Dev Module", set `TARGET_ID` (1-8) in `target_node/config.h`, open `target_node/target_node.ino`, upload each node individually
5. Debug output: Serial monitor at 115200 baud

## Testing

There are **no automated tests**. Verification is done manually:
- Serial monitor output at 115200 baud (`DEBUG_ENABLED true` in both config.h files)
- Web interface dashboard shows live target status and event log
- Hardware feedback: LED colors, buzzer sounds, hit detection response
- Step-by-step test procedures are documented in `docs/INSTALL.md`

## Code Conventions

### General Style
- **Language:** Comments and documentation are in Dutch; code identifiers (variables, functions, enums) are in English
- **File headers:** Block comment with project name, version, description, and installation steps
- **Section separators:** `// ============================================================================` banners with section titles
- **Include guards:** `#ifndef CONFIG_H` / `#define CONFIG_H` in headers

### Naming
- **Constants/defines:** `UPPER_SNAKE_CASE` (e.g., `PIEZO_THRESHOLD`, `CMD_ACTIVATE`)
- **Functions:** `camelCase` (e.g., `processHit()`, `activateTarget()`, `broadcastState()`)
- **Variables:** `camelCase` (e.g., `targetId`, `gameRunning`, `lastHitTime`)
- **Structs:** `PascalCase` (e.g., `MasterMessage`, `TargetMessage`, `HighscoreEntry`)
- **Enums:** `PascalCase` names with `UPPER_SNAKE_CASE` values (e.g., `enum Commands { CMD_ACTIVATE = 1 }`)

### Configuration
- All tunable parameters are in `config.h` files, not inline in `.ino` files
- Pin assignments, thresholds, timing, scoring, colors, and network settings are all configurable via `#define`
- Each target node requires a unique `TARGET_ID` (1-8) set before upload

### Web Interface
- Single-file architecture: all HTML, CSS, and JS in one `index.html`
- CSS uses custom properties (variables) for theming
- Grid layout with responsive design
- WebSocket auto-reconnect logic built in
- TV mode for large screen display

## Key Configuration Values

### Master (`master_controller/config.h`)
| Define | Default | Purpose |
|--------|---------|---------|
| `WIFI_SSID` | `"RAF RTT Training"` | Access point name |
| `WIFI_PASSWORD` | `"shoot2score"` | AP password (min 8 chars) |
| `MAX_TARGETS` | `8` | Maximum number of targets |
| `DEFAULT_GAME_TIME` | `60` | Game duration in seconds |
| `DEFAULT_TARGET_TIME` | `3` | Seconds per target in sequence/random |
| `POINTS_HIT` | `100` | Points for hitting active target |
| `POINTS_NOSHOOT_HIT` | `-50` | Penalty for hitting no-shoot target |
| `POINTS_BONUS_FAST` | `50` | Bonus for hit under 1 second |

### Target Node (`target_node/config.h`)
| Define | Default | Purpose |
|--------|---------|---------|
| `TARGET_ID` | `1` | Unique per target (1-8) |
| `PIEZO_PIN` | `34` | ADC pin for piezo sensor |
| `LED_PIN` | `13` | WS2812B data pin |
| `BUZZER_PIN` | `26` | Buzzer output pin |
| `PIEZO_THRESHOLD` | `150` | Hit detection threshold (0-4095) |
| `PIEZO_DEBOUNCE_MS` | `100` | Debounce time between hits |
| `NUM_LEDS` | `12` | LEDs per target strip |
| `HEARTBEAT_INTERVAL` | `5000` | Heartbeat to master (ms) |

## Game Modes

1. **Free Play** — All targets active simultaneously, shoot freely
2. **Sequence** — Targets activate in order, hit to advance
3. **Random** — Random target activates, hit before timeout
4. **Shoot/No Shoot** — 70% green (shoot) / 30% red (don't shoot) targets

## Data Persistence

- **Highscores** (top 10) are stored in ESP32 NVS flash via `Preferences.h`
- Game state is ephemeral (in-memory only, lost on reboot)
- No external database

## Common Modification Tasks

### Adding a new game mode
1. Add mode to the `GameMode` enum in `master_controller.ino`
2. Add game logic in the `updateGame()` function
3. Add mode button in the web interface (`index.html` and the PROGMEM fallback)
4. Add WebSocket command handling for the new mode

### Adjusting sensitivity
- Change `PIEZO_THRESHOLD` in `target_node/config.h` (higher = less sensitive)
- Change `PIEZO_DEBOUNCE_MS` for hit interval timing

### Adding more targets
- Change `MAX_TARGETS` in `master_controller/config.h`
- Flash each new ESP32 with a unique `TARGET_ID`
- Update the web interface grid if needed

### Changing scoring
- All point values are `#define` constants in `master_controller/config.h`

## Important Notes for AI Assistants

- The web interface exists in **two places**: `web_interface/index.html` (full version) and embedded as `PROGMEM` in `master_controller.ino` (compact fallback). Changes to the UI may need to be reflected in both.
- ESP-NOW messages use fixed 8-byte binary structs for performance. Do not change struct sizes without updating both master and target firmware.
- The `masterMAC` in `target_node/config.h` defaults to broadcast (`0xFF` x6). For production, this should be set to the actual master MAC address.
- Arduino `.ino` files are compiled as C++ but use Arduino framework conventions (global `setup()` and `loop()` entry points).
- There is no package manager or dependency lock file. Library versions are not pinned.
- All documentation in `docs/` is in Dutch.
