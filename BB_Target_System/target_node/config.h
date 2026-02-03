/**
 * BB Target System - Target Node Configuration
 *
 * Pas deze instellingen aan per target node
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// TARGET IDENTIFICATIE
// ============================================================================
// BELANGRIJK: Pas dit aan voor elk target (1-8)
#define TARGET_ID           1

// ============================================================================
// PIN CONFIGURATIE
// ============================================================================
// Piezo sensor (analoge pin)
#define PIEZO_PIN           34      // ADC1 pin voor piezo sensor

// LED Strip
#define LED_PIN             13      // Data pin voor WS2812B
#define NUM_LEDS            12      // Aantal LEDs per target

// Buzzer
#define BUZZER_PIN          26      // Buzzer output pin

// Status LED (ingebouwde LED)
#define STATUS_LED_PIN      2       // Blauwe LED op ESP32

// ============================================================================
// PIEZO SENSOR INSTELLINGEN
// ============================================================================
#define PIEZO_THRESHOLD     150     // Minimum waarde voor hit detectie (0-4095)
#define PIEZO_DEBOUNCE_MS   100     // Debounce tijd in milliseconden
#define PIEZO_SAMPLE_RATE   1000    // Samples per seconde

// ============================================================================
// LED INSTELLINGEN
// ============================================================================
#define LED_BRIGHTNESS      150     // 0-255
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB

// Standaard kleuren (RGB)
#define COLOR_INACTIVE_R    0
#define COLOR_INACTIVE_G    0
#define COLOR_INACTIVE_B    20      // Dim blauw wanneer inactief

#define COLOR_ACTIVE_R      0
#define COLOR_ACTIVE_G      255
#define COLOR_ACTIVE_B      0       // Groen wanneer actief

#define COLOR_HIT_R         255
#define COLOR_HIT_G         100
#define COLOR_HIT_B         0       // Oranje bij hit

#define COLOR_NOSHOOT_R     255
#define COLOR_NOSHOOT_G     0
#define COLOR_NOSHOOT_B     0       // Rood voor no-shoot

// ============================================================================
// COMMUNICATIE INSTELLINGEN
// ============================================================================
#define HEARTBEAT_INTERVAL  5000    // Heartbeat elke 5 seconden
#define WIFI_CHANNEL        1       // WiFi kanaal voor ESP-NOW

// Master Controller MAC adres (pas aan naar jouw ESP32-S3)
// Gebruik de Serial Monitor om het MAC adres te vinden
uint8_t masterMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Broadcast eerst

// ============================================================================
// GELUID INSTELLINGEN
// ============================================================================
#define SOUND_HIT_FREQ      1000    // Frequentie hit geluid (Hz)
#define SOUND_HIT_DURATION  50      // Duur hit geluid (ms)
#define SOUND_MISS_FREQ     200     // Frequentie miss geluid (Hz)
#define SOUND_MISS_DURATION 200     // Duur miss geluid (ms)

// ============================================================================
// DEBUG INSTELLINGEN
// ============================================================================
#define DEBUG_ENABLED       true    // Serial debug output
#define SERIAL_BAUD         115200  // Serial baudrate

#endif // CONFIG_H
