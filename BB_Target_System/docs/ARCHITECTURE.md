# RTT Target System - Systeemarchitectuur

## Overzicht

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     MASTER CONTROLLER (ESP32-S3)                        │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐   │
│  │  WiFi AP    │  │  WebSocket  │  │  ESP-NOW    │  │   Game      │   │
│  │  + Server   │  │   Server    │  │   Master    │  │   Engine    │   │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘   │
│         │                │                │                │           │
│         └────────────────┴────────────────┴────────────────┘           │
│                                   │                                    │
└───────────────────────────────────┼────────────────────────────────────┘
                                    │ ESP-NOW (< 10ms latency)
           ┌────────────────────────┼────────────────────────┐
           │            │           │           │            │
           ▼            ▼           ▼           ▼            ▼
    ┌──────────┐  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐
    │ Target 1 │  │ Target 2 │ │ Target 3 │ │ Target 4 │ │ Target 5 │ │ Target 6 │
    │  ESP32   │  │  ESP32   │ │  ESP32   │ │  ESP32   │ │  ESP32   │ │  ESP32   │
    │  Piezo   │  │  Piezo   │ │  Piezo   │ │  Piezo   │ │  Piezo   │ │  Piezo   │
    │  LEDs    │  │  LEDs    │ │  LEDs    │ │  LEDs    │ │  LEDs    │ │  LEDs    │
    └──────────┘  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘
```

## Netwerk

- WiFi SSID: `RAF RTT TRAINING SYSTEM`
- WiFi Password: `12345678`
- Master MAC: `28:05:A5:07:41:FD`
- 6 targets actief
- ESP-NOW channel: 1

### Bekende Target MACs

- Target 1: `28:05:A5:07:5D:88` (GPIO34=piezo, D4=LED)
- Target 2: `B0:CB:D8:E9:E9:50`
- Target 3: `B0:CB:D8:E9:E7:DC`

## Componenten

### 1. Target Node (ESP32 WROOM-32 per doeltje)
- **Piezo Sensor**: Detecteert BB-impact op metalen plaat (threshold: 100)
- **LED Strip**: WS2812B — wit flash 300ms bij hit
- **ESP-NOW**: Communicatie met master (< 10ms latency)

### 2. Master Controller (ESP32-S3)
- **WiFi Access Point**: `RAF RTT TRAINING SYSTEM`
- **Webserver**: Responsive interface (wit/rood/zwart thema)
- **WebSocket**: Real-time updates
- **ESP-NOW Master**: Ontvangt hits, stuurt commando's
- **Game Engine**: Alle game logica
- **OTA Updates**: ArduinoOTA + ElegantOTA support

### 3. Webinterface
- **Dashboard**: Live status alle targets
- **Game Modi**: 5 verschillende modi
- **Top 3 Leaderboard**: Persistent opslag
- **Controls**: Timer, stopwatch, start/stop/pauze/reset
- **Dark mode**: Toggle

## Game Modi

| Modus | Beschrijving |
|-------|-------------|
| **Free Play** | Alle targets actief, onbeperkt schieten |
| **Sequence** | Targets in vaste volgorde raken |
| **Random** | Willekeurige targets oplichten |
| **Manual** | Handmatige target selectie via web UI |
| **Shoot/No-Shoot** | Rood=shoot(+10), groen=no-shoot(-11), dark mode met flash feedback |

## Communicatie Protocol

### ESP-NOW Messages (Master → Target)
```json
{
  "cmd": "ACTIVATE|DEACTIVATE|COLOR|RESET",
  "target": 1-6,
  "color": {"r": 0, "g": 255, "b": 0}
}
```

### ESP-NOW Messages (Target → Master)
```json
{
  "type": "HIT|HEARTBEAT|STATUS",
  "target": 1-6,
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
| LED Strip | WS2812B |
| Voeding | 5V 2A |

### Master Controller
| Component | Specificatie |
|-----------|-------------|
| MCU | ESP32-S3 |
| MAC | 28:05:A5:07:41:FD |
| WiFi | 2.4GHz AP mode |
| Voeding | 5V 1A |

## Firmware — MasterV3Sec

- Security locking mechanisme
- OTA update support (ArduinoOTA + ElegantOTA)
- Volledige suite game mode fixes
- Arduino Core 3.x compatibel

## CloudBridge

`RTT_CloudBridge.h` — header voor het posten van game scores vanuit `endGame()` naar Supabase.
Leaderboard bridge tussen ESP32 systeem en de RTT App (React Native + Expo, Supabase backend).

## Bestandsstructuur

```
BB_Target_System/
├── master_controller/
│   ├── master_controller.ino    # Master firmware (MasterV3Sec)
│   └── config.h                 # Configuratie
├── target_node/
│   ├── target_node.ino          # Target firmware
│   └── config.h                 # Configuratie
├── web_interface/
│   ├── index.html               # Dashboard
│   └── running_test.html        # Hardloop test timer
└── docs/
    ├── ARCHITECTURE.md          # Dit document
    ├── WIRING.md                # Bedradingsschema
    ├── INSTALL.md               # Installatie handleiding
    └── SHOPPING_LIST.md         # Componenten lijst
```
