# CLAUDE.md — Running the Target (RTT)

> Dit bestand bevat de volledige projectcontext voor Claude Code.
> Eigenaar: Rafaël Degener — oprichter en eigenaar van RTT.
> Laatst bijgewerkt: maart 2026

-----

## Over RTT

Running the Target (RTT) is een indoor CQB airsoft facility.

- Adres: Burgemeester Smeetsweg 8, Zoeterwoude
- Faciliteiten: trainingshal (1000m²), dojo, kantine, vergaderruimte, kleedkamer
- Brandkleuren: wit (primair), zwart, rood (#E3263A)
- Typografie: DM Sans
- Website-stijl: Apple-inspired — clean white, minimalistisch, premium feel
- WhatsApp Business: wa.me/message/STUK27OW53RKK1

### Evenementen

| Event            | Dag     | Speeltijd     | Deuren        |
| ---------------- | ------- | ------------- | ------------- |
| Tactical Tuesday | Dinsdag | 19:30 – 22:30 | 18:15 – 23:15 |
| Airsoft Friday   | Vrijdag | 19:30 – 22:30 | 18:45 – 23:15 |
| Airsoft Sunday   | Zondag  | 13:00 – 16:00 | 12:15 – 17:00 |

Groepsboekingen op aanvraag via email/WhatsApp.

-----

## Technische Stack — Overzicht

### Hardware / Fabrication

- Laser: Gweike G3 Ultra — 60W MOPA fiber + 40W diode
- Lasersoftware: LightBurn
- 3D printer: Bambu Lab
- Server: Hetzner VPS (Ubuntu)

### Firmware / Embedded

- Platform: ESP32 (WROOM-32, ESP32-S3)
- Framework: Arduino IDE met ESP32 Arduino Core 3.x
- Communicatieprotocol: ESP-NOW (draadloos, peer-to-peer)
- OTA updates: ArduinoOTA + ElegantOTA (web-based .bin upload)
- LED: WS2812B via Adafruit NeoPixel / FastLED
- Sensoren: piezo (hit-detectie), HX711 load cells, HC-SR04 ultrasonic, ADXL345 accelerometer, velostat drukmatrix
- NFC: PN532 / RC522 (MFRC522) met NTAG213/215 tags
- Display: LiquidCrystal_I2C (16x2 LCD)

### Software / App

- RTT App: React Native + Expo (eerste mobile app project)
- Backend: Supabase (EU Frankfurt regio)
- Google Calendar sync: Node.js polling script op Hetzner
- Email agent: Python IMAP processing op Hetzner (systemd service: rtt-email-agent.service)
- WhatsApp automation: OpenClaw onderzocht voor Hetzner deployment

-----

## RTT Target System — Kern

Het hart van RTT: een modulair draadloos doelsysteem voor schiettraining.

### Architectuur

```
Master ESP32-S3  ←──ESP-NOW──→  Target Node 1
  (Web UI host)  ←──ESP-NOW──→  Target Node 2
  192.168.4.1    ←──ESP-NOW──→  Target Node 3
                 ←──ESP-NOW──→  Target Node 4
                 ←──ESP-NOW──→  Target Node 5
                 ←──ESP-NOW──→  Target Node 6
```

### Netwerk

- WiFi SSID: `RAF RTT TRAINING SYSTEM`
- WiFi Password: `12345678`
- Master MAC: `28:05:A5:07:41:FD`
- 6 targets actief
- ESP-NOW channel: 1

### Bekende Target MACs

- Target 1: `28:05:A5:07:5D:88` (GPIO34=piezo, D4=LED)
- Target 2: `B0:CB:D8:E9:E9:50`
- Target 3: `B0:CB:D8:E9:E7:DC`

### Game Modes

- Free Play — alle targets actief, onbeperkt schieten
- Sequence — targets lichten op in volgorde
- Random — willekeurige targets activeren
- Manual — handmatige target selectie via web UI
- Shoot/No-Shoot — rood=shoot(+10), groen=no-shoot(-11), dark mode met flash feedback

### Features

- Real-time hit-detectie (<10ms latency)
- Piezo-based met configureerbare threshold (standaard 100)
- WS2812B LED feedback (wit flash 300ms bij hit)
- Web dashboard (wit/rood/zwart thema)
- Start/Stop/Pauze/Reset controls
- Top 3 leaderboard met persistent opslag
- Timer en stopwatch
- Configureerbare gevoeligheid
- Dark mode toggle

### Firmware — MasterV3Sec

Laatste grote milestone:

- Security locking mechanisme
- OTA update support
- Volledige suite game mode fixes
- Arduino Core 3.x compatibel

### CloudBridge

`RTT_CloudBridge.h` — header voor het posten van game scores vanuit `endGame()` naar Supabase.
Doel: leaderboard bridge tussen ESP32 systeem en de RTT app.

-----

## Subsystemen

### Domination Box

Twee-knops capture-the-flag box met NeoPixel ring, LCD, en optionele QLC+ lichtintegratie.

- Knoppen: GPIO18 (groen), GPIO19 (blauw)
- NeoPixel: GPIO4 (24 LEDs)
- MOSFET: GPIO5
- LCD: I2C 0x27 (16x2), auto-detect met scanner
- WiFi AP: `RTT-DOMINATION` / `12345678`
- QLC+ integratie: HTTP calls, FreeRTOS non-blocking op Core 0, uitgeschakeld by default
- LCD progress bar: custom blokjes-characters
- Game states: IDLE, RUNNING, GAMEOVER
- Capture mechanic: groen vs blauw, score per seconde owned

### Spawn Box v2.0

5 knoppen, 5 dual-color LEDs (rood + groen), buzzer, ESP-NOW.

- Modes: Ready-Up, Wave Spawn, Spawn Tickets
- ESP-NOW communicatie met Master (spawn lock bij objective capture)
- Web interface: `RTT SPAWNBOX` / `12345678`
- LED pinout:
  - Groen (active LOW): GPIO 25, 26, 27, 14, 12
  - Rood (active HIGH): GPIO 17, 16, 4, 0, 2

### Ammo Crate Transport Game

Munitiekisten verplaatsen naar platforms met gewichtsmeting.

- Per platform: ESP32 + HX711 load cell + RC522 NFC + WS2812B LED paal
- 5 game modes: Free, Assigned, Sequence, Sabotage, Relay
- NFC tag registratie per kist (NTAG213/215)
- Calibration sketch beschikbaar
- Master ontvangt ESP-NOW, triggert Bitfocus Companion via HTTP

### Sprint Sensor System

Agility drill met HC-SR04 sensoren in driehoekige 360° opstelling.

- OLED display + web dashboard
- Configureerbare tijden
- Blauw=target, groen=gehaald, rood=te laat

### Smart Target (Velostat Matrix)

`RTT_SmartTarget.ino` — positiedetectie voor treffers.

- 10×10 velostat drukmatrix
- WebSocket hit broadcasting
- Web UI met ring scoring en live hit visualisatie
- Real-time trefpositie op telefoon/tablet

### NFC Scoreboard System

- Spelernaam opgeslagen op NFC tag ("RT" magic header)
- NTAG213/215 tags
- PN532 readers
- ESP-NOW naar master
- Naam-op-tag storage (schrijven + lezen)

-----

## RTT App (Mobile)

Eerste mobile app project. Stack:

- React Native + Expo SDK
- Supabase backend (EU Frankfurt)
- DM Sans fonts
- Tunnel mode voor development
- Google Calendar sync via Node.js polling script (deployed op Hetzner)
- Leaderboard bridge via `RTT_CloudBridge.h`

-----

## B2B / Commercialisatie

### Doelmarkten

- Nationale Politie (Politieacademie Ossendrecht, regionale eenheden, AT's)
- Defensie (KCT Roosendaal, Infanterie Harskamp, Mariniers, DMO)
- Ministerie van Binnenlandse Zaken (offertes geproduceerd: RTT-2026-0301/0302)

### Positionering

Het target systeem wordt extern gepositioneerd als **RTT Training Target System (TTS)** — professioneel trainingsplatform, niet als airsoftproduct. Tagline: "Train real. Train smart."

### Documenten geproduceerd

- Technisch datasheet (Engels)
- Product positionering document (Nederlands)
- Offertes Binnenlandse Zaken (tabelvrij typografisch design):
  - Basis: €1.500/dagdeel excl. BTW (faciliteiten + media)
  - Plus: €1.775/dagdeel (+ sleutelhouder en ondersteuning)

### Verkoopmodellen

1. Hardware-only (plug & play)
2. Training package (hardware + installatie + instructie)
3. SaaS/licentie (dashboard, scenario's, data-analyse)

### Volgende stappen

- NIDV aanmelding (defensietoeleverancier)
- LinkedIn outreach naar schiet-/tactisch instructeurs
- Live demo-scenario voorbereiden (30 min, 6 targets)
- CE/RoHS documentatie starten

-----

## Stijl & Werkwijze

### Code conventions

- Arduino Core 3.x (niet 2.x)
- ESP-NOW op channel 1
- WiFi AP als fallback voor alle subsystemen (password: `12345678`)
- Web UI's: wit/rood/zwart thema, responsive
- Serial baud rate: 115200
- Gebruik `delay()` spaarzaam — FreeRTOS tasks waar mogelijk

### Document conventions

- Offertes: tabelvrij typografisch design
- Datasheets: professioneel, Engelstalig
- Intern: Nederlands

-----

## IP / Security

### ESP32 Security

- Secure Boot en Flash Encryption onderzocht
- Development Mode vs Release Mode trade-offs gedocumenteerd
- Vereist ESP-IDF (niet Arduino IDE alleen)
- Bestaande Arduino code kan als component binnen ESP-IDF draaien
- KRITISCH: signing key backup op 3+ locaties — verlies = permanent bricked

### Hetzner VPS

- Email agent: systemd service (`rtt-email-agent.service`)
- Google Calendar sync: Node.js polling script
- Python automation scripts

-----

## Business Directions (strategisch)

Drie onderzochte richtingen:

1. **RTT Tactical League** — competitieve airsoft league met seizoensgebonden toernooien
2. **RTT GameBox** — B2B hardware + SaaS platform voor trainingscentra
3. **RTT Tactical Academy** — professionele CQB-opleidingen

Tactisch toernooi-concept uitgewerkt: teams van 6, vier disciplines, tweemaandelijks.
