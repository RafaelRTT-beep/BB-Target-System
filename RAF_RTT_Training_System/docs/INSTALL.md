# RAF RTT Training System - Installatie Handleiding

## Overzicht

Dit document beschrijft de complete installatie van het RAF RTT Training System, van software setup tot eerste gebruik.

---

## Deel 1: Software Installatie

### 1.1 Arduino IDE Installeren

1. Download Arduino IDE 2.x van https://www.arduino.cc/en/software
2. Installeer en start Arduino IDE

### 1.2 ESP32 Boards Toevoegen

1. Open Arduino IDE
2. Ga naar **File → Preferences**
3. Plak bij "Additional boards manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Klik **OK**
5. Ga naar **Tools → Board → Boards Manager**
6. Zoek "esp32" en installeer **esp32 by Espressif Systems**

### 1.3 Benodigde Libraries Installeren

Ga naar **Sketch → Include Library → Manage Libraries** en installeer:

| Library | Versie | Voor |
|---------|--------|------|
| FastLED | 3.6+ | LED strip aansturing |
| ArduinoJson | 7.x | JSON parsing |
| ESPAsyncWebServer | - | Webserver |
| AsyncTCP | - | TCP voor webserver |

**AsyncTCP en ESPAsyncWebServer handmatig installeren:**

1. Download van:
   - https://github.com/me-no-dev/AsyncTCP
   - https://github.com/me-no-dev/ESPAsyncWebServer

2. In Arduino IDE: **Sketch → Include Library → Add .ZIP Library**
3. Selecteer gedownloade ZIP bestanden

---

## Deel 2: Master Controller Programmeren

### 2.1 Board Selecteren

1. Verbind ESP32-S3 via USB
2. Ga naar **Tools → Board → esp32**
3. Selecteer **ESP32S3 Dev Module**
4. Selecteer juiste COM poort bij **Tools → Port**

### 2.2 Board Instellingen

```
Board: ESP32S3 Dev Module
Upload Speed: 921600
USB Mode: Hardware CDC and JTAG
USB CDC On Boot: Enabled
Flash Size: 8MB (of wat je hebt)
Partition Scheme: Default 4MB with spiffs
PSRAM: Disabled (of OPI PSRAM als aanwezig)
```

### 2.3 Code Uploaden

1. Open `master_controller/master_controller.ino`
2. Pas indien nodig `config.h` aan:
   - WiFi naam en wachtwoord
   - Game instellingen
3. Klik **Upload** (→ pijl)
4. Wacht tot "Done uploading" verschijnt

### 2.4 Master Testen

1. Open **Serial Monitor** (Tools → Serial Monitor)
2. Zet baudrate op **115200**
3. Je zou moeten zien:

```
============================================
   RAF RTT TRAINING SYSTEM - MASTER CONTROLLER
============================================

WiFi Access Point starten...
AP IP: 192.168.4.1
MAC: XX:XX:XX:XX:XX:XX
ESP-NOW geinitialiseerd
Webserver gestart op poort 80
Verbind met WiFi: RAF RTT Training
Open browser: http://192.168.4.1

Master Controller gereed!
Wacht op targets...
```

---

## Deel 3: Target Nodes Programmeren

### 3.1 Board Selecteren

1. Verbind ESP32 (WROOM-32) via USB
2. Ga naar **Tools → Board → esp32**
3. Selecteer **ESP32 Dev Module**
4. Selecteer juiste COM poort

### 3.2 Board Instellingen

```
Board: ESP32 Dev Module
Upload Speed: 921600
Flash Frequency: 80MHz
Flash Mode: QIO
Flash Size: 4MB
Partition Scheme: Default 4MB with spiffs
```

### 3.3 TARGET_ID Aanpassen

**BELANGRIJK:** Elk target moet een uniek ID hebben!

1. Open `target_node/config.h`
2. Pas regel aan:
   ```cpp
   #define TARGET_ID   1   // Verander naar 1, 2, 3, ... 8
   ```
3. Sla op

### 3.4 Code Uploaden

1. Open `target_node/target_node.ino`
2. Klik **Upload**
3. Herhaal voor elk target met ander TARGET_ID

### 3.5 Target Testen

Na upload zie je in Serial Monitor:

```
============================================
   RAF RTT TRAINING SYSTEM - TARGET NODE
   Target ID: 1
============================================

MAC Adres: XX:XX:XX:XX:XX:XX
ESP-NOW geinitialiseerd

Target Node gereed!
Wacht op master controller...

Heartbeat verzonden - Target 1, Status: 0
```

---

## Deel 4: Hardware Assemblage

### 4.1 Piezo Sensor Monteren

1. **Oppervlak voorbereiden:**
   - Maak metalen plaat schoon met alcohol
   - Markeer midden van plaat

2. **Piezo plakken:**
   - Gebruik contactcement of 2-componenten epoxy
   - Breng lijm aan op ACHTERKANT van plaat
   - Druk piezo stevig aan (30 sec)
   - Laat 24 uur uitharden

3. **Bedrading:**
   - Soldeer kabels aan piezo (+ en -)
   - Gebruik krimpkous voor isolatie

### 4.2 LED Strip Monteren

1. **Strip knippen:**
   - Knip op aangegeven snijlijnen
   - 12 LEDs is ideaal voor middelgroot target

2. **Bedrading:**
   - Soldeer kabels aan: DIN, VCC, GND
   - Voeg 330Ω weerstand toe in DIN lijn (optioneel)

3. **Monteren:**
   - Plak rond of naast doelgebied
   - LED kant naar schieter toe

### 4.3 ESP32 Aansluiten

Volg het bedradingsschema in `WIRING.md`:

| ESP32 Pin | Component |
|-----------|-----------|
| GPIO34 | Piezo + |
| GPIO13 | LED Strip DIN |
| GPIO26 | Buzzer + |
| VIN | 5V voeding |
| GND | Alle GND |

### 4.4 Behuizing

1. Gebruik projectbox of 3D-geprint behuizing
2. Maak gaten voor:
   - USB poort (optioneel)
   - Kabels naar piezo/LED
   - Ventilatie
3. Monteer ESP32 met afstandhouders

---

## Deel 5: Systeem Testen

### 5.1 Verbinding Testen

1. Zet Master Controller aan
2. Zet Target Node aan
3. Check Master Serial Monitor:
   ```
   Target 1 online! Totaal: 1
   ```

### 5.2 Webinterface Testen

1. Verbind telefoon/laptop met WiFi: **RAF RTT Training**
2. Wachtwoord: **shoot2score**
3. Open browser: **http://192.168.4.1**
4. Je ziet het dashboard met targets

### 5.3 Hit Detectie Testen

1. Klik in webinterface op target nummer → LED gaat aan
2. Tik op metalen plaat → Buzzer piept, LED flasht
3. Check score in webinterface

### 5.4 Game Modi Testen

Test elke modus kort:

1. **Free Play:** Alle targets actief, scoor bij elke hit
2. **Sequence:** Targets lichten 1 voor 1 op in volgorde
3. **Random:** Willekeurig target licht op
4. **Shoot/No Shoot:** Groene targets = goed, rode = fout

---

## Deel 6: Kalibratie

### 6.1 Piezo Gevoeligheid

Als hits niet gedetecteerd worden of false positives:

1. Open `target_node/config.h`
2. Pas `PIEZO_THRESHOLD` aan:
   ```cpp
   #define PIEZO_THRESHOLD   150   // Verhoog voor minder gevoelig
   ```
3. Test waarden: 100 (gevoelig) tot 500 (minder gevoelig)
4. Upload opnieuw

### 6.2 LED Helderheid

```cpp
#define LED_BRIGHTNESS   150   // 0-255
```

### 6.3 Debounce Tijd

Als dubbele hits geregistreerd worden:

```cpp
#define PIEZO_DEBOUNCE_MS   100   // Verhoog naar 150-200
```

---

## Deel 7: Gebruik

### 7.1 Opstarten

1. Zet alle apparaten aan
2. Wacht 10 seconden
3. Verbind met WiFi
4. Open webinterface

### 7.2 Game Starten

1. Selecteer **Game Modus**
2. Stel **Speeltijd** in
3. Vul **Speler Naam** in
4. Klik **START**

### 7.3 Keyboard Shortcuts

| Toets | Actie |
|-------|-------|
| Spatie | Start/Pauze |
| Escape | Stop |
| R | Reset |
| 1-8 | Activeer target |
| T | TV Mode |

### 7.4 TV Weergave

1. Verbind laptop met TV via HDMI
2. Open webinterface in browser
3. Druk **T** of klik **TV Mode**
4. Scherm schaalt automatisch

---

## Troubleshooting

### Target komt niet online

1. Check voeding (5V stabiel?)
2. Check Serial Monitor voor errors
3. Verklein afstand tot master
4. Herstart beide apparaten

### WebSocket verbinding faalt

1. Refresh browser pagina
2. Check WiFi verbinding
3. Herstart Master Controller

### Hits worden niet gedetecteerd

1. Test piezo met multimeter (weerstand mode)
2. Check bedrading
3. Verhoog gevoeligheid in config

### LED strip flikkert

1. Voeg 1000µF condensator toe
2. Check voeding capaciteit
3. Kort kabels in

---

## Onderhoud

### Wekelijks
- Controleer alle verbindingen
- Test alle targets kort

### Maandelijks
- Schoonmaken metalen platen
- Check piezo bevestiging
- Update firmware indien nodig

### Bij Problemen
- Backup highscores (voordat je reset)
- Factory reset via Serial: stuur "RESET"
- Herinstalleer firmware

---

## Vragen?

Bekijk de code comments voor technische details of pas de code aan naar je eigen wensen. Het systeem is modulair opgezet zodat je makkelijk functies kunt toevoegen of aanpassen.
