/**
 * RTT Target System - Master Controller Configuration
 * Running the Target (RTT) — Train real. Train smart.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// NETWERK INSTELLINGEN
// ============================================================================
#define WIFI_SSID           "RAF RTT TRAINING SYSTEM"
#define WIFI_PASSWORD       "12345678"  // Minimaal 8 karakters
#define WIFI_CHANNEL        1

// Webserver
#define WEB_SERVER_PORT     80
#define WEBSOCKET_PORT      81

// Master MAC: 28:05:A5:07:41:FD

// ============================================================================
// SYSTEEM INSTELLINGEN
// ============================================================================
#define MAX_TARGETS         6       // Maximum aantal targets
#define TARGET_TIMEOUT_MS   15000   // Target offline na 15 sec zonder heartbeat

// ============================================================================
// GAME INSTELLINGEN
// ============================================================================
// Standaard game tijden (in seconden)
#define DEFAULT_GAME_TIME   60      // 1 minuut
#define DEFAULT_TARGET_TIME 3       // 3 seconden per target in sequence/random

// Punten
#define POINTS_HIT          100     // Punten voor hit
#define POINTS_MISS         -25     // Strafpunten voor miss
#define POINTS_SHOOT_HIT    10      // Punten voor shoot target (Shoot/No-Shoot)
#define POINTS_NOSHOOT_HIT  -11     // Strafpunten voor no-shoot hit
#define POINTS_TIMEOUT      -10     // Strafpunten voor timeout
#define POINTS_BONUS_FAST   50      // Bonus voor snelle hit (< 1 sec)

// ============================================================================
// SCORE OPSLAG
// ============================================================================
#define MAX_HIGHSCORES      10      // Aantal highscores opslaan
#define SCORE_NAMESPACE     "scores"

// ============================================================================
// DEBUG
// ============================================================================
#define DEBUG_ENABLED       true
#define SERIAL_BAUD         115200

// ============================================================================
// KLEUREN (standaard)
// ============================================================================
#define COLOR_SHOOT_R       0
#define COLOR_SHOOT_G       255
#define COLOR_SHOOT_B       0

#define COLOR_NOSHOOT_R     255
#define COLOR_NOSHOOT_G     0
#define COLOR_NOSHOOT_B     0

#define COLOR_IDLE_R        0
#define COLOR_IDLE_G        0
#define COLOR_IDLE_B        50

#endif // CONFIG_H
