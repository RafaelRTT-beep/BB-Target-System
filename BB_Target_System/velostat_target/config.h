// ============================================
// BB Target System - Velostat Hit Map Config
// ============================================
// Detecteert WAAR op het doel je hebt geschoten
// met een velostat drukmatrix en kopertape (10mm)
// ============================================

#ifndef CONFIG_H
#define CONFIG_H

// ---- Netwerk Instellingen ----
#define WIFI_SSID         "BB_HitMap"
#define WIFI_PASSWORD     "shoot2score"
#define WEBSOCKET_PORT    81
#define WEBSERVER_PORT    80

// ---- Matrix Afmetingen ----
// 8x8 grid met 10mm kopertape strips
// Geeft een actief meetgebied van ~12x12 cm
#define MATRIX_ROWS       8
#define MATRIX_COLS       8

// ---- Row Pins (OUTPUT - stuur kant) ----
// Deze pinnen sturen de rij-strips aan (bovenkant velostat)
// Gebruik digitale output pins
const int ROW_PINS[MATRIX_ROWS] = {
  4,    // Rij 0 (bovenste strip)
  16,   // Rij 1
  17,   // Rij 2
  5,    // Rij 3
  18,   // Rij 4
  19,   // Rij 5
  21,   // Rij 6
  22    // Rij 7 (onderste strip)
};

// ---- Column Pins (ADC INPUT - lees kant) ----
// Deze pinnen lezen de kolom-strips (onderkant velostat)
// MOET ADC1 pins zijn (ADC2 werkt niet met WiFi!)
// ESP32 ADC1 kanalen: GPIO 32, 33, 34, 35, 36, 39
// Extra: GPIO 25, 26 (DAC pins, ook bruikbaar als ADC)
const int COL_PINS[MATRIX_COLS] = {
  36,   // Kolom 0 (linker strip)  - VP/SVP
  39,   // Kolom 1                 - VN/SVN
  34,   // Kolom 2
  35,   // Kolom 3
  32,   // Kolom 4
  33,   // Kolom 5
  25,   // Kolom 6
  26    // Kolom 7 (rechter strip)
};

// ---- Pull-down Weerstanden ----
// Elke kolom-pin heeft een 10kΩ pull-down weerstand
// nodig naar GND (extern aansluiten!)

// ---- Hit Detectie Instellingen ----
#define HIT_THRESHOLD     300     // ADC waarde (0-4095) drempel voor hit
#define NOISE_FLOOR       50      // Alles onder deze waarde = ruis
#define DEBOUNCE_MS       150     // Tijd tussen hits (ms)
#define SCAN_INTERVAL_US  100     // Microseconden tussen matrix scans
#define ROW_SETTLE_US     50      // Wachttijd na rij activeren (microsec)

// ---- Buzzer (optioneel) ----
#define BUZZER_PIN        27
#define BUZZER_ENABLED    true
#define HIT_TONE_FREQ     2000    // Hz
#define HIT_TONE_DURATION 50      // ms

// ---- Status LED ----
#define STATUS_LED_PIN    2       // Ingebouwde LED

// ---- Hit Geschiedenis ----
#define MAX_HITS          200     // Max aantal hits opslaan
#define HIT_RADIUS_MM     5       // Weergave radius per hit (mm)

// ---- Doelwit Afmetingen ----
// Fysieke grootte van het actieve meetgebied
#define TARGET_WIDTH_MM   120     // 8 strips x 15mm (10mm tape + 5mm gap)
#define TARGET_HEIGHT_MM  120     // 8 strips x 15mm
#define STRIP_WIDTH_MM    10      // Breedte kopertape
#define STRIP_GAP_MM      5       // Ruimte tussen strips

// ---- Debug ----
#define DEBUG_ENABLED     true
#define SERIAL_BAUD       115200

#if DEBUG_ENABLED
  #define DEBUG_PRINT(x)    Serial.print(x)
  #define DEBUG_PRINTLN(x)  Serial.println(x)
  #define DEBUG_PRINTF(...)  Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

#endif // CONFIG_H
