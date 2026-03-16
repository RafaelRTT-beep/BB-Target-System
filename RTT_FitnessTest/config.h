/**
 * RTT Fitness Test Timer - Configuratie
 *
 * Pas de WiFi instellingen aan voor jouw netwerk.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// WIFI INSTELLINGEN
// ============================================================================
#define WIFI_SSID       "JouwNetwerk"
#define WIFI_PASSWORD   "JouwWachtwoord"

// ============================================================================
// SYSTEEM INSTELLINGEN
// ============================================================================
#define MAX_PARTICIPANTS  15
#define MAX_BUDDIES       8

// ============================================================================
// BUZZER (optioneel - verwijder comment om te activeren)
// ============================================================================
// #define BUZZER_PIN     26

// ============================================================================
// DEBUG
// ============================================================================
#define SERIAL_BAUD     115200

#endif // CONFIG_H
