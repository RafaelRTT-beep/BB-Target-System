# BB Target System - Systeemarchitectuur

## Overzicht

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        MASTER CONTROLLER (ESP32-S3)                      │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │
│  │  WiFi AP    │  │  WebSocket  │  │  ESP-NOW    │  │   Game      │     │
│  │  + Server   │  │   Server    │  │   Master    │  │   Engine    │     │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘     │
│         │                │                │                │             │
│         └────────────────┴────────────────┴────────────────┘             │
│                                   │                                      │
└───────────────────────────────────┼──────────────────────────────────────┘
                                    │ ESP-NOW (< 5ms latency)
           ┌────────────────────────┼────────────────────────┐
           │            │           │           │            │
           ▼            ▼           ▼           ▼            ▼
    ┌──────────┐  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐
    │ Target 1 │  │ Target 2 │ │ Target 3 │ │ Target 4 │ │ Target.. │
    │  ESP32   │  │  ESP32   │ │  ESP32   │ │  ESP32   │ │  ESP32   │
    │  Piezo   │  │  Piezo   │ │  Piezo   │ │  Piezo   │ │  Piezo   │
    │  LEDs    │  │  LEDs    │ │  LEDs    │ │  LEDs    │ │  LEDs    │
    │  Buzzer  │  │  Buzzer  │ │  Buzzer  │ │  Buzzer  │ │  Buzzer  │
    └──────────┘  └──────────┘ └──────────┘ └──────────┘ └──────────┘
```

## Componenten

### 1. Target Node (ESP32 per doeltje)
- **Piezo Sensor**: Detecteert BB-impact op metalen plaat
- **LED Strip**: WS2812B (8-16 LEDs per target)
- **Buzzer**: Geluids feedback bij hit
- **ESP-NOW**: Communicatie met master (< 5ms latency)

### 2. Master Controller (ESP32-S3)
- **WiFi Access Point**: Eigen netwerk voor beheer
- **Webserver**: Responsive interface
- **WebSocket**: Real-time updates (< 10ms)
- **ESP-NOW Master**: Ontvangt hits, stuurt commando's
- **Game Engine**: Alle game logica

### 3. Webinterface
- **Dashboard**: Live status alle targets
- **Game Modi**: 4 verschillende modi
- **Scorebord**: Top 3 van de dag
- **Controls**: Timer, stopwatch, start/stop

## Game Modi

| Modus | Beschrijving |
|-------|-------------|
| **Free Play** | Alle targets actief, schiet vrij |
| **Sequence** | Targets in vaste volgorde raken |
| **Random** | Willekeurige targets oplichten |
| **Shoot/No Shoot** | Groene targets raken, rode vermijden |

## Communicatie Protocol

### ESP-NOW Messages (Master → Target)
```json
{
  "cmd": "ACTIVATE|DEACTIVATE|COLOR|SOUND|RESET",
  "target": 1-8,
  "color": {"r": 0, "g": 255, "b": 0},
  "sound": "HIT|MISS|START|END"
}
```

### ESP-NOW Messages (Target → Master)
```json
{
  "type": "HIT|HEARTBEAT|STATUS",
  "target": 1-8,
  "intensity": 0-4095,
  "timestamp": 123456789
}
```

### WebSocket Messages (Master → Browser)
```json
{
  "event": "hit|gameState|score|timer",
  "data": { ... }
}
```

## Hardware Specificaties

### Per Target Node
| Component | Specificatie |
|-----------|-------------|
| MCU | ESP32 (WROOM-32) |
| Piezo | 27mm keramisch |
| LED Strip | WS2812B, 8-16 LEDs |
| Buzzer | Actieve buzzer 5V |
| Voeding | 5V 2A |

### Master Controller
| Component | Specificatie |
|-----------|-------------|
| MCU | ESP32-S3 |
| WiFi | 2.4GHz AP mode |
| Voeding | 5V 1A |

## Bestandsstructuur

```
BB_Target_System/
├── master_controller/
│   ├── master_controller.ino    # Hoofd firmware
│   ├── config.h                 # Configuratie
│   ├── game_engine.h/.cpp       # Game logica
│   ├── esp_now_handler.h/.cpp   # ESP-NOW communicatie
│   ├── web_server.h/.cpp        # Webserver
│   └── score_manager.h/.cpp     # Scores opslaan
├── target_node/
│   ├── target_node.ino          # Target firmware
│   ├── config.h                 # Configuratie
│   ├── piezo_sensor.h/.cpp      # Hit detectie
│   └── led_controller.h/.cpp    # LED animaties
├── web_interface/
│   ├── index.html               # Dashboard
│   ├── style.css                # Styling
│   └── app.js                   # JavaScript
└── docs/
    ├── ARCHITECTURE.md          # Dit document
    ├── WIRING.md                # Bedradingsschema
    └── INSTALL.md               # Installatie
```
