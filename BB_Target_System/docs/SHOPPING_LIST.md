# RAF RTT Fysiek - Componenten Bestellijst

## Voor 8 Targets + 1 Master Controller

### Microcontrollers

| Component | Aantal | Link voorbeeld | Prijs (circa) |
|-----------|--------|----------------|---------------|
| ESP32-S3 DevKit | 1 | AliExpress/Amazon | €10-15 |
| ESP32 WROOM-32 (38 pin) | 8 | AliExpress bulk | €5-8 per stuk |

**Tip:** Koop 1-2 extra ESP32's als backup

---

### Sensoren & Actuatoren

| Component | Aantal | Specificatie | Prijs (circa) |
|-----------|--------|--------------|---------------|
| Piezo element 27mm | 8 | Keramisch, met kabels | €0.50-2 per stuk |
| WS2812B LED strip | 2m | 60 LED/m, IP30 | €8-15 |
| Actieve buzzer 5V | 8 | Met header pins | €0.50-1 per stuk |

**LED Strip Berekening:**
- 12 LEDs per target × 8 targets = 96 LEDs
- Bij 60 LED/m = 1.6 meter nodig
- Bestel 2 meter voor speling

---

### Weerstanden & Condensatoren

| Component | Aantal | Specificatie | Prijs |
|-----------|--------|--------------|-------|
| Weerstand 1MΩ | 10 | 1/4W | €0.10 |
| Weerstand 330Ω | 10 | 1/4W | €0.10 |
| Condensator 1000µF | 8 | 16V elektrolytisch | €0.30 per stuk |

---

### Kabels & Connectoren

| Component | Aantal | Specificatie | Prijs |
|-----------|--------|--------------|-------|
| Dupont kabels M-F | 40 stuks | 20cm | €3 |
| Dupont kabels F-F | 40 stuks | 20cm | €3 |
| Siliconenkabel | 5m | 22AWG, rood | €4 |
| Siliconenkabel | 5m | 22AWG, zwart | €4 |
| Siliconenkabel | 5m | 22AWG, geel | €4 |
| JST connector set | 1 | SM 2-pin | €5 |
| Krimpkous set | 1 | Diverse maten | €5 |

---

### Voeding

| Component | Aantal | Specificatie | Prijs |
|-----------|--------|--------------|-------|
| 5V 2A adapter | 8 | Barrel jack 5.5×2.1mm | €3-5 per stuk |
| OF: 5V 10A voeding | 1 | Centrale voeding | €15-25 |
| USB-C kabel | 1 | Voor ESP32-S3 | €3 |
| Barrel jack splitter | - | Optioneel | €5 |

**Optie A:** Individuele adapters (makkelijker, flexibeler)
**Optie B:** Centrale 5V 10A voeding (goedkoper, netter)

---

### Behuizingen

| Component | Aantal | Specificatie | Prijs |
|-----------|--------|--------------|-------|
| Projectbox 100×60×25mm | 8 | ABS kunststof | €2-4 per stuk |
| OF: 3D print filament | 1kg | PLA/PETG | €20 |
| Kabeldoorvoer | 16 | PG7 of rubber | €0.50 per stuk |
| M3 schroeven + moeren | Set | Diverse lengtes | €5 |
| Afstandhouders M3 | 32 | 6mm hoogte | €3 |

---

### Metalen Doelplaten

| Component | Aantal | Specificatie | Prijs |
|-----------|--------|--------------|-------|
| Staalplaat 4mm | 8 | ~20×20cm per target | Lokale metaalhandel |
| OF: Aluminium 4mm | 8 | Lichter, zachter geluid | €5-10 per stuk |

**Tips:**
- Vraag lokale metaalhandel voor restanten
- Laat gaten boren voor montage
- Afwerken met schuurpapier (geen scherpe randen)

---

### Gereedschap (eenmalig)

| Item | Nodig? | Prijs |
|------|--------|-------|
| Soldeerbout + tin | Ja | €20-40 |
| Multimeter | Ja | €15-30 |
| Striptang | Ja | €10 |
| Derde hand / klem | Handig | €10 |
| Schroevendraaier set | Ja | €10 |
| Boormachine + bits | Voor metaal | - |

---

## Totale Kostenschatting

### Budget Versie (8 targets)

| Categorie | Prijs |
|-----------|-------|
| Microcontrollers | €55-75 |
| Sensoren & LEDs | €30-45 |
| Elektronica | €15-20 |
| Kabels | €25-30 |
| Voeding | €30-45 |
| Behuizingen | €20-35 |
| **Totaal** | **€175-250** |

### Premium Versie (beter/mooier)

| Upgrade | Extra kosten |
|---------|--------------|
| IP65 LED strips | +€20 |
| Centrale 10A voeding | +€10 |
| 3D geprinte behuizingen | +€20 |
| Betere connectoren | +€15 |
| **Totaal** | **€240-315** |

---

## Bestellen Tips

### AliExpress (goedkoopst, 2-4 weken)
- ESP32 boards
- LED strips
- Piezo's
- Connectoren
- Kabels

### Amazon/Bol.com (sneller, iets duurder)
- Voedingen (kwaliteit belangrijk)
- Gereedschap
- Behuizingen

### Lokaal (direct beschikbaar)
- Weerstanden/condensatoren bij Conrad/Kijkshop
- Metalen platen bij metaalhandel
- Gereedschap bij bouwmarkt

---

## Gedetailleerde AliExpress Links (zoektermen)

```
ESP32 WROOM-32 development board 38 pin
ESP32-S3 DevKitC-1 N8R2
WS2812B LED strip 5V 60 LED/m IP30
Piezo disc 27mm element buzzer
Active buzzer module 5V
1M ohm resistor 1/4W
Dupont jumper wire female male 20cm
5V 2A power adapter DC 5.5x2.1
Project box enclosure 100x60x25
```

---

## Stap-voor-Stap Bestelvolgorde

1. **Week 1:** Bestel microcontrollers en basis elektronica
2. **Week 2:** Test eerste ESP32 met simpele code
3. **Week 3:** Bestel rest als test succesvol
4. **Week 4-5:** Assemblage terwijl spullen binnenkomen

Zo voorkom je dat je veel geld uitgeeft aan iets dat niet werkt!
