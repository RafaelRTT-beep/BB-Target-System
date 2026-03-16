# RTT Fitness Test Timer

Timing systeem voor groepstesten (max 15 deelnemers). Buddy's bedienen een timer op hun telefoon, een admin ziet alles live op een dashboard. Draait op een ESP32 via het lokale WiFi-netwerk.

## Features

- **Admin dashboard** — Sessie aanmaken, oefeningen starten, live resultaten
- **Buddy scherm** — Grote timer met STOP knop, geoptimaliseerd voor telefoon
- **3 Oefeningen** — Sprint, Hardlopen, Schietoefening
- **Real-time sync** — WebSocket communicatie tussen admin en buddy's
- **QR codes** — Buddy's scannen een QR code om hun timer te openen
- **Data export** — JSON en CSV download van resultaten

## Installatie

### 1. Libraries installeren

In Arduino IDE Library Manager:
- **ArduinoJson** (v7+)
- **ESPAsyncWebServer** (handmatig installeren)
- **AsyncTCP** (handmatig installeren)

### 2. WiFi configureren

Open `config.h` en pas aan:

```cpp
#define WIFI_SSID       "JouwNetwerk"
#define WIFI_PASSWORD   "JouwWachtwoord"
```

### 3. LittleFS data uploaden

Installeer de **ESP32 LittleFS Upload** plugin voor Arduino IDE. Upload de `data/` map naar de ESP32.

### 4. Firmware uploaden

1. Open `RTT_FitnessTest.ino` in Arduino IDE
2. Selecteer **ESP32 Dev Module** (of ESP32-S3)
3. Upload

### 5. Starten

1. Open Serial Monitor (115200 baud) — het IP-adres verschijnt
2. Open `http://<ip>/admin` op een tablet of laptop
3. Maak een sessie aan met deelnemersnamen
4. Wijs deelnemers toe aan buddy's
5. Laat buddy's de QR code scannen
6. Start een oefening — alle buddy-timers starten tegelijk
7. Buddy's drukken op STOP als hun deelnemer klaar is

## Mapstructuur

```
RTT_FitnessTest/
├── RTT_FitnessTest.ino   # Hoofdfirmware
├── config.h               # WiFi en systeemconfiguratie
├── webserver.h             # Routes, WebSocket, sessiebeheer
├── data/
│   ├── admin.html          # Admin dashboard
│   ├── buddy.html          # Buddy timer scherm
│   └── style.css           # Gedeelde styling
└── README.md
```

## Oefeningen

| Oefening | Timer | Extra |
|----------|-------|-------|
| Sprint | Seconden (2 decimalen) | — |
| Hardlopen | Minuten:seconden | — |
| Schietoefening | Seconden (2 decimalen) | Score invoerveld |

## API Endpoints

- `GET /admin` — Admin dashboard
- `GET /buddy?id=X` — Buddy timer (X = buddy nummer)
- `GET /api/results` — Resultaten als JSON
- `GET /api/export` — CSV download
- `WS /ws` — WebSocket voor real-time communicatie

## Buzzer (optioneel)

Sluit een actieve buzzer aan op een GPIO pin en activeer in `config.h`:

```cpp
#define BUZZER_PIN     26
```

Er klinkt een kort signaal bij het starten van een oefening.
