/**
 * ============================================================================
 * BB TARGET SYSTEM - ULTRASONIC COUNTER NODE FIRMWARE
 * ============================================================================
 *
 * Versie: 2.0.0
 * Auteur: Raf RTT Training System
 *
 * Dit is de firmware voor de ultrasone teller node.
 * Compatibel met MasterV3Sec via TargetMessage protocol.
 *
 * Features:
 * - HC-SR04 ultrasone sensor voor hand detectie
 * - WS2812B LED strip (6 LEDs) als visuele teller
 * - Buzzer piept ALTIJD bij detectie
 * - Instelbaar: hoeveel detecties per LED
 * - ESP-NOW communicatie met MasterV3Sec (zelfde protocol als targets)
 *
 * WERKING:
 * - Beweeg je hand over de sensor (binnen detectie afstand)
 * - De buzzer geeft een piep bij ELKE detectie
 * - Na X detecties (instelbaar) gaat er 1 LED branden
 * - Als alle 6 LEDs branden: gouden animatie + lang buzzer signaal
 * - Reset via webinterface of master controller
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
// MESSAGE PROTOCOL - Must match MasterV3Sec EXACTLY
// ============================================================================

enum MessageType {
  MSG_HIT           = 1,
  MSG_LIGHT_ON      = 2,
  MSG_LIGHT_OFF     = 3,
  MSG_PING          = 4,
  MSG_PONG          = 5,
  MSG_FLASH         = 6,
  MSG_SET_THRESHOLD = 7,
  MSG_OTA_MODE      = 8,
  MSG_ANNOUNCE      = 9,
  MSG_BUZZ          = 10
};

typedef struct __attribute__((packed)) {
  uint8_t  msgType;
  uint8_t  targetId;
  uint32_t timestamp;
  uint16_t intensity;
  uint8_t  colorR;
  uint8_t  colorG;
  uint8_t  colorB;
  uint8_t  extra;
} TargetMessage;

// ============================================================================
// NODE STATES
// ============================================================================

enum NodeState {
  STATE_IDLE     = 0,
  STATE_COUNTING = 1,
  STATE_FULL     = 2,
  STATE_PAUSED   = 3
};

// ============================================================================
// GLOBALE VARIABELEN
// ============================================================================

CRGB leds[NUM_LEDS];

volatile NodeState currentState = STATE_COUNTING;
volatile uint16_t totalCount = 0;
volatile uint8_t  ledsLit = 0;
volatile uint8_t  countsPerLed = DEFAULT_COUNTS_PER_LED;
volatile uint16_t countsSinceLastLed = 0;

volatile bool handDetected = false;
volatile bool previousHandState = false;
volatile float lastDistance = 0;

uint8_t broadcastMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

unsigned long lastHeartbeat = 0;
unsigned long lastMeasurement = 0;
unsigned long lastDetectionTime = 0;

bool celebrationActive = false;
unsigned long celebrationStart = 0;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(100);

  Serial.println("\n============================================");
  Serial.println("   BB TARGET SYSTEM - ULTRASONIC COUNTER V2");
  Serial.printf("   Node ID: %d (counter node)\n", NODE_ID);
  Serial.println("   Protocol: TargetMessage (MasterV3Sec)");
  Serial.println("============================================\n");

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  startupAnimation();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.print("MAC Adres: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("FOUT: ESP-NOW init mislukt!");
    errorBlink();
    return;
  }

  esp_now_register_recv_cb(onDataReceived);

  memcpy(peerInfo.peer_addr, broadcastMAC, 6);
  peerInfo.channel = WIFI_CHANNEL;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("FOUT: Peer toevoegen mislukt!");
    errorBlink();
    return;
  }

  digitalWrite(STATUS_LED_PIN, HIGH);
  showIdleLeds();

  Serial.println("Counter gereed! Wacht op master...");
  Serial.printf("Detectie afstand: %d cm, Counts/LED: %d\n", DETECTION_DISTANCE_CM, countsPerLed);

  // Stuur announce zodat Master ons vindt
  sendAnnounce();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  unsigned long now = millis();

  if (now - lastMeasurement >= MEASUREMENT_INTERVAL_MS) {
    lastMeasurement = now;
    measureDistance();
  }

  // Heartbeat als PONG (zodat Master weet dat we online zijn)
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    lastHeartbeat = now;
    sendPong();
  }

  if (celebrationActive) {
    updateCelebration();
  }

  FastLED.show();
}

// ============================================================================
// ULTRASONE SENSOR
// ============================================================================

float readUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  if (duration == 0) return MAX_DISTANCE_CM;
  return (duration * 0.0343) / 2.0;
}

void measureDistance() {
  if (currentState != STATE_COUNTING) return;

  float distance = readUltrasonicDistance();
  lastDistance = distance;

  bool currentHandState = (distance > 0 && distance < DETECTION_DISTANCE_CM);

  // Edge detection: alleen bij nieuwe detectie
  if (currentHandState && !previousHandState) {
    if (millis() - lastDetectionTime >= DEBOUNCE_MS) {
      lastDetectionTime = millis();
      onHandDetected(distance);
    }
  }

  previousHandState = currentHandState;
}

void onHandDetected(float distance) {
  // Buzzer altijd bij detectie
  tone(BUZZER_PIN, BUZZER_FREQ, BUZZER_DURATION);

  totalCount++;
  countsSinceLastLed++;

  Serial.printf("DETECTIE! Afstand: %.1f cm | Totaal: %d | LED progress: %d/%d\n",
                distance, totalCount, countsSinceLastLed, countsPerLed);

  // Check of er een nieuwe LED aan moet
  if (countsSinceLastLed >= countsPerLed) {
    countsSinceLastLed = 0;
    if (ledsLit < NUM_LEDS) {
      ledsLit++;
      updateLedDisplay();
      Serial.printf("LED %d/%d AAN!\n", ledsLit, NUM_LEDS);

      if (ledsLit >= NUM_LEDS) {
        onAllLedsFull();
      }
    }
  } else {
    flashNextLed();
  }

  // Stuur MSG_HIT naar Master met count als intensity
  sendHit();
}

void onAllLedsFull() {
  currentState = STATE_FULL;
  celebrationActive = true;
  celebrationStart = millis();
  tone(BUZZER_PIN, BUZZER_FULL_FREQ, BUZZER_FULL_DURATION);
  Serial.println("*** ALLE LEDS VOL! ***");
  sendHit();
}

// ============================================================================
// LED FUNCTIES
// ============================================================================

void updateLedDisplay() {
  for (int i = 0; i < NUM_LEDS; i++) {
    if (i < ledsLit) {
      uint8_t progress = map(i, 0, NUM_LEDS - 1, 0, 255);
      leds[i] = CRGB(progress, 255 - (progress / 3), 0);
    } else {
      leds[i] = CRGB(COLOR_LED_OFF_R, COLOR_LED_OFF_G, COLOR_LED_OFF_B);
    }
  }
}

void flashNextLed() {
  if (ledsLit < NUM_LEDS) {
    uint8_t brightness = map(countsSinceLastLed, 0, countsPerLed, 30, 200);
    leds[ledsLit] = CRGB(COLOR_FLASH_R, COLOR_FLASH_G, COLOR_FLASH_B);
    leds[ledsLit].nscale8(brightness);
    FastLED.show();
    delay(30);
    leds[ledsLit] = CRGB(COLOR_LED_OFF_R, COLOR_LED_OFF_G, COLOR_LED_OFF_B);
  }
}

void showIdleLeds() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(COLOR_LED_OFF_R, COLOR_LED_OFF_G, COLOR_LED_OFF_B);
  }
}

void startupAnimation() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(0, 255, 0);
    FastLED.show();
    delay(40);
  }
  delay(200);
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
    celebrationActive = false;
    fill_solid(leds, NUM_LEDS, CRGB(COLOR_FULL_R, COLOR_FULL_G, COLOR_FULL_B));
    return;
  }
  static uint8_t hue = 0;
  fill_rainbow(leds, NUM_LEDS, hue, 255 / NUM_LEDS);
  hue += 3;
}

// ============================================================================
// TELLER FUNCTIES
// ============================================================================

void resetCounter() {
  totalCount = 0;
  ledsLit = 0;
  countsSinceLastLed = 0;
  celebrationActive = false;
  currentState = STATE_COUNTING;

  showIdleLeds();
  FastLED.show();

  // Start geluid
  tone(BUZZER_PIN, 800, 100);
  delay(120);
  tone(BUZZER_PIN, 1200, 100);

  Serial.println("Teller gereset!");
  sendPong();
}

// ============================================================================
// ESP-NOW COMMUNICATIE (TargetMessage protocol)
// ============================================================================

void sendMsg(uint8_t type) {
  TargetMessage msg = {};
  msg.msgType   = type;
  msg.targetId  = NODE_ID;
  msg.timestamp = millis();
  msg.intensity = totalCount;
  msg.colorR    = ledsLit;
  msg.colorG    = NUM_LEDS;
  msg.colorB    = countsPerLed;
  msg.extra     = (uint8_t)currentState;
  esp_now_send(broadcastMAC, (uint8_t*)&msg, sizeof(msg));
}

void sendHit()     { sendMsg(MSG_HIT); }
void sendPong()    { sendMsg(MSG_PONG); }
void sendAnnounce(){ sendMsg(MSG_ANNOUNCE); }

void onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(TargetMessage)) return;

  TargetMessage msg;
  memcpy(&msg, data, sizeof(msg));

  // Alleen berichten voor ons of broadcast (id=0)
  if (msg.targetId != NODE_ID && msg.targetId != 0) return;

  switch (msg.msgType) {
    case MSG_PING:
      sendPong();
      break;

    case MSG_LIGHT_ON:
      // Activeer tellen + stel kleur in
      if (currentState == STATE_IDLE || currentState == STATE_PAUSED || currentState == STATE_FULL) {
        resetCounter();
      }
      currentState = STATE_COUNTING;
      break;

    case MSG_LIGHT_OFF:
      // Pauzeer of stop
      currentState = STATE_PAUSED;
      showIdleLeds();
      FastLED.show();
      break;

    case MSG_FLASH:
      // Flash LEDs in opgegeven kleur
      for (int i = 0; i < 3; i++) {
        fill_solid(leds, NUM_LEDS, CRGB(msg.colorR, msg.colorG, msg.colorB));
        FastLED.show();
        delay(80);
        updateLedDisplay();
        FastLED.show();
        delay(80);
      }
      break;

    case MSG_SET_THRESHOLD:
      // Hergebruik: stel countsPerLed in via intensity veld
      if (msg.intensity > 0 && msg.intensity <= 100) {
        countsPerLed = msg.intensity;
        countsSinceLastLed = 0;
        // Herbereken LEDs
        ledsLit = 0;
        uint16_t temp = totalCount;
        while (temp >= countsPerLed && ledsLit < NUM_LEDS) {
          ledsLit++;
          temp -= countsPerLed;
        }
        countsSinceLastLed = temp;
        updateLedDisplay();
        FastLED.show();
        Serial.printf("Counts/LED: %d\n", countsPerLed);
      }
      break;

    case MSG_BUZZ:
      tone(BUZZER_PIN, 1500, 200);
      break;
  }
}
