/**
 * ============================================================================
 * BB TARGET SYSTEM - ULTRASONIC COUNTER NODE FIRMWARE
 * ============================================================================
 *
 * Versie: 1.0.0
 * Auteur: Raf RTT Training System
 *
 * Dit is de firmware voor de ultrasone teller node.
 * Features:
 * - HC-SR04 ultrasone sensor voor hand detectie
 * - WS2812B LED strip (6 LEDs) als visuele teller
 * - Buzzer piept ALTIJD bij detectie
 * - Instelbaar: hoeveel detecties per LED
 * - ESP-NOW communicatie met master controller
 * - Koppeling met het Raf RTT Training Systeem
 *
 * WERKING:
 * - Beweeg je hand over de sensor (binnen detectie afstand)
 * - De buzzer geeft een piep bij ELKE detectie
 * - Na X detecties (instelbaar) gaat er 1 LED branden
 * - Als alle 6 LEDs branden: gouden animatie + lang buzzer signaal
 * - Reset via webinterface of master controller
 *
 * INSTALLATIE:
 * 1. Installeer Arduino IDE
 * 2. Voeg ESP32 board toe via Board Manager
 * 3. Installeer FastLED library
 * 4. Pas NODE_ID aan in config.h
 * 5. Upload naar ESP32
 *
 * BEDRADING:
 * - HC-SR04 TRIG -> GPIO 5
 * - HC-SR04 ECHO -> GPIO 18 (via spanningsdeler 5V->3.3V)
 * - HC-SR04 VCC  -> 5V
 * - HC-SR04 GND  -> GND
 * - WS2812B DIN  -> GPIO 13
 * - WS2812B VCC  -> 5V
 * - WS2812B GND  -> GND
 * - Buzzer +     -> GPIO 26
 * - Buzzer -     -> GND
 *
 * ============================================================================
 */

#include <esp_now.h>
#include <WiFi.h>
#include <FastLED.h>
#include "config.h"

// ============================================================================
// STRUCTUREN EN ENUMS
// ============================================================================

// Bericht van Master naar Counter Node
typedef struct {
    uint8_t cmd;        // Command type
    uint8_t targetId;   // Node ID (0 = alle nodes)
    uint8_t r, g, b;    // RGB kleur
    uint8_t sound;      // Geluid type
    uint8_t param1;     // Extra parameter 1 (bijv. countsPerLed)
    uint8_t param2;     // Extra parameter 2
} MasterMessage;

// Bericht van Counter Node naar Master
typedef struct {
    uint8_t type;       // Message type
    uint8_t targetId;   // Dit node ID
    uint16_t intensity; // Detectie afstand (cm) of count waarde
    uint32_t timestamp; // Tijdstempel
    uint8_t status;     // Node status
} CounterMessage;

// Command types (compatibel met target nodes + uitbreidingen)
enum Commands {
    CMD_ACTIVATE = 1,
    CMD_DEACTIVATE = 2,
    CMD_SET_COLOR = 3,
    CMD_PLAY_SOUND = 4,
    CMD_RESET = 5,
    CMD_FLASH = 6,
    CMD_PULSE = 7,
    CMD_RAINBOW = 8,
    CMD_SET_BRIGHTNESS = 9,
    CMD_PING = 10,
    // Counter-specifieke commando's
    CMD_SET_COUNTS_PER_LED = 20,    // Stel in hoeveel detecties per LED
    CMD_RESET_COUNTER = 21,         // Reset de teller
    CMD_SET_TARGET_COUNT = 22       // Stel doel-telling in
};

// Message types
enum MessageTypes {
    MSG_HIT = 1,           // Hergebruikt voor detectie event
    MSG_HEARTBEAT = 2,
    MSG_STATUS = 3,
    MSG_PONG = 4,
    MSG_COUNTER_UPDATE = 10 // Counter specifiek status update
};

// Sound types
enum Sounds {
    SND_NONE = 0,
    SND_HIT = 1,
    SND_MISS = 2,
    SND_START = 3,
    SND_END = 4,
    SND_CORRECT = 5,
    SND_WRONG = 6,
    SND_BEEP = 7       // Korte beep voor detectie
};

// Node states
enum NodeState {
    STATE_IDLE = 0,         // Wacht op activatie
    STATE_COUNTING = 1,     // Actief tellen
    STATE_FULL = 2,         // Alle LEDs aan
    STATE_PAUSED = 3,       // Gepauzeerd
    STATE_RED_WAITING = 4,  // Rode LED - wacht op hand
    STATE_GREEN_OK = 5      // Groene LED - hand op tijd!
};

// ============================================================================
// GLOBALE VARIABELEN
// ============================================================================

// LED Array
CRGB leds[NUM_LEDS];

// Teller status
volatile NodeState currentState = STATE_IDLE;
volatile uint16_t totalCount = 0;          // Totaal aantal detecties
volatile uint8_t ledsLit = 0;              // Aantal brandende LEDs
volatile uint8_t countsPerLed = DEFAULT_COUNTS_PER_LED;  // Detecties per LED
volatile uint16_t countsSinceLastLed = 0;  // Teller sinds laatste LED

// Sensor status
volatile bool handDetected = false;
volatile bool previousHandState = false;   // Voor edge detection
volatile float lastDistance = 0;

// Communicatie
uint8_t broadcastMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

// Timing
unsigned long lastHeartbeat = 0;
unsigned long lastMeasurement = 0;
unsigned long lastDetectionTime = 0;

// Animatie
bool celebrationActive = false;
unsigned long celebrationStart = 0;

// Rode timer / Groene feedback
bool redTimerActive = false;
unsigned long redTimerStart = 0;
uint8_t redTimerSeconds = 0;
bool greenFlashActive = false;
unsigned long greenFlashStart = 0;

// Rondes
uint8_t currentRound = 0;
uint8_t totalRounds = 0;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    // Serial initialiseren
    Serial.begin(SERIAL_BAUD);
    delay(100);

    Serial.println("\n============================================");
    Serial.println("   BB TARGET SYSTEM - ULTRASONIC COUNTER");
    Serial.printf("   Node ID: %d\n", NODE_ID);
    Serial.println("   Raf RTT Training Systeem");
    Serial.println("============================================\n");

    // Pins configureren
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(TRIG_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    // FastLED initialiseren
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(LED_BRIGHTNESS);
    FastLED.clear();
    FastLED.show();

    // Startup animatie
    startupAnimation();

    // WiFi initialiseren voor ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // MAC adres printen
    Serial.print("MAC Adres: ");
    Serial.println(WiFi.macAddress());

    // ESP-NOW initialiseren
    if (esp_now_init() != ESP_OK) {
        Serial.println("FOUT: ESP-NOW initialisatie mislukt!");
        errorBlink();
        return;
    }
    Serial.println("ESP-NOW geinitialiseerd");

    // Callback functies registreren
    esp_now_register_recv_cb(onDataReceived);
    esp_now_register_send_cb(onDataSent);

    // Broadcast peer toevoegen
    memcpy(peerInfo.peer_addr, broadcastMAC, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("FOUT: Peer toevoegen mislukt!");
        errorBlink();
        return;
    }

    // Status LED aan
    digitalWrite(STATUS_LED_PIN, HIGH);

    // Toon inactieve LEDs (dim blauw)
    showIdleLeds();

    Serial.println("\nUltrasonic Counter gereed!");
    Serial.printf("Detectie afstand: %d cm\n", DETECTION_DISTANCE_CM);
    Serial.printf("Counts per LED: %d\n", countsPerLed);
    Serial.println("Wacht op master controller...\n");

    // Stuur eerste heartbeat
    sendHeartbeat();

    // Start direct in counting mode (standalone modus)
    currentState = STATE_COUNTING;
    Serial.println("Standalone modus: tellen gestart");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    unsigned long currentTime = millis();

    // Ultrasone sensor meten
    if (currentTime - lastMeasurement >= MEASUREMENT_INTERVAL_MS) {
        lastMeasurement = currentTime;
        measureDistance();
    }

    // Heartbeat versturen
    if (currentTime - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        lastHeartbeat = currentTime;
        sendHeartbeat();
    }

    // Viering animatie
    if (celebrationActive) {
        updateCelebration();
    }

    // Rode timer afhandelen
    if (redTimerActive) {
        updateRedTimer();
    }

    // Groene flash afhandelen (0.5 seconde groen na detectie bij rode timer)
    if (greenFlashActive && (currentTime - greenFlashStart > 500)) {
        greenFlashActive = false;
        // Terug naar teller display
        updateLedDisplay();
    }

    // LEDs updaten
    FastLED.show();
}

// ============================================================================
// ULTRASONE SENSOR
// ============================================================================

float readUltrasonicDistance() {
    // Stuur trigger puls
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Meet echo duur (timeout na 25ms = ~400cm)
    long duration = pulseIn(ECHO_PIN, HIGH, 25000);

    if (duration == 0) {
        return MAX_DISTANCE_CM; // Geen echo = max afstand
    }

    // Bereken afstand in cm
    float distance = (duration * 0.0343) / 2.0;
    return distance;
}

void measureDistance() {
    if (currentState != STATE_COUNTING && currentState != STATE_RED_WAITING) return;

    float distance = readUltrasonicDistance();
    lastDistance = distance;

    // Detecteer hand (object binnen detectie afstand)
    bool currentHandState = (distance > 0 && distance < DETECTION_DISTANCE_CM);

    // Edge detection: alleen tellen bij nieuwe detectie (hand komt erbij)
    if (currentHandState && !previousHandState) {
        // Debounce check
        if (millis() - lastDetectionTime >= DEBOUNCE_MS) {
            lastDetectionTime = millis();
            onHandDetected(distance);
        }
    }

    previousHandState = currentHandState;
}

void onHandDetected(float distance) {
    // ============================================
    // BUZZER GAAT ALTIJD AF BIJ DETECTIE
    // ============================================
    tone(BUZZER_PIN, BUZZER_FREQ, BUZZER_DURATION);

    // Als we in rode timer modus wachten: hand op tijd! -> GROEN
    if (redTimerActive) {
        redTimerActive = false;
        greenFlashActive = true;
        greenFlashStart = millis();

        // ALLE LEDs GROEN = je hebt het gehaald!
        fill_solid(leds, NUM_LEDS, CRGB(0, 255, 0));
        FastLED.show();

        // Extra bevestiging geluid
        tone(BUZZER_PIN, 1200, 100);

        if (DEBUG_ENABLED) {
            Serial.println("OP TIJD! LEDs groen!");
        }
    }

    // Verhoog teller
    totalCount++;
    countsSinceLastLed++;

    if (DEBUG_ENABLED) {
        Serial.printf("DETECTIE! Afstand: %.1f cm | Totaal: %d | Sinds LED: %d/%d\n",
                      distance, totalCount, countsSinceLastLed, countsPerLed);
    }

    // Check of er een nieuwe LED aan moet
    if (countsSinceLastLed >= countsPerLed) {
        countsSinceLastLed = 0;

        if (ledsLit < NUM_LEDS) {
            ledsLit++;
            updateLedDisplay();

            if (DEBUG_ENABLED) {
                Serial.printf("LED %d/%d AAN!\n", ledsLit, NUM_LEDS);
            }

            // Check of alle LEDs aan zijn
            if (ledsLit >= NUM_LEDS) {
                onAllLedsFull();
            }
        }
    } else {
        // Flash de volgende LED kort als preview
        flashNextLed();
    }

    // Stuur detectie event naar master
    sendDetectionEvent(distance);
}

void onAllLedsFull() {
    currentState = STATE_FULL;
    celebrationActive = true;
    celebrationStart = millis();

    // Lang buzzer signaal
    tone(BUZZER_PIN, BUZZER_FULL_FREQ, BUZZER_FULL_DURATION);

    Serial.println("*** ALLE LEDS VOL! VIERING! ***");

    // Stuur volledig event naar master
    CounterMessage msg;
    msg.type = MSG_COUNTER_UPDATE;
    msg.targetId = NODE_ID;
    msg.intensity = totalCount;
    msg.timestamp = millis();
    msg.status = STATE_FULL;
    esp_now_send(broadcastMAC, (uint8_t *)&msg, sizeof(msg));
}

// ============================================================================
// LED FUNCTIES
// ============================================================================

void updateLedDisplay() {
    // Toon brandende LEDs in groen, rest dim blauw
    for (int i = 0; i < NUM_LEDS; i++) {
        if (i < ledsLit) {
            // Kleurverloop van groen naar geel naarmate meer LEDs branden
            uint8_t progress = map(i, 0, NUM_LEDS - 1, 0, 255);
            leds[i] = CRGB(progress, 255 - (progress / 3), 0);
        } else {
            leds[i] = CRGB(COLOR_LED_OFF_R, COLOR_LED_OFF_G, COLOR_LED_OFF_B);
        }
    }
}

void flashNextLed() {
    // Flash de volgende LED kort om voortgang te tonen
    if (ledsLit < NUM_LEDS) {
        uint8_t nextLed = ledsLit;
        CRGB originalColor = leds[nextLed];

        // Bereken helderheid op basis van voortgang naar volgende LED
        uint8_t brightness = map(countsSinceLastLed, 0, countsPerLed, 30, 200);
        leds[nextLed] = CRGB(COLOR_FLASH_R, COLOR_FLASH_G, COLOR_FLASH_B);
        leds[nextLed].nscale8(brightness);
        FastLED.show();
        delay(30);

        // Herstel
        leds[nextLed] = originalColor;
    }
}

void showIdleLeds() {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB(COLOR_LED_OFF_R, COLOR_LED_OFF_G, COLOR_LED_OFF_B);
    }
}

void startupAnimation() {
    // Opbouw animatie
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB(0, 255, 0);
        FastLED.show();
        delay(40);
    }
    delay(200);

    // Afbouw
    for (int i = NUM_LEDS - 1; i >= 0; i--) {
        leds[i] = CRGB::Black;
        FastLED.show();
        delay(40);
    }

    // Node ID knipperen
    for (int i = 0; i < (NODE_ID % 10); i++) {
        fill_solid(leds, NUM_LEDS, CRGB::Cyan);
        FastLED.show();
        delay(200);
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();
        delay(200);
    }
}

void errorBlink() {
    for (int i = 0; i < 10; i++) {
        fill_solid(leds, NUM_LEDS, CRGB::Red);
        FastLED.show();
        delay(100);
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();
        delay(100);
    }
}

void updateCelebration() {
    unsigned long elapsed = millis() - celebrationStart;

    if (elapsed > 5000) {
        // Viering klaar - terug naar FULL state met gouden LEDs
        celebrationActive = false;
        fill_solid(leds, NUM_LEDS, CRGB(COLOR_FULL_R, COLOR_FULL_G, COLOR_FULL_B));
        return;
    }

    // Rainbow animatie tijdens viering
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue, 255 / NUM_LEDS);
    hue += 3;
}

// ============================================================================
// RODE TIMER FUNCTIES
// ============================================================================

void startRedTimer(uint8_t seconds) {
    redTimerSeconds = seconds;
    redTimerActive = true;
    redTimerStart = millis();

    // LEDs rood maken
    fill_solid(leds, NUM_LEDS, CRGB(255, 0, 0));
    FastLED.show();

    currentState = STATE_RED_WAITING;

    if (DEBUG_ENABLED) {
        Serial.printf("Rode timer gestart: %d seconden\n", seconds);
    }
}

void updateRedTimer() {
    unsigned long elapsed = millis() - redTimerStart;
    unsigned long total = (unsigned long)redTimerSeconds * 1000;

    if (elapsed >= total) {
        // TIMEOUT! Niet op tijd bij sensor
        redTimerActive = false;
        currentState = STATE_COUNTING;

        // Fout signaal: snel rood knipperen
        for (int i = 0; i < 5; i++) {
            fill_solid(leds, NUM_LEDS, CRGB(255, 0, 0));
            FastLED.show();
            delay(80);
            fill_solid(leds, NUM_LEDS, CRGB::Black);
            FastLED.show();
            delay(80);
        }

        // Fout geluid
        tone(BUZZER_PIN, 300, 500);

        // Herstel LED display
        updateLedDisplay();

        if (DEBUG_ENABLED) {
            Serial.println("TIMEOUT! Niet op tijd bij sensor.");
        }
        return;
    }

    // Visueel aftellen: LEDs gaan stuk voor stuk uit van rechts naar links
    float progress = (float)elapsed / (float)total;
    int ledsToShow = NUM_LEDS - (int)(progress * NUM_LEDS);

    for (int i = 0; i < NUM_LEDS; i++) {
        if (i < ledsToShow) {
            // Kleur gaat van rood naar donkerder rood naarmate tijd vordert
            uint8_t brightness = map(ledsToShow, 0, NUM_LEDS, 50, 255);
            leds[i] = CRGB(brightness, 0, 0);
        } else {
            leds[i] = CRGB::Black;
        }
    }

    // Laatste seconden: knippereffect
    if (total - elapsed < 2000) {
        if ((elapsed / 200) % 2 == 0) {
            for (int i = 0; i < ledsToShow; i++) {
                leds[i] = CRGB(255, 0, 0);
            }
        }
    }
}

// ============================================================================
// BUZZER FUNCTIES
// ============================================================================

void playSound(uint8_t soundType) {
    switch (soundType) {
        case SND_BEEP:
            tone(BUZZER_PIN, BUZZER_FREQ, BUZZER_DURATION);
            break;
        case SND_HIT:
            tone(BUZZER_PIN, 1000, 50);
            break;
        case SND_START:
            for (int f = 500; f <= 1500; f += 100) {
                tone(BUZZER_PIN, f, 30);
                delay(30);
            }
            break;
        case SND_END:
            for (int f = 1500; f >= 500; f -= 100) {
                tone(BUZZER_PIN, f, 30);
                delay(30);
            }
            break;
        case SND_CORRECT:
            tone(BUZZER_PIN, 1200, 100);
            delay(100);
            tone(BUZZER_PIN, 1600, 150);
            break;
        case SND_WRONG:
            tone(BUZZER_PIN, 300, 300);
            break;
    }
}

// ============================================================================
// TELLER FUNCTIES
// ============================================================================

void resetCounter() {
    totalCount = 0;
    ledsLit = 0;
    countsSinceLastLed = 0;
    celebrationActive = false;
    redTimerActive = false;
    greenFlashActive = false;
    currentState = STATE_COUNTING;

    // Reset LED display
    showIdleLeds();
    FastLED.show();

    // Geluid
    playSound(SND_START);

    Serial.println("Teller gereset!");

    // Stuur reset event naar master
    CounterMessage msg;
    msg.type = MSG_COUNTER_UPDATE;
    msg.targetId = NODE_ID;
    msg.intensity = 0;
    msg.timestamp = millis();
    msg.status = STATE_COUNTING;
    esp_now_send(broadcastMAC, (uint8_t *)&msg, sizeof(msg));
}

void setCountsPerLed(uint8_t counts) {
    if (counts < 1) counts = 1;
    if (counts > 100) counts = 100;

    countsPerLed = counts;
    countsSinceLastLed = 0;

    Serial.printf("Counts per LED ingesteld op: %d\n", countsPerLed);

    // Herbereken LED display op basis van nieuwe instelling
    ledsLit = 0;
    uint16_t tempCount = totalCount;
    while (tempCount >= countsPerLed && ledsLit < NUM_LEDS) {
        ledsLit++;
        tempCount -= countsPerLed;
    }
    countsSinceLastLed = tempCount;

    updateLedDisplay();
    FastLED.show();
}

// ============================================================================
// ESP-NOW CALLBACKS
// ============================================================================

void onDataReceived(const uint8_t *mac, const uint8_t *data, int len) {
    if (len != sizeof(MasterMessage)) return;

    MasterMessage *msg = (MasterMessage *)data;

    // Check of bericht voor dit node is (of broadcast)
    if (msg->targetId != 0 && msg->targetId != NODE_ID) return;

    if (DEBUG_ENABLED) {
        Serial.printf("Ontvangen CMD: %d voor node %d\n", msg->cmd, msg->targetId);
    }

    switch (msg->cmd) {
        case CMD_ACTIVATE:
            // Start tellen
            if (currentState == STATE_IDLE || currentState == STATE_PAUSED) {
                currentState = STATE_COUNTING;
                if (msg->sound) playSound(SND_START);
                Serial.println("TELLER GEACTIVEERD");
            }
            break;

        case CMD_DEACTIVATE:
            // Pauzeer tellen
            currentState = STATE_PAUSED;
            redTimerActive = false;
            Serial.println("Teller gepauzeerd");
            break;

        case CMD_SET_COLOR:
            // Rood = start rode timer, Groen = bevestiging
            if (msg->r > 200 && msg->g < 50) {
                // ROOD: start rode timer met param1 als seconden
                startRedTimer(msg->param1 > 0 ? msg->param1 : 5);
            } else if (msg->g > 200 && msg->r < 50) {
                // GROEN: bevestiging (op tijd bij sensor)
                greenFlashActive = true;
                greenFlashStart = millis();
                fill_solid(leds, NUM_LEDS, CRGB(0, 255, 0));
            }
            break;

        case CMD_RESET:
            resetCounter();
            break;

        case CMD_SET_COUNTS_PER_LED:
            setCountsPerLed(msg->param1);
            break;

        case CMD_RESET_COUNTER:
            resetCounter();
            break;

        case CMD_SET_BRIGHTNESS:
            FastLED.setBrightness(msg->param1);
            break;

        case CMD_PLAY_SOUND:
            playSound(msg->sound);
            break;

        case CMD_FLASH:
            // Flash alle LEDs in opgegeven kleur
            for (int i = 0; i < msg->param1; i++) {
                fill_solid(leds, NUM_LEDS, CRGB(msg->r, msg->g, msg->b));
                FastLED.show();
                delay(50);
                updateLedDisplay();
                FastLED.show();
                delay(50);
            }
            break;

        case CMD_RAINBOW:
            celebrationActive = true;
            celebrationStart = millis();
            break;

        case CMD_PING:
            {
                CounterMessage pong;
                pong.type = MSG_PONG;
                pong.targetId = NODE_ID;
                pong.intensity = totalCount;
                pong.timestamp = millis();
                pong.status = currentState;
                esp_now_send(broadcastMAC, (uint8_t *)&pong, sizeof(pong));
            }
            break;
    }
}

void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    if (DEBUG_ENABLED && status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("WAARSCHUWING: Bericht verzenden mislukt");
    }
    if (status == ESP_NOW_SEND_SUCCESS) {
        digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
    }
}

// ============================================================================
// COMMUNICATIE
// ============================================================================

void sendHeartbeat() {
    CounterMessage msg;
    msg.type = MSG_HEARTBEAT;
    msg.targetId = NODE_ID;
    msg.intensity = totalCount;
    msg.timestamp = millis();
    msg.status = currentState;

    esp_now_send(broadcastMAC, (uint8_t *)&msg, sizeof(msg));

    if (DEBUG_ENABLED) {
        Serial.printf("Heartbeat - Node %d, Count: %d, LEDs: %d/%d, State: %d\n",
                      NODE_ID, totalCount, ledsLit, NUM_LEDS, currentState);
    }
}

void sendDetectionEvent(float distance) {
    CounterMessage msg;
    msg.type = MSG_HIT;
    msg.targetId = NODE_ID;
    msg.intensity = totalCount;    // Stuur totale count
    msg.timestamp = millis();
    msg.status = currentState;

    esp_now_send(broadcastMAC, (uint8_t *)&msg, sizeof(msg));

    if (DEBUG_ENABLED) {
        Serial.printf("Detectie event verzonden - Count: %d, LEDs: %d/%d\n",
                      totalCount, ledsLit, NUM_LEDS);
    }
}
