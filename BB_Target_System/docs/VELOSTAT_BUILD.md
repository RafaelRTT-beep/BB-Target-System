# BB HitMap - Velostat Schietdoel Bouwhandleiding

Bouw een schietdoel dat de **exacte inslagpositie** toont op je telefoon of tablet.
Gebruikt een velostat druksensor-matrix met 10mm kopertape en een ESP32.

---

## Wat heb je nodig?

### Electronica
| Component | Aantal | Opmerking |
|-----------|--------|-----------|
| ESP32 WROOM-32 | 1x | Elke ESP32 variant werkt (ook S3) |
| Velostat sheet | 1x | ~15×15 cm, dikte 0.1mm |
| Kopertape 10mm breed | ~3 meter | Geleidend (met geleidende lijm!) |
| Weerstand 10kΩ | 8x | Pull-down voor kolom-pinnen |
| Piezo buzzer (passief) | 1x | Optioneel, voor geluidsfeedback |
| Breadboard + jumper wires | 1 set | Of soldeer op perfboard |
| USB kabel + 5V voeding | 1x | Micro-USB of USB-C (afhankelijk van ESP32) |

### Constructie
| Materiaal | Opmerking |
|-----------|-----------|
| Karton of dun hout | 2x ~18×18 cm, als boven/onderplaat |
| Schuimrubber/foam | ~5mm dik, als achterlaag |
| Tape / lijmpistool | Om alles bij elkaar te houden |
| Optioneel: 3D-print behuizing | Voor nette afwerking |

### Gereedschap
- Soldeerbout + soldeertin
- Schaar / snijmes
- Liniaal
- Multimeter (handig voor testen)

### Geschatte kosten: **€15-25**

---

## Stap 1: Kopertape Strips Snijden

Snijd **16 strips** kopertape van elk **~14 cm** lang:
- **8 strips** voor de rijen (horizontaal)
- **8 strips** voor de kolommen (verticaal)

```
Elke strip: 10mm breed × 140mm lang
Laat ~20mm extra aan een kant voor aansluiting
```

---

## Stap 2: Rij-strips Plakken (Bovenkant)

Plak 8 horizontale strips op de **bovenkant** van het velostat vel:

```
  ┌─────────────────────────┐
  │ ═══════════════════════ │ ← Rij 0 (kopertape, 10mm)
  │         5mm gap         │
  │ ═══════════════════════ │ ← Rij 1
  │         5mm gap         │
  │ ═══════════════════════ │ ← Rij 2
  │         5mm gap         │
  │ ═══════════════════════ │ ← Rij 3
  │         5mm gap         │
  │ ═══════════════════════ │ ← Rij 4
  │         5mm gap         │
  │ ═══════════════════════ │ ← Rij 5
  │         5mm gap         │
  │ ═══════════════════════ │ ← Rij 6
  │         5mm gap         │
  │ ═══════════════════════ │ ← Rij 7
  └─────────────────────────┘
       BOVENKANT VELOSTAT
```

**Belangrijk:**
- 10mm tape + 5mm tussenruimte = 15mm per rij
- Totale hoogte: 8 × 15mm - 5mm = 115mm ≈ 12cm
- Laat de strips aan de linkerkant ~20mm uitsteken voor bedrading

---

## Stap 3: Kolom-strips Plakken (Onderkant)

Draai het velostat vel om. Plak 8 **verticale** strips op de onderkant:

```
  ┌─────────────────────────┐
  │ ║   ║   ║   ║   ║   ║  ║   ║ │
  │ ║   ║   ║   ║   ║   ║  ║   ║ │
  │ ║   ║   ║   ║   ║   ║  ║   ║ │
  │ ║   ║   ║   ║   ║   ║  ║   ║ │
  │ ║   ║   ║   ║   ║   ║  ║   ║ │
  │ ║   ║   ║   ║   ║   ║  ║   ║ │
  │ K0 K1 K2 K3 K4 K5 K6 K7│
  └─────────────────────────┘
       ONDERKANT VELOSTAT
```

De kolom-strips staan **loodrecht** op de rij-strips.
Laat ze aan de onderkant ~20mm uitsteken voor bedrading.

---

## Stap 4: De Sandwich

Het doel is een "sandwich" structuur:

```
  Bovenplaat (karton)      ← Bescherming / schietoppervlak
  ────────────────────
  Rij-strips (kopertape)   ← Bovenkant velostat
  ════════════════════
  VELOSTAT SHEET            ← Drukgevoelig materiaal
  ════════════════════
  Kolom-strips (kopertape) ← Onderkant velostat
  ────────────────────
  Schuimrubber              ← Zachte achterlaag
  ────────────────────
  Achterplaat (karton)     ← Structuur
```

**Tip:** Het schuimrubber zorgt ervoor dat het velostat terugveert na een inslag.

---

## Stap 5: ESP32 Bedrading

### Rij-pinnen (OUTPUT) → Bovenkant kopertape

| Rij | ESP32 Pin | Kopertape strip |
|-----|-----------|-----------------|
| 0   | GPIO 4    | Bovenste strip  |
| 1   | GPIO 16   | 2e strip        |
| 2   | GPIO 17   | 3e strip        |
| 3   | GPIO 5    | 4e strip        |
| 4   | GPIO 18   | 5e strip        |
| 5   | GPIO 19   | 6e strip        |
| 6   | GPIO 21   | 7e strip        |
| 7   | GPIO 22   | Onderste strip  |

### Kolom-pinnen (ADC INPUT) → Onderkant kopertape

| Kolom | ESP32 Pin | Kopertape strip | Pull-down |
|-------|-----------|-----------------|-----------|
| 0     | GPIO 36 (VP)  | Linker strip  | 10kΩ → GND |
| 1     | GPIO 39 (VN)  | 2e strip      | 10kΩ → GND |
| 2     | GPIO 34       | 3e strip      | 10kΩ → GND |
| 3     | GPIO 35       | 4e strip      | 10kΩ → GND |
| 4     | GPIO 32       | 5e strip      | 10kΩ → GND |
| 5     | GPIO 33       | 6e strip      | 10kΩ → GND |
| 6     | GPIO 25       | 7e strip      | 10kΩ → GND |
| 7     | GPIO 26       | Rechter strip | 10kΩ → GND |

### Buzzer (optioneel)

| Component | ESP32 Pin |
|-----------|-----------|
| Buzzer +  | GPIO 27   |
| Buzzer -  | GND       |

### Schema

```
                      ESP32 WROOM-32
                    ┌────────────────┐
  Rij 0 ─────────── │ GPIO 4         │
  Rij 1 ─────────── │ GPIO 16        │
  Rij 2 ─────────── │ GPIO 17        │
  Rij 3 ─────────── │ GPIO 5         │
  Rij 4 ─────────── │ GPIO 18        │
  Rij 5 ─────────── │ GPIO 19        │
  Rij 6 ─────────── │ GPIO 21        │
  Rij 7 ─────────── │ GPIO 22        │
                     │                │
  Kolom 0 ─┬─10kΩ── │ GPIO 36 (VP)   │
            └─ GND   │                │
  Kolom 1 ─┬─10kΩ── │ GPIO 39 (VN)   │
            └─ GND   │                │
  Kolom 2 ─┬─10kΩ── │ GPIO 34        │
            └─ GND   │                │
  Kolom 3 ─┬─10kΩ── │ GPIO 35        │
            └─ GND   │                │
  Kolom 4 ─┬─10kΩ── │ GPIO 32        │
            └─ GND   │                │
  Kolom 5 ─┬─10kΩ── │ GPIO 33        │
            └─ GND   │                │
  Kolom 6 ─┬─10kΩ── │ GPIO 25        │
            └─ GND   │                │
  Kolom 7 ─┬─10kΩ── │ GPIO 26        │
            └─ GND   │                │
  Buzzer + ───────── │ GPIO 27        │
  Buzzer - ───────── │ GND            │
                     │                │
             USB ─── │ 5V / GND       │
                    └────────────────┘
```

**Let op:**
- Kolom-pinnen gebruiken **ADC1** (GPIO 32-39, 25, 26). ADC2 werkt NIET samen met WiFi!
- Elke kolom-pin heeft een **10kΩ pull-down weerstand** naar GND nodig
- Soldeer draden aan de uitstekende kopertape strips

---

## Stap 6: Software Uploaden

### Arduino IDE Voorbereiden

1. Installeer [Arduino IDE 2.x](https://www.arduino.cc/en/software)
2. Voeg ESP32 board toe:
   - Ga naar **File → Preferences**
   - Board Manager URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Installeer **esp32 by Espressif Systems**
3. Installeer libraries via **Library Manager**:
   - `WebSockets` by Markus Sattler (versie 2.4.0+)

### Uploaden

1. Open `velostat_target/velostat_target.ino`
2. Selecteer board: **ESP32 Dev Module**
3. Selecteer juiste COM-poort
4. Klik **Upload**

---

## Stap 7: Gebruiken

1. **Voeding aansluiten** op de ESP32 (USB of 5V adapter)
2. **Wacht** tot je de opstarttonen hoort (3 piepjes)
3. Op je telefoon/tablet:
   - Verbind met WiFi: **BB_HitMap** (wachtwoord: `shoot2score`)
   - Open browser → ga naar **http://192.168.4.1**
4. **Schiet** op het doelwit
5. De inslagpositie verschijnt direct op je scherm!

### Interface functies

| Functie | Beschrijving |
|---------|-------------|
| Schoten teller | Totaal aantal gedetecteerde schoten |
| Laatste positie | X,Y van laatste inslag in mm |
| Groepering | Diameter van je schotgroep |
| Gem. Afwijking | Gemiddelde afstand van centrum |
| Spread X/Y | Horizontale/verticale spreiding |
| Reset | Wis alle hits en begin opnieuw |
| Kalibreer | Herkalibreer de sensor (niet aanraken!) |
| Pauze | Pauzeer/hervat detectie |
| Hit Log | Chronologisch overzicht van alle schoten |

---

## Kalibratie & Tips

### Gevoeligheid aanpassen

In `config.h`:
```cpp
#define HIT_THRESHOLD  300   // Verhoog als er vals-positieven zijn
                              // Verlaag als hits gemist worden
#define NOISE_FLOOR    50    // Verhoog bij veel ruis
#define DEBOUNCE_MS    150   // Verhoog als dubbele hits voorkomen
```

### Beste resultaten

- **Stevige ondergrond**: Plak het doel op een plank of muur
- **Schuimrubber**: Essentieel voor goede terugvering
- **Kopertape**: Gebruik tape met **geleidende lijmlaag**!
  (Niet alle kopertape geleidt door de lijm heen)
- **Druk**: Velostat heeft redelijke druk nodig - BB's op korte afstand werken het best
- **Afstand**: Optimaal 3-10 meter, afhankelijk van je wapen

### Probleemoplossing

| Probleem | Oplossing |
|----------|-----------|
| Geen hits gedetecteerd | Verlaag `HIT_THRESHOLD` (bv. 100), controleer bedrading |
| Valse hits | Verhoog `HIT_THRESHOLD`, controleer pull-down weerstanden |
| Onnauwkeurige positie | Controleer of strips goed contact maken met velostat |
| WiFi verbindt niet | Controleer of ESP32 opstart (Serial Monitor 115200 baud) |
| Dubbele hits | Verhoog `DEBOUNCE_MS` (bv. 250) |

---

## Hoe Werkt Het?

### Velostat als Druksensor

Velostat is een drukgevoelig geleidend materiaal. In rust heeft het een **hoge weerstand**
(tientallen kΩ). Bij druk daalt de weerstand sterk (tot honderden Ω).

### Matrix Scanning

```
Rij-strips (bovenkant)     Kolom-strips (onderkant)
   ══════════                   ║  ║  ║
   ══════════                   ║  ║  ║
   ══════════                   ║  ║  ║

             ╲               ╱
              ╲             ╱
               ╲           ╱
            ┌─── VELOSTAT ───┐
            │                 │
            │  Elk kruispunt  │
            │  = 1 meetpunt   │
            │  (8×8 = 64)    │
            └─────────────────┘
```

De ESP32 scant de matrix als volgt:
1. Zet rij 0 op HIGH (3.3V), alle andere op LOW
2. Lees alle 8 kolommen via ADC
3. Herhaal voor elke rij

Bij een inslag wordt de velostat op dat punt ingedrukt → weerstand daalt →
stroom vloeit van rij naar kolom → ADC meet hogere spanning.

### Centroid Berekening

Voor nauwkeurigheid gebruiken we een **gewogen gemiddelde** (centroid) van alle
meetpunten boven de ruis-drempel. Dit geeft sub-cel nauwkeurigheid (~2-3mm).
