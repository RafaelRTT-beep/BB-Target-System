# RAF RTT Training System

Een professioneel, draadloos schietdoelsysteem voor BB/Airsoft training met real-time feedback.

```
    ┌─────────────────────────────────────────────────────────────┐
    │                 RAF RTT TRAINING SYSTEM                      │
    │                                                             │
    │    🎯 8 Draadloze Targets    ⚡ <10ms Responstijd           │
    │    📱 Webinterface           🏆 Highscores                  │
    │    🎮 4 Game Modi            📺 TV-compatibel               │
    │                                                             │
    └─────────────────────────────────────────────────────────────┘
```

## Features

- **Real-time hit detectie** via piezo sensoren op metalen platen
- **Draadloze communicatie** via ESP-NOW (<10ms latency)
- **LED feedback** met WS2812B strips per target
- **Audio feedback** met buzzer per target
- **Webinterface** voor telefoon en TV
- **4 Game Modi:**
  - Free Play - alle targets actief
  - Sequence - targets in volgorde
  - Random - willekeurige targets
  - Shoot/No Shoot - groene vs rode targets
- **Timer & Stopwatch**
- **Puntentelling & Highscores**
- **Start/Stop/Pauze/Reset controls**
- **Numpad voor handmatige target selectie**

## Hardware Overzicht

| Component | Functie |
|-----------|---------|
| ESP32-S3 | Master Controller |
| ESP32 WROOM-32 (x8) | Target Nodes |
| Piezo sensoren | Hit detectie |
| WS2812B LED strips | Visuele feedback |
| Actieve buzzers | Audio feedback |
| 4mm metalen platen | Doelplaten |

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

Voor elk target (1-8):

1. Open `target_node/config.h`
2. Verander `TARGET_ID` naar 1, 2, 3... etc.
3. Open `target_node/target_node.ino`
4. Selecteer **ESP32 Dev Module**
5. Upload

### 4. Verbinden

1. Verbind met WiFi: `RAF RTT Training`
2. Wachtwoord: `shoot2score`
3. Open browser: `http://192.168.4.1`

## Mapstructuur

```
RAF_RTT_Training_System/
├── master_controller/
│   ├── master_controller.ino   # Master firmware
│   └── config.h                # Master configuratie
├── target_node/
│   ├── target_node.ino         # Target firmware
│   └── config.h                # Target configuratie
├── web_interface/
│   └── index.html              # Uitgebreide webinterface
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
| **Free Play** | Alle targets actief, schiet vrij | +100 per hit |
| **Sequence** | Raak targets in de juiste volgorde | +100, -10 timeout |
| **Random** | Willekeurig target licht op | +100, +50 snelheidsbonus |
| **Shoot/No Shoot** | Groene targets = goed, rode = fout | +100/-50 |

## Webinterface

De webinterface werkt op telefoon, tablet, laptop en TV:

- **Dashboard** met live target status
- **Timer** met countdown
- **Score** met hits/misses teller
- **Numpad** voor handmatige bediening
- **Highscores** top 3 van de dag
- **Event log** voor debugging
- **TV Mode** voor grote schermen

### Keyboard Shortcuts

| Toets | Actie |
|-------|-------|
| `Spatie` | Start/Pauze |
| `Escape` | Stop |
| `R` | Reset |
| `1-8` | Activeer target |
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
#define PIEZO_THRESHOLD   150   // Hoger = minder gevoelig
#define PIEZO_DEBOUNCE_MS 100   // Tijd tussen hits
```

### WiFi Instellingen

In `master_controller/config.h`:

```cpp
#define WIFI_SSID       "RAF RTT Training"
#define WIFI_PASSWORD   "shoot2score"
```

### Game Instellingen

```cpp
#define POINTS_HIT        100   // Punten voor hit
#define POINTS_MISS       -25   // Strafpunten
#define DEFAULT_GAME_TIME 60    // Speeltijd in seconden
```

## Troubleshooting

| Probleem | Oplossing |
|----------|-----------|
| Target komt niet online | Check voeding, herstart beide |
| Geen hit detectie | Verhoog `PIEZO_THRESHOLD` |
| LED strip werkt niet | Check DIN verbinding, 5V voeding |
| WebSocket disconnects | Refresh browser, check WiFi |

## Kosten Schatting

Budget versie (8 targets): **€175-250**
Premium versie: **€240-315**

Zie `docs/SHOPPING_LIST.md` voor complete lijst.

## Uitbreidingen

Het systeem is modulair en kan uitgebreid worden met:

- Meer targets (tot 20 ondersteund)
- Bewegende targets (servo motors)
- Afstandsbediening via fysieke knoppen
- Bluetooth speaker voor effecten
- Score database met statistieken

## Licentie

Dit project is vrij te gebruiken en aan te passen voor persoonlijk gebruik.

---

**Veel schietplezier!** 🎯
