# RTT Target System

Een professioneel, modulair draadloos doelsysteem voor schiettraining, ontwikkeld door Running the Target (RTT).

```
    ┌─────────────────────────────────────────────────────────────┐
    │                    RTT TARGET SYSTEM                        │
    │                                                             │
    │    6 Draadloze Targets       <10ms Responstijd              │
    │    Webinterface              Leaderboard                    │
    │    5 Game Modi               TV-compatibel                  │
    │                                                             │
    └─────────────────────────────────────────────────────────────┘
```

## Features

- **Real-time hit detectie** via piezo sensoren op metalen platen
- **Draadloze communicatie** via ESP-NOW (<10ms latency)
- **WS2812B LED feedback** (wit flash 300ms bij hit)
- **Web dashboard** (wit/rood/zwart thema)
- **Dark mode toggle**
- **5 Game Modi:**
  - Free Play — alle targets actief, onbeperkt schieten
  - Sequence — targets lichten op in volgorde
  - Random — willekeurige targets activeren
  - Manual — handmatige target selectie via web UI
  - Shoot/No-Shoot — rood=shoot(+10), groen=no-shoot(-11), dark mode met flash feedback
- **Timer & Stopwatch**
- **Start/Stop/Pauze/Reset controls**
- **Top 3 leaderboard** met persistent opslag
- **Configureerbare gevoeligheid** (standaard threshold: 100)
- **OTA update support**

## Hardware Overzicht

| Component | Functie |
|-----------|---------|
| ESP32-S3 | Master Controller (Web UI host) |
| ESP32 WROOM-32 (x6) | Target Nodes |
| Piezo sensoren | Hit detectie |
| WS2812B LED strips | Visuele feedback |
| 4mm metalen platen | Doelplaten |

## Architectuur

```
Master ESP32-S3  ←──ESP-NOW──→  Target Node 1
  (Web UI host)  ←──ESP-NOW──→  Target Node 2
  192.168.4.1    ←──ESP-NOW──→  Target Node 3
                 ←──ESP-NOW──→  Target Node 4
                 ←──ESP-NOW──→  Target Node 5
                 ←──ESP-NOW──→  Target Node 6
```

## Netwerk

- WiFi SSID: `RAF RTT TRAINING SYSTEM`
- WiFi Password: `12345678`
- Master MAC: `28:05:A5:07:41:FD`
- ESP-NOW channel: 1

### Bekende Target MACs

| Target | MAC Adres | Opmerkingen |
|--------|-----------|-------------|
| Target 1 | `28:05:A5:07:5D:88` | GPIO34=piezo, D4=LED |
| Target 2 | `B0:CB:D8:E9:E9:50` | |
| Target 3 | `B0:CB:D8:E9:E7:DC` | |

## Quick Start

### 1. Libraries Installeren

In Arduino IDE, installeer via Library Manager:
- FastLED
- ArduinoJson
- ESPAsyncWebServer (handmatig)
- AsyncTCP (handmatig)

### 2. Master Controller Uploaden

1. Open `master_controller/master_controller.ino`
2. Selecteer **ESP32S3 Dev Module**
3. Upload

### 3. Target Nodes Uploaden

Voor elk target (1-6):

1. Open `target_node/config.h`
2. Verander `TARGET_ID` naar 1, 2, 3... etc.
3. Open `target_node/target_node.ino`
4. Selecteer **ESP32 Dev Module**
5. Upload

### 4. Verbinden

1. Verbind met WiFi: `RAF RTT TRAINING SYSTEM`
2. Wachtwoord: `12345678`
3. Open browser: `http://192.168.4.1`

## Mapstructuur

```
BB_Target_System/
├── master_controller/
│   ├── master_controller.ino   # Master firmware (MasterV3Sec)
│   └── config.h                # Master configuratie
├── target_node/
│   ├── target_node.ino         # Target firmware
│   └── config.h                # Target configuratie
├── web_interface/
│   ├── index.html              # Uitgebreide webinterface
│   └── running_test.html       # Hardloop test timer
├── docs/
│   ├── ARCHITECTURE.md         # Systeemarchitectuur
│   ├── WIRING.md               # Bedradingsschema
│   ├── INSTALL.md              # Installatie handleiding
│   └── SHOPPING_LIST.md        # Componenten lijst
└── README.md                   # Dit bestand
```

## Game Modi

| Modus | Beschrijving | Punten |
|-------|--------------|--------|
| **Free Play** | Alle targets actief, onbeperkt schieten | +100 per hit |
| **Sequence** | Targets lichten op in volgorde | +100, -10 timeout |
| **Random** | Willekeurig target licht op | +100, +50 snelheidsbonus |
| **Manual** | Handmatige target selectie via web UI | +100 per hit |
| **Shoot/No-Shoot** | Rood=shoot, groen=no-shoot | +10 / -11 |

## Webinterface

De webinterface (wit/rood/zwart thema) werkt op telefoon, tablet, laptop en TV:

- **Dashboard** met live target status
- **Timer** met countdown
- **Score** met hits/misses teller
- **Numpad** voor handmatige bediening
- **Top 3 leaderboard** van de dag
- **Event log** voor debugging
- **TV Mode** voor grote schermen
- **Dark mode** toggle

### Keyboard Shortcuts

| Toets | Actie |
|-------|-------|
| `Spatie` | Start/Pauze |
| `Escape` | Stop |
| `R` | Reset |
| `1-6` | Activeer target |
| `T` | Toggle TV mode |

## Bedrading

Zie `docs/WIRING.md` voor volledige schema's.

### Target Node Pinout

| ESP32 Pin | Component |
|-----------|-----------|
| GPIO34 | Piezo sensor |
| GPIO13 | LED strip DIN |
| GPIO26 | Buzzer |
| VIN | 5V voeding |
| GND | Common ground |

## Configuratie

### Piezo Gevoeligheid

In `target_node/config.h`:

```cpp
#define PIEZO_THRESHOLD   100   // Hoger = minder gevoelig (standaard: 100)
#define PIEZO_DEBOUNCE_MS 100   // Tijd tussen hits
```

### WiFi Instellingen

In `master_controller/config.h`:

```cpp
#define WIFI_SSID       "RAF RTT TRAINING SYSTEM"
#define WIFI_PASSWORD   "12345678"
```

## Firmware — MasterV3Sec

Laatste grote milestone:

- Security locking mechanisme
- OTA update support
- Volledige suite game mode fixes
- Arduino Core 3.x compatibel

## CloudBridge

`RTT_CloudBridge.h` — header voor het posten van game scores vanuit `endGame()` naar Supabase.
Doel: leaderboard bridge tussen ESP32 systeem en de RTT App.

## Troubleshooting

| Probleem | Oplossing |
|----------|-----------|
| Target komt niet online | Check voeding, herstart beide |
| Geen hit detectie | Pas `PIEZO_THRESHOLD` aan in config |
| LED strip werkt niet | Check DIN verbinding, 5V voeding |
| WebSocket disconnects | Refresh browser, check WiFi |

## Code Conventions

- Arduino Core 3.x (niet 2.x)
- ESP-NOW op channel 1
- WiFi AP als fallback (password: `12345678`)
- Web UI: wit/rood/zwart thema, responsive
- Serial baud rate: 115200
- Gebruik `delay()` spaarzaam — FreeRTOS tasks waar mogelijk

---

**RTT — Running the Target** | Train real. Train smart.
