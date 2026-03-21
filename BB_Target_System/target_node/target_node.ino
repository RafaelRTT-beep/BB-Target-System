/**
 * ============================================================================
 * RAF RTT FYSIEK - TARGET NODE FIRMWARE
 * ============================================================================
 *
 * Versie: 1.0.0
 * Auteur: RAF RTT Fysiek
 *
 * Dit is de firmware voor elke individuele target node.
 * Elke target heeft:
 * - Piezo sensor voor hit detectie
 * - WS2812B LED strip voor visuele feedback
 * - Buzzer voor audio feedback
 * - ESP-NOW communicatie met master
 *
 * INSTALLATIE:
 * 1. Installeer Arduino IDE
 * 2. Voeg ESP32 board toe via Board Manager
 * 3. Installeer FastLED library
 * 4. Pas TARGET_ID aan in config.h (1-8)
 * 5. Upload naar ESP32
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

// Bericht van Master naar Target
typedef struct {
    uint8_t cmd;        // Command type
    uint8_t targetId;   // Target ID (0 = alle targets)
    uint8_t r, g, b;    // RGB kleur
    uint8_t sound;      // Geluid type
    uint8_t param1;     // Extra parameter 1
    uint8_t param2;     // Extra parameter 2
} MasterMessage;

// Bericht van Target naar Master
typedef struct {
    uint8_t type;       // Message type
    uint8_t targetId;   // Dit target ID
    uint16_t intensity; // Hit intensiteit (0-4095)
    uint32_t timestamp; // Tijdstempel
    uint8_t status;     // Target status
} TargetMessage;

// Command types
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
    CMD_PING = 10
};

// Message types
enum MessageTypes {
    MSG_HIT = 1,
    MSG_HEARTBEAT = 2,
    MSG_STATUS = 3,
    MSG_PONG = 4
};

// Sound types
enum Sounds {
    SND_NONE = 0,
    SND_HIT = 1,
    SND_MISS = 2,
    SND_START = 3,
    SND_END = 4,
    SND_CORRECT = 5,
    SND_WRONG = 6
};

// Target states
enum TargetState {
    STATE_INACTIVE = 0,
    STATE_ACTIVE = 1,
    STATE_HIT = 2,
    STATE_NOSHOOT = 3,
    STATE_COOLDOWN = 4
};

// ============================================================================
// GLOBALE VARIABELEN
// ============================================================================

// LED Array
CRGB leds[NUM_LEDS];

// Target status
volatile TargetState currentState = STATE_INACTIVE;
volatile bool hitDetected = false;
volatile uint16_t hitIntensity = 0;
volatile uint32_t lastHitTime = 0;

// Communicatie
uint8_t broadcastMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;
bool masterRegistered = false;

// Timing
unsigned long lastHeartbeat = 0;
unsigned long lastPiezoRead = 0;
unsigned long stateStartTime = 0;

// Animatie
bool animationActive = false;
uint8_t animationType = 0;
uint8_t animationPhase = 0;

// Huidige kleur
CRGB currentColor = CRGB(COLOR_INACTIVE_R, COLOR_INACTIVE_G, COLOR_INACTIVE_B);
CRGB targetColor = currentColor;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    // Serial initialiseren
    Serial.begin(SERIAL_BAUD);
    delay(100);

    Serial.println("\n============================================");
    Serial.println("   BB TARGET SYSTEM - TARGET NODE");
    Serial.printf("   Target ID: %d\n", TARGET_ID);
    Serial.println("============================================\n");

    // Pins configureren
    pinMode(PIEZO_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(STATUS_LED_PIN, OUTPUT);
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

    // Broadcast peer toevoegen voor discovery
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

    // Inactieve kleur instellen
    setAllLeds(CRGB(COLOR_INACTIVE_R, COLOR_INACTIVE_G, COLOR_INACTIVE_B));

    Serial.println("\nTarget Node gereed!");
    Serial.println("Wacht op master controller...\n");

    // Stuur eerste heartbeat
    sendHeartbeat();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    unsigned long currentTime = millis();

    // Piezo sensor uitlezen (hoge frequentie)
    if (currentTime - lastPiezoRead >= (1000 / PIEZO_SAMPLE_RATE)) {
        lastPiezoRead = currentTime;
        readPiezoSensor();
    }

    // Hit verwerken indien gedetecteerd
    if (hitDetected) {
        processHit();
        hitDetected = false;
    }

    // Heartbeat versturen
    if (currentTime - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        lastHeartbeat = currentTime;
        sendHeartbeat();
    }

    // Animaties updaten
    if (animationActive) {
        updateAnimation();
    }

    // State management
    updateState();

    // LEDs updaten
    FastLED.show();
}

// ============================================================================
// PIEZO SENSOR
// ============================================================================

void readPiezoSensor() {
    // Alleen detecteren als actief of in free play
    if (currentState != STATE_ACTIVE && currentState != STATE_NOSHOOT) {
        return;
    }

    // Debounce check
    if (millis() - lastHitTime < PIEZO_DEBOUNCE_MS) {
        return;
    }

    // Lees piezo waarde
    uint16_t piezoValue = analogRead(PIEZO_PIN);

    // Check threshold
    if (piezoValue > PIEZO_THRESHOLD) {
        hitDetected = true;
        hitIntensity = piezoValue;
        lastHitTime = millis();

        if (DEBUG_ENABLED) {
            Serial.printf("HIT DETECTED! Intensity: %d\n", piezoValue);
        }
    }
}

void processHit() {
    // Stuur hit bericht naar master
    TargetMessage msg;
    msg.type = MSG_HIT;
    msg.targetId = TARGET_ID;
    msg.intensity = hitIntensity;
    msg.timestamp = millis();
    msg.status = currentState;

    esp_now_send(broadcastMAC, (uint8_t *)&msg, sizeof(msg));

    // Lokale feedback
    if (currentState == STATE_ACTIVE) {
        // Goede hit!
        playSound(SND_HIT);
        flashLeds(CRGB(COLOR_HIT_R, COLOR_HIT_G, COLOR_HIT_B), 3);
        currentState = STATE_COOLDOWN;
        stateStartTime = millis();
    } else if (currentState == STATE_NOSHOOT) {
        // Foute hit (no-shoot target geraakt)
        playSound(SND_WRONG);
        flashLeds(CRGB::Red, 5);
    }

    if (DEBUG_ENABLED) {
        Serial.printf("Hit verzonden - Target: %d, Intensity: %d\n",
                      TARGET_ID, hitIntensity);
    }
}

// ============================================================================
// LED FUNCTIES
// ============================================================================

void setAllLeds(CRGB color) {
    currentColor = color;
    fill_solid(leds, NUM_LEDS, color);
}

void flashLeds(CRGB color, int times) {
    for (int i = 0; i < times; i++) {
        fill_solid(leds, NUM_LEDS, color);
        FastLED.show();
        delay(50);
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();
        delay(50);
    }
    // Herstel naar huidige kleur
    fill_solid(leds, NUM_LEDS, currentColor);
}

void pulseLeds() {
    static uint8_t brightness = 0;
    static int8_t direction = 5;

    brightness += direction;
    if (brightness >= 250 || brightness <= 50) {
        direction = -direction;
    }

    CRGB pulsedColor = currentColor;
    pulsedColor.nscale8(brightness);
    fill_solid(leds, NUM_LEDS, pulsedColor);
}

void rainbowLeds() {
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue, 255 / NUM_LEDS);
    hue++;
}

void startupAnimation() {
    // Kleur wipe animatie
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Green;
        FastLED.show();
        delay(30);
    }
    delay(200);
    for (int i = NUM_LEDS - 1; i >= 0; i--) {
        leds[i] = CRGB::Black;
        FastLED.show();
        delay(30);
    }

    // Target ID knipperen
    for (int i = 0; i < TARGET_ID; i++) {
        fill_solid(leds, NUM_LEDS, CRGB::Blue);
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

// ============================================================================
// GELUID FUNCTIES
// ============================================================================

void playSound(uint8_t soundType) {
    switch (soundType) {
        case SND_HIT:
            tone(BUZZER_PIN, SOUND_HIT_FREQ, SOUND_HIT_DURATION);
            break;
        case SND_MISS:
            tone(BUZZER_PIN, SOUND_MISS_FREQ, SOUND_MISS_DURATION);
            break;
        case SND_START:
            // Oplopende toon
            for (int f = 500; f <= 1500; f += 100) {
                tone(BUZZER_PIN, f, 30);
                delay(30);
            }
            break;
        case SND_END:
            // Aflopende toon
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
// ESP-NOW CALLBACKS
// ============================================================================

void onDataReceived(const uint8_t *mac, const uint8_t *data, int len) {
    if (len != sizeof(MasterMessage)) {
        return;
    }

    MasterMessage *msg = (MasterMessage *)data;

    // Check of bericht voor dit target is (of broadcast)
    if (msg->targetId != 0 && msg->targetId != TARGET_ID) {
        return;
    }

    if (DEBUG_ENABLED) {
        Serial.printf("Ontvangen CMD: %d voor target %d\n", msg->cmd, msg->targetId);
    }

    // Verwerk commando
    switch (msg->cmd) {
        case CMD_ACTIVATE:
            currentState = STATE_ACTIVE;
            currentColor = CRGB(msg->r, msg->g, msg->b);
            if (currentColor == CRGB::Black) {
                currentColor = CRGB(COLOR_ACTIVE_R, COLOR_ACTIVE_G, COLOR_ACTIVE_B);
            }
            setAllLeds(currentColor);
            animationActive = false;
            if (msg->sound) playSound(SND_START);
            Serial.println("TARGET GEACTIVEERD");
            break;

        case CMD_DEACTIVATE:
            currentState = STATE_INACTIVE;
            currentColor = CRGB(COLOR_INACTIVE_R, COLOR_INACTIVE_G, COLOR_INACTIVE_B);
            setAllLeds(currentColor);
            animationActive = false;
            Serial.println("Target gedeactiveerd");
            break;

        case CMD_SET_COLOR:
            currentColor = CRGB(msg->r, msg->g, msg->b);
            setAllLeds(currentColor);
            // Als kleur rood is, markeer als no-shoot
            if (msg->r > 200 && msg->g < 50 && msg->b < 50) {
                currentState = STATE_NOSHOOT;
            }
            break;

        case CMD_PLAY_SOUND:
            playSound(msg->sound);
            break;

        case CMD_RESET:
            currentState = STATE_INACTIVE;
            currentColor = CRGB(COLOR_INACTIVE_R, COLOR_INACTIVE_G, COLOR_INACTIVE_B);
            setAllLeds(currentColor);
            animationActive = false;
            hitDetected = false;
            Serial.println("Target gereset");
            break;

        case CMD_FLASH:
            flashLeds(CRGB(msg->r, msg->g, msg->b), msg->param1);
            break;

        case CMD_PULSE:
            animationActive = true;
            animationType = CMD_PULSE;
            break;

        case CMD_RAINBOW:
            animationActive = true;
            animationType = CMD_RAINBOW;
            break;

        case CMD_SET_BRIGHTNESS:
            FastLED.setBrightness(msg->param1);
            break;

        case CMD_PING:
            // Stuur pong terug
            {
                TargetMessage pong;
                pong.type = MSG_PONG;
                pong.targetId = TARGET_ID;
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
    // Blink status LED bij succesvolle verzending
    if (status == ESP_NOW_SEND_SUCCESS) {
        digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
    }
}

// ============================================================================
// HEARTBEAT & STATUS
// ============================================================================

void sendHeartbeat() {
    TargetMessage msg;
    msg.type = MSG_HEARTBEAT;
    msg.targetId = TARGET_ID;
    msg.intensity = 0;
    msg.timestamp = millis();
    msg.status = currentState;

    esp_now_send(broadcastMAC, (uint8_t *)&msg, sizeof(msg));

    if (DEBUG_ENABLED) {
        Serial.printf("Heartbeat verzonden - Target %d, Status: %d\n",
                      TARGET_ID, currentState);
    }
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

void updateState() {
    // Cooldown afhandelen
    if (currentState == STATE_COOLDOWN) {
        if (millis() - stateStartTime > 500) {
            currentState = STATE_INACTIVE;
            setAllLeds(CRGB(COLOR_INACTIVE_R, COLOR_INACTIVE_G, COLOR_INACTIVE_B));
        }
    }
}

// ============================================================================
// ANIMATIE
// ============================================================================

void updateAnimation() {
    static unsigned long lastUpdate = 0;

    if (millis() - lastUpdate < 20) { // 50 FPS
        return;
    }
    lastUpdate = millis();

    switch (animationType) {
        case CMD_PULSE:
            pulseLeds();
            break;
        case CMD_RAINBOW:
            rainbowLeds();
            break;
    }
}
