# RTT Target System - Bedradingsschema

## Benodigde Materialen

### Per Target Node
| Component | Aantal | Specificatie | Geschatte prijs |
|-----------|--------|--------------|-----------------|
| ESP32 WROOM-32 | 1 | 38-pin development board | €6-10 |
| Piezo sensor | 1 | 27mm keramisch element | €0.50-2 |
| WS2812B LED strip | 1 | 12-16 LEDs, 5V | €3-5 |
| Actieve buzzer | 1 | 5V | €0.50-1 |
| 1MΩ weerstand | 1 | Voor piezo | €0.05 |
| Voeding | 1 | 5V 2A adapter | €5-8 |
| Behuizing | 1 | 3D print of projectbox | €3-10 |

### Master Controller
| Component | Aantal | Specificatie | Geschatte prijs |
|-----------|--------|--------------|-----------------|
| ESP32-S3 | 1 | Development board | €8-15 |
| Voeding | 1 | 5V 1A (USB) | €3-5 |

### Gereedschap
- Soldeerbout + soldeertin
- Krimpkous of isolatietape
- Multimeter
- Kabels (verschillende kleuren)

---

## Target Node Bedrading

```
                         ┌─────────────────────────────────────┐
                         │          ESP32 WROOM-32             │
                         │                                     │
    ┌────────────┐       │   3V3 ──┐                          │
    │   PIEZO    │       │         │                          │
    │  SENSOR    │       │  GPIO34 ├─────┬───────┐            │
    │   (+)──────┼───────┤  (ADC)  │     │       │            │
    │   (-)──────┼───┬───┤   GND   │    1MΩ     PIEZO        │
    └────────────┘   │   │         │     │       │            │
                     │   │         │     │       │            │
                    GND  │         │    GND     GND           │
                         │                                     │
    ┌────────────┐       │                                     │
    │  WS2812B   │       │                                     │
    │ LED STRIP  │       │                                     │
    │            │       │                                     │
    │   DIN ─────┼───────┤  GPIO13                            │
    │   VCC ─────┼───────┤  5V (VIN)                          │
    │   GND ─────┼───────┤  GND                               │
    └────────────┘       │                                     │
                         │                                     │
    ┌────────────┐       │                                     │
    │  BUZZER    │       │                                     │
    │  (Actief)  │       │                                     │
    │   (+) ─────┼───────┤  GPIO26                            │
    │   (-) ─────┼───────┤  GND                               │
    └────────────┘       │                                     │
                         │                                     │
    ┌────────────┐       │                                     │
    │  5V 2A     │       │                                     │
    │  VOEDING   │       │                                     │
    │   (+) ─────┼───────┤  VIN (5V)                          │
    │   (-) ─────┼───────┤  GND                               │
    └────────────┘       └─────────────────────────────────────┘
```

### Piezo Sensor Montage op Metalen Plaat

```
    ┌─────────────────────────────────────────┐
    │          METALEN PLAAT (4mm)            │
    │                                         │
    │    ┌───────────────────────────┐        │
    │    │                           │        │
    │    │     ╭─────────────╮       │        │
    │    │     │             │       │        │
    │    │     │   PIEZO     │       │        │
    │    │     │   27mm      │       │        │
    │    │     │             │       │        │
    │    │     ╰──────┬──────╯       │        │
    │    │            │              │        │
    │    │         Lijm/Tape         │        │
    │    │      (contactcement)      │        │
    │    └───────────────────────────┘        │
    │                                         │
    │         ↓ BB Impact Zone ↓              │
    │                                         │
    └─────────────────────────────────────────┘

    BELANGRIJK:
    - Plak piezo aan ACHTERKANT van plaat
    - Gebruik contactcement of 2-componentenlijm
    - Zorg voor goede mechanische koppeling
    - Bescherm piezo met schuimrubber aan buitenkant
```

### ESP32 Pinout Referentie

```
                    ESP32 WROOM-32 (38 pins)

              ┌──────────────────────────────┐
              │                              │
        EN ───┤ 1                        38 ├─── GPIO23
      GPIO36 ─┤ 2                        37 ├─── GPIO22
      GPIO39 ─┤ 3                        36 ├─── GPIO1 (TX)
    * GPIO34 ─┤ 4  ← PIEZO              35 ├─── GPIO3 (RX)
      GPIO35 ─┤ 5                        34 ├─── GPIO21
      GPIO32 ─┤ 6                        33 ├─── GND
      GPIO33 ─┤ 7                        32 ├─── GPIO19
      GPIO25 ─┤ 8                        31 ├─── GPIO18
    * GPIO26 ─┤ 9  ← BUZZER             30 ├─── GPIO5
      GPIO27 ─┤ 10                       29 ├─── GPIO17
      GPIO14 ─┤ 11                       28 ├─── GPIO16
      GPIO12 ─┤ 12                       27 ├─── GPIO4
    * GND ────┤ 13                       26 ├─── GPIO0
    * GPIO13 ─┤ 14 ← LED STRIP          25 ├─── GPIO2 (LED)
      GPIO9  ─┤ 15                       24 ├─── GPIO15
      GPIO10 ─┤ 16                       23 ├─── GPIO8
      GPIO11 ─┤ 17                       22 ├─── GPIO7
    * VIN ────┤ 18 ← 5V VOEDING         21 ├─── GPIO6
    * GND ────┤ 19                       20 ├─── 3V3
              │                              │
              └──────────────────────────────┘

    * = Gebruikte pins (markeer met kleur)
```

---

## Master Controller Bedrading

De Master Controller (ESP32-S3) heeft alleen voeding nodig:

```
    ┌─────────────────────────────────────┐
    │          ESP32-S3                   │
    │                                     │
    │   USB-C ──── Computer/Adapter       │
    │   (5V voeding + programmeren)       │
    │                                     │
    │   Optioneel: Externe antenne        │
    │   voor beter WiFi bereik            │
    │                                     │
    └─────────────────────────────────────┘
```

---

## Kleurcodering Kabels

Gebruik consistente kleuren voor makkelijker debuggen:

| Kleur | Functie |
|-------|---------|
| **Rood** | 5V / VCC |
| **Zwart** | GND |
| **Geel** | Data signaal (LED, Piezo) |
| **Groen** | Buzzer |
| **Blauw** | I2C / Extra |

---

## LED Strip Aansluiting Detail

```
    WS2812B LED Strip

    ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐
    │ L1 │ L2 │ L3 │ L4 │ L5 │ L6 │ L7 │ L8 │ L9 │L10 │L11 │L12 │
    └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘
      ↑
    INPUT
    (DIN)

    ┌───────────────────────────────────────────────────────────┐
    │                                                           │
    │   DIN ────────── GPIO13 (ESP32)                          │
    │   VCC ────────── 5V                                      │
    │   GND ────────── GND                                     │
    │                                                           │
    │   OPTIONEEL: 1000µF condensator tussen VCC en GND        │
    │              (vlak bij eerste LED)                        │
    │                                                           │
    │   OPTIONEEL: 330Ω weerstand in DIN lijn                  │
    │              (beschermt tegen spanningspieken)            │
    │                                                           │
    └───────────────────────────────────────────────────────────┘
```

---

## Piezo Sensor Circuit

```
                    1MΩ
    GPIO34 ──────┬──/\/\/──┬────── GND
                 │         │
                 │    ┌────┴────┐
                 │    │         │
                 └────┤  PIEZO  │
                      │         │
                      └────┬────┘
                           │
                          GND

    WAAROM 1MΩ WEERSTAND?
    - Voorkomt dat piezo te lang geladen blijft
    - Zorgt voor snelle reset na hit
    - Beschermt ESP32 ADC input

    ALTERNATIEF (gevoeliger):
    - Gebruik 10MΩ voor hogere gevoeligheid
    - Voeg 3.3V zener diode toe voor bescherming
```

---

## Complete Target Assembly

```
    ┌─────────────────────────────────────────────────────────────┐
    │                                                             │
    │                    PROJECTBOX                               │
    │                                                             │
    │   ┌───────────────────────────────────────────────────┐    │
    │   │                 ESP32 Board                       │    │
    │   │   ┌─────┐                          ┌─────┐        │    │
    │   │   │USB  │                          │ LED │        │    │
    │   │   └──┬──┘                          └──┬──┘        │    │
    │   │      │                                │           │    │
    │   └──────┼────────────────────────────────┼───────────┘    │
    │          │                                │                 │
    │          │    ┌──────────────────────┐   │                 │
    │          │    │      BUZZER          │   │                 │
    │          │    │        ◉             │   │                 │
    │          │    └──────────────────────┘   │                 │
    │          │                                │                 │
    │   ───────┴────────────────────────────────┴─────────────   │
    │                        KABELS                               │
    │                          │                                  │
    └──────────────────────────┼──────────────────────────────────┘
                               │
                               │ Naar metalen plaat
                               │
    ┌──────────────────────────┼──────────────────────────────────┐
    │                          │                                  │
    │                      ┌───┴───┐                              │
    │                      │ PIEZO │                              │
    │                      └───────┘                              │
    │                                                             │
    │              METALEN DOELPLAAT (4mm)                        │
    │                                                             │
    │   ┌─────────────────────────────────────────────────────┐  │
    │   │                                                     │  │
    │   │               LED STRIP (rond/vierkant)             │  │
    │   │                                                     │  │
    │   │      ╔═══════════════════════════════════╗          │  │
    │   │      ║                                   ║          │  │
    │   │      ║         DOELGEBIED                ║          │  │
    │   │      ║                                   ║          │  │
    │   │      ╚═══════════════════════════════════╝          │  │
    │   │                                                     │  │
    │   └─────────────────────────────────────────────────────┘  │
    │                                                             │
    └─────────────────────────────────────────────────────────────┘
```

---

## Voeding Berekening

### Per Target
| Component | Stroomverbruik |
|-----------|----------------|
| ESP32 (actief) | ~80mA |
| WS2812B (12 LEDs, max) | 12 × 60mA = 720mA |
| Buzzer | ~30mA |
| **Totaal max** | **~830mA** |
| **Praktisch** | **~400mA** (LEDs niet op volle sterkte) |

**Aanbevolen:** 5V 2A adapter per target

### Hele Systeem (6 targets)
- Targets: 6 × 400mA = 2.4A
- Master: ~200mA
- **Totaal:** ~2.6A @ 5V

---

## Troubleshooting

### LED Strip werkt niet
1. Check of DIN naar GPIO13 gaat (niet GND)
2. Check voeding (5V direct, niet via ESP32)
3. Test met simpel blink script

### Piezo detecteert niet
1. Check weerstand (1MΩ)
2. Verhoog `PIEZO_THRESHOLD` in config.h
3. Test met `Serial.println(analogRead(34));`

### Geen verbinding met Master
1. Check WiFi kanaal (moet gelijk zijn)
2. Check MAC adres in Serial Monitor
3. Verminder afstand voor testen

### Buzzer geeft geen geluid
1. Check of het een ACTIEVE buzzer is
2. Check GPIO26 verbinding
3. Test met `digitalWrite(26, HIGH);`
