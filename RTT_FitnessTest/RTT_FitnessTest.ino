/**
 * ============================================================================
 * RTT FITNESS TEST TIMER SYSTEM
 * ============================================================================
 *
 * Versie: 1.0.0
 *
 * Timing systeem voor groepstesten met max 15 deelnemers.
 * Buddy's bedienen timers op hun telefoon, admin ziet alles live.
 *
 * INSTALLATIE:
 * 1. Pas WiFi instellingen aan in config.h
 * 2. Installeer libraries: ESPAsyncWebServer, AsyncTCP, ArduinoJson
 * 3. Upload LittleFS data (data/ map) via ESP32 Sketch Data Upload
 * 4. Upload firmware naar ESP32
 * 5. Open Serial Monitor (115200 baud) voor IP-adres
 *
 * ============================================================================
 */

#include <WiFi.h>
#include <LittleFS.h>
#include "config.h"

// Buzzer state (moet voor webserver.h include staan)
#ifdef BUZZER_PIN
volatile unsigned long buzzerOffTime = 0;

void playBuzzer(int durationMs) {
    ledcWriteTone(BUZZER_PIN, 1000);
    buzzerOffTime = millis() + durationMs;
}
#else
void playBuzzer(int durationMs) {}
#endif

#include "webserver.h"

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);

    Serial.println();
    Serial.println("============================================");
    Serial.println("   RTT FITNESS TEST TIMER SYSTEM");
    Serial.println("============================================");

    // Buzzer pin
    #ifdef BUZZER_PIN
    ledcAttach(BUZZER_PIN, 2000, 8);
    Serial.println("[OK] Buzzer geconfigureerd");
    #endif

    // LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("[FOUT] LittleFS mount mislukt!");
        return;
    }
    Serial.println("[OK] LittleFS gestart");

    // WiFi verbinden (STA mode)
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("Verbinden met WiFi: %s", WIFI_SSID);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[FOUT] WiFi verbinding mislukt!");
        Serial.println("Controleer SSID en wachtwoord in config.h");
        return;
    }

    Serial.println("[OK] WiFi verbonden");
    Serial.println("============================================");
    Serial.printf("   IP-adres: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("   Admin:    http://%s/admin\n", WiFi.localIP().toString().c_str());
    Serial.println("============================================");

    // Webserver starten
    setupWebServer();
    Serial.println("[OK] Webserver gestart");
    Serial.println();
    Serial.println("Systeem gereed!");
}

void loop() {
    ws.cleanupClients();

    // Buzzer uitschakelen na timeout
    #ifdef BUZZER_PIN
    if (buzzerOffTime > 0 && millis() >= buzzerOffTime) {
        ledcWriteTone(BUZZER_PIN, 0);
        buzzerOffTime = 0;
    }
    #endif
}
