/**
 * Raf RTT Training System - Ultrasonic Counter Node Configuration
 *
 * Stel deze instellingen in voor de ultrasone teller node
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// NODE IDENTIFICATIE
// ============================================================================
// BELANGRIJK: Gebruik een ID buiten het bereik van target nodes (bijv. 9+)
#define NODE_ID             9

// ============================================================================
// PIN CONFIGURATIE
// ============================================================================
// HC-SR04 Ultrasone Sensor
#define TRIG_PIN            5       // Trigger pin
#define ECHO_PIN            18      // Echo pin

// LED Strip
#define LED_PIN             13      // Data pin voor WS2812B
#define NUM_LEDS            12      // Aantal LEDs op de strip

// Buzzer
#define BUZZER_PIN          26      // Buzzer output pin

// Status LED (ingebouwde LED)
#define STATUS_LED_PIN      2       // Blauwe LED op ESP32

// ============================================================================
// ULTRASONE SENSOR INSTELLINGEN
// ============================================================================
#define DETECTION_DISTANCE_CM   20      // Maximale detectie afstand (cm)
#define DEBOUNCE_MS             500     // Debounce tijd (ms) - voorkomt dubbeltelling
#define MEASUREMENT_INTERVAL_MS 50      // Meet interval (ms) - 20x per seconde
#define MAX_DISTANCE_CM         400     // Maximale meetbereik sensor (cm)

// ============================================================================
// TELLER INSTELLINGEN
// ============================================================================
#define DEFAULT_COUNTS_PER_LED  1       // Standaard: elke detectie = 1 LED
#define MAX_COUNT               9999    // Maximale teller waarde

// ============================================================================
// LED INSTELLINGEN
// ============================================================================
#define LED_BRIGHTNESS      150     // 0-255
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB

// LED kleuren (RGB)
#define COLOR_LED_ON_R      0
#define COLOR_LED_ON_G      255
#define COLOR_LED_ON_B      0       // Groen voor actieve LED

#define COLOR_LED_OFF_R     0
#define COLOR_LED_OFF_G     0
#define COLOR_LED_OFF_B     10      // Dim blauw voor inactieve LED

#define COLOR_FLASH_R       255
#define COLOR_FLASH_G       165
#define COLOR_FLASH_B       0       // Oranje voor detectie flash

#define COLOR_FULL_R        255
#define COLOR_FULL_G        215
#define COLOR_FULL_B        0       // Goud als alle LEDs aan zijn

// ============================================================================
// BUZZER INSTELLINGEN
// ============================================================================
#define BUZZER_FREQ         1500    // Buzzer frequentie bij detectie (Hz)
#define BUZZER_DURATION     80      // Buzzer duur bij detectie (ms)
#define BUZZER_FULL_FREQ    2000    // Frequentie als alle LEDs vol zijn (Hz)
#define BUZZER_FULL_DURATION 200    // Duur als alle LEDs vol zijn (ms)

// ============================================================================
// COMMUNICATIE INSTELLINGEN
// ============================================================================
#define HEARTBEAT_INTERVAL  5000    // Heartbeat elke 5 seconden
#define WIFI_CHANNEL        1       // WiFi kanaal voor ESP-NOW

// Master Controller MAC adres (broadcast)
uint8_t masterMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ============================================================================
// DEBUG INSTELLINGEN
// ============================================================================
#define DEBUG_ENABLED       true
#define SERIAL_BAUD         115200

#endif // CONFIG_H
