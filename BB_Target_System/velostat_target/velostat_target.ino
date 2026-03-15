// ============================================
// BB Target System - Velostat Hit Map
// ============================================
// Detecteert inslagposities op een velostat
// drukmatrix en toont ze real-time op je
// telefoon/tablet via WiFi.
//
// Hardware:
//   - ESP32 (WROOM-32 of S3)
//   - Velostat sheet (~15x15 cm)
//   - 10mm kopertape (16 strips totaal)
//   - 8x 10kΩ pull-down weerstanden
//   - Buzzer (optioneel)
//
// Werking:
//   1. 8 kopertape rijen (bovenkant velostat)
//   2. 8 kopertape kolommen (onderkant velostat)
//   3. ESP32 scant de matrix 1000+ keer/sec
//   4. Bij impact: positie berekenen via centroid
//   5. WebSocket stuurt X,Y naar je telefoon
// ============================================

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include "config.h"

// ---- Globale Variabelen ----
WebServer server(WEBSERVER_PORT);
WebSocketsServer webSocket(WEBSOCKET_PORT);

// Matrix data
int matrixValues[MATRIX_ROWS][MATRIX_COLS];
int prevPeakValue = 0;        // Vorige scan piekwaarde (voor stijgende flank detectie)
int baselineValues[MATRIX_ROWS][MATRIX_COLS];

// Hit tracking
struct Hit {
  float x;          // 0.0 - 1.0 genormaliseerd
  float y;          // 0.0 - 1.0 genormaliseerd
  int intensity;    // 0-4095 piek ADC waarde
  unsigned long timestamp;
};

Hit hitHistory[MAX_HITS];
int hitCount = 0;
unsigned long lastHitTime = 0;
bool sessionActive = true;

// Statistieken
int totalShots = 0;
float avgX = 0, avgY = 0;

// ---- Forward Declarations ----
void scanMatrix();
void detectHit();
void calibrateBaseline();
void sendHitToClients(float x, float y, int intensity);
void sendFullState(uint8_t clientNum);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
void handleRoot();
void playHitSound();

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  DEBUG_PRINTLN("\n=== BB HitMap - Velostat Target ===");

  // Status LED
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH);

  // Buzzer
  if (BUZZER_ENABLED) {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Row pins als output
  for (int r = 0; r < MATRIX_ROWS; r++) {
    pinMode(ROW_PINS[r], OUTPUT);
    digitalWrite(ROW_PINS[r], LOW);
  }

  // Column pins als input
  for (int c = 0; c < MATRIX_COLS; c++) {
    pinMode(COL_PINS[c], INPUT);
  }

  // ADC configuratie
  analogReadResolution(12);       // 12-bit (0-4095)
  analogSetAttenuation(ADC_11db); // Volledig bereik 0-3.3V

  // Baseline kalibratie
  DEBUG_PRINTLN("Kalibreren... niet aanraken!");
  Serial.flush();
  delay(500);
  calibrateBaseline();
  DEBUG_PRINTLN("Kalibratie compleet.");
  Serial.flush();

  // WiFi Access Point starten
  DEBUG_PRINTLN(">> WiFi starten...");
  Serial.flush();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  delay(100);

  IPAddress ip = WiFi.softAPIP();
  DEBUG_PRINTF("WiFi AP gestart: %s\n", WIFI_SSID);
  DEBUG_PRINTF("IP adres: %s\n", ip.toString().c_str());

  // WebSocket starten
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // Webserver routes
  server.on("/", handleRoot);
  server.begin();

  DEBUG_PRINTLN("Webserver gestart op poort 80");
  DEBUG_PRINTLN("Open http://192.168.4.1 op je telefoon");
  DEBUG_PRINTLN("=================================\n");

  digitalWrite(STATUS_LED_PIN, LOW);

  // Startup toon
  if (BUZZER_ENABLED) {
    tone(BUZZER_PIN, 1000, 100);
    delay(150);
    tone(BUZZER_PIN, 1500, 100);
    delay(150);
    tone(BUZZER_PIN, 2000, 100);
  }
}

// ============================================
// LOOP
// ============================================
void loop() {
  webSocket.loop();
  server.handleClient();

  if (sessionActive) {
    scanMatrix();
    detectHit();
  }
}

// ============================================
// Matrix Scanning
// ============================================
void scanMatrix() {
  for (int r = 0; r < MATRIX_ROWS; r++) {
    // Activeer deze rij
    digitalWrite(ROW_PINS[r], HIGH);
    delayMicroseconds(ROW_SETTLE_US);

    // Lees alle kolommen
    for (int c = 0; c < MATRIX_COLS; c++) {
      int raw = analogRead(COL_PINS[c]);
      // Trek baseline af
      int value = raw - baselineValues[r][c];
      matrixValues[r][c] = max(0, value);
    }

    // Deactiveer rij
    digitalWrite(ROW_PINS[r], LOW);
  }
}

// ============================================
// Hit Detectie met Centroid Berekening
// ============================================
void detectHit() {
  unsigned long now = millis();

  // Debounce check
  if (now - lastHitTime < DEBOUNCE_MS) return;

  // Zoek de piek waarde
  int peakValue = 0;
  for (int r = 0; r < MATRIX_ROWS; r++) {
    for (int c = 0; c < MATRIX_COLS; c++) {
      if (matrixValues[r][c] > peakValue) {
        peakValue = matrixValues[r][c];
      }
    }
  }

  // Geen hit als onder drempel
  if (peakValue < HIT_THRESHOLD) {
    prevPeakValue = peakValue;
    return;
  }

  // Stijgende flank detectie: alleen triggeren als het signaal
  // NIEUW is (vorige scan was onder drempel). Dit voorkomt dat
  // een constant signaal (slecht contact, druk) eindeloos triggert.
  if (prevPeakValue >= HIT_THRESHOLD) {
    prevPeakValue = peakValue;
    return;
  }
  prevPeakValue = peakValue;

  // Centroid berekening (gewogen gemiddelde)
  // Dit geeft sub-cel nauwkeurigheid
  float sumX = 0, sumY = 0, sumWeight = 0;

  for (int r = 0; r < MATRIX_ROWS; r++) {
    for (int c = 0; c < MATRIX_COLS; c++) {
      int val = matrixValues[r][c];
      if (val > NOISE_FLOOR) {
        sumX += c * val;
        sumY += r * val;
        sumWeight += val;
      }
    }
  }

  if (sumWeight <= 0) return;

  // Genormaliseerde positie (0.0 - 1.0)
  float hitX = (sumX / sumWeight) / (MATRIX_COLS - 1);
  float hitY = (sumY / sumWeight) / (MATRIX_ROWS - 1);

  // Clamp
  hitX = constrain(hitX, 0.0f, 1.0f);
  hitY = constrain(hitY, 0.0f, 1.0f);

  // Hit opslaan
  if (hitCount < MAX_HITS) {
    hitHistory[hitCount] = {hitX, hitY, peakValue, now};
    hitCount++;
  }

  lastHitTime = now;
  totalShots++;

  // Running average positie
  avgX = avgX + (hitX - avgX) / totalShots;
  avgY = avgY + (hitY - avgY) / totalShots;

  DEBUG_PRINTF("HIT #%d @ (%.2f, %.2f) intensiteit: %d\n",
               totalShots, hitX, hitY, peakValue);

  // Feedback
  playHitSound();
  digitalWrite(STATUS_LED_PIN, HIGH);

  // Stuur naar alle WebSocket clients
  sendHitToClients(hitX, hitY, peakValue);

  delay(10);
  digitalWrite(STATUS_LED_PIN, LOW);
}

// ============================================
// Baseline Kalibratie
// ============================================
void calibrateBaseline() {
  // Neem gemiddelde van meerdere scans
  const int NUM_SAMPLES = 20;

  DEBUG_PRINTLN(">> Baseline reset...");
  // Reset baseline
  for (int r = 0; r < MATRIX_ROWS; r++)
    for (int c = 0; c < MATRIX_COLS; c++)
      baselineValues[r][c] = 0;

  // Meerdere scans middelen
  DEBUG_PRINTLN(">> Start sampling...");
  for (int s = 0; s < NUM_SAMPLES; s++) {
    DEBUG_PRINTF("  Sample %d/%d\n", s + 1, NUM_SAMPLES);
    for (int r = 0; r < MATRIX_ROWS; r++) {
      digitalWrite(ROW_PINS[r], HIGH);
      delayMicroseconds(ROW_SETTLE_US);
      for (int c = 0; c < MATRIX_COLS; c++) {
        int val = analogRead(COL_PINS[c]);
        baselineValues[r][c] += val;
      }
      digitalWrite(ROW_PINS[r], LOW);
    }
    delay(10);
  }

  DEBUG_PRINTLN(">> Gemiddelde berekenen...");
  // Gemiddelde berekenen
  for (int r = 0; r < MATRIX_ROWS; r++) {
    for (int c = 0; c < MATRIX_COLS; c++) {
      baselineValues[r][c] /= NUM_SAMPLES;
      DEBUG_PRINTF("Baseline[%d][%d] = %d\n", r, c, baselineValues[r][c]);
    }
  }
  DEBUG_PRINTLN(">> calibrateBaseline() KLAAR");
}

// ============================================
// WebSocket
// ============================================
void sendHitToClients(float x, float y, int intensity) {
  // JSON bericht: {"type":"hit","x":0.45,"y":0.32,"i":1200,"n":5}
  char json[128];
  snprintf(json, sizeof(json),
    "{\"type\":\"hit\",\"x\":%.3f,\"y\":%.3f,\"i\":%d,\"n\":%d}",
    x, y, intensity, totalShots);
  webSocket.broadcastTXT(json);
}

void sendFullState(uint8_t clientNum) {
  // Stuur sessie info
  char json[128];
  snprintf(json, sizeof(json),
    "{\"type\":\"init\",\"shots\":%d,\"active\":%s}",
    totalShots, sessionActive ? "true" : "false");
  webSocket.sendTXT(clientNum, json);

  // Stuur alle vorige hits
  for (int i = 0; i < hitCount; i++) {
    snprintf(json, sizeof(json),
      "{\"type\":\"hit\",\"x\":%.3f,\"y\":%.3f,\"i\":%d,\"n\":%d}",
      hitHistory[i].x, hitHistory[i].y, hitHistory[i].intensity, i + 1);
    webSocket.sendTXT(clientNum, json);
    delay(5); // Voorkom buffer overflow
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      DEBUG_PRINTF("Client [%u] disconnected\n", num);
      break;

    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      DEBUG_PRINTF("Client [%u] connected: %s\n", num, ip.toString().c_str());
      sendFullState(num);
      break;
    }

    case WStype_TEXT: {
      String msg = String((char *)payload);
      DEBUG_PRINTF("Client [%u]: %s\n", num, msg.c_str());

      if (msg == "reset") {
        hitCount = 0;
        totalShots = 0;
        avgX = 0;
        avgY = 0;
        webSocket.broadcastTXT("{\"type\":\"reset\"}");
        DEBUG_PRINTLN("Sessie gereset");
      }
      else if (msg == "calibrate") {
        sessionActive = false;
        calibrateBaseline();
        sessionActive = true;
        webSocket.broadcastTXT("{\"type\":\"calibrated\"}");
        DEBUG_PRINTLN("Herkalibratie voltooid");
      }
      else if (msg == "pause") {
        sessionActive = false;
        webSocket.broadcastTXT("{\"type\":\"paused\"}");
      }
      else if (msg == "resume") {
        sessionActive = true;
        webSocket.broadcastTXT("{\"type\":\"resumed\"}");
      }
      break;
    }
  }
}

// ============================================
// Buzzer
// ============================================
void playHitSound() {
  if (!BUZZER_ENABLED) return;
  tone(BUZZER_PIN, HIT_TONE_FREQ, HIT_TONE_DURATION);
}

// ============================================
// Webpagina (embedded)
// ============================================
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="nl">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>BB HitMap</title>
  <style>
    :root {
      --bg: #0a0e1a;
      --surface: #141a2e;
      --border: #1e2a4a;
      --text: #e0e6f0;
      --text-dim: #6a7a9a;
      --accent: #00d4ff;
      --accent2: #ff6b35;
      --hit-new: #ff3333;
      --hit-old: #ffaa00;
      --hit-oldest: #888888;
      --bullseye: rgba(0, 212, 255, 0.15);
    }
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: var(--bg);
      color: var(--text);
      min-height: 100vh;
      overflow-x: hidden;
      -webkit-user-select: none;
      user-select: none;
    }

    /* Header */
    .header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 12px 16px;
      background: var(--surface);
      border-bottom: 1px solid var(--border);
    }
    .header h1 {
      font-size: 18px;
      font-weight: 700;
      color: var(--accent);
    }
    .status {
      display: flex;
      align-items: center;
      gap: 6px;
      font-size: 12px;
      color: var(--text-dim);
    }
    .status-dot {
      width: 8px; height: 8px;
      border-radius: 50%;
      background: #444;
    }
    .status-dot.connected { background: #00cc66; }

    /* Stats Bar */
    .stats-bar {
      display: flex;
      gap: 0;
      background: var(--surface);
      border-bottom: 1px solid var(--border);
    }
    .stat {
      flex: 1;
      text-align: center;
      padding: 10px 8px;
      border-right: 1px solid var(--border);
    }
    .stat:last-child { border-right: none; }
    .stat-value {
      font-size: 22px;
      font-weight: 700;
      color: var(--accent);
    }
    .stat-label {
      font-size: 10px;
      color: var(--text-dim);
      text-transform: uppercase;
      letter-spacing: 0.5px;
      margin-top: 2px;
    }

    /* Target Container */
    .target-container {
      display: flex;
      justify-content: center;
      padding: 16px;
    }
    .target-wrap {
      position: relative;
      width: min(85vw, 85vh - 200px);
      max-width: 500px;
      aspect-ratio: 1;
    }
    canvas#targetCanvas {
      width: 100%;
      height: 100%;
      border-radius: 50%;
      background: var(--surface);
      border: 2px solid var(--border);
    }

    /* Grouping Info */
    .grouping-bar {
      display: flex;
      gap: 0;
      margin: 0 16px;
      background: var(--surface);
      border-radius: 8px;
      border: 1px solid var(--border);
      overflow: hidden;
    }
    .group-stat {
      flex: 1;
      text-align: center;
      padding: 8px;
      border-right: 1px solid var(--border);
    }
    .group-stat:last-child { border-right: none; }
    .group-value {
      font-size: 16px;
      font-weight: 600;
      color: var(--accent2);
    }
    .group-label {
      font-size: 9px;
      color: var(--text-dim);
      text-transform: uppercase;
      margin-top: 2px;
    }

    /* Buttons */
    .controls {
      display: flex;
      gap: 10px;
      padding: 16px;
      justify-content: center;
    }
    .btn {
      padding: 12px 24px;
      border: 1px solid var(--border);
      border-radius: 8px;
      background: var(--surface);
      color: var(--text);
      font-size: 14px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.15s;
      -webkit-tap-highlight-color: transparent;
    }
    .btn:active {
      transform: scale(0.95);
    }
    .btn-primary {
      background: var(--accent);
      color: #000;
      border-color: var(--accent);
    }
    .btn-danger {
      border-color: #ff4444;
      color: #ff4444;
    }

    /* Hit Log */
    .log-section {
      margin: 0 16px 16px;
      background: var(--surface);
      border-radius: 8px;
      border: 1px solid var(--border);
      max-height: 150px;
      overflow-y: auto;
    }
    .log-header {
      padding: 8px 12px;
      font-size: 11px;
      color: var(--text-dim);
      text-transform: uppercase;
      letter-spacing: 1px;
      border-bottom: 1px solid var(--border);
      position: sticky;
      top: 0;
      background: var(--surface);
    }
    .log-entry {
      padding: 6px 12px;
      font-size: 12px;
      font-family: 'SF Mono', 'Cascadia Code', monospace;
      border-bottom: 1px solid rgba(30,42,74,0.5);
      display: flex;
      justify-content: space-between;
    }
    .log-entry:last-child { border-bottom: none; }
    .log-num { color: var(--text-dim); width: 30px; }
    .log-pos { color: var(--accent); }
    .log-int { color: var(--accent2); }

    /* Responsive */
    @media (max-height: 600px) {
      .target-wrap { width: min(60vw, 60vh); }
      .stats-bar .stat { padding: 6px; }
      .stat-value { font-size: 18px; }
      .controls { padding: 8px 16px; }
    }

    /* Pulse animation on hit */
    @keyframes hitPulse {
      0% { box-shadow: 0 0 0 0 rgba(255,51,51,0.6); }
      100% { box-shadow: 0 0 0 20px rgba(255,51,51,0); }
    }
    .hit-flash {
      animation: hitPulse 0.4s ease-out;
    }
  </style>
</head>
<body>

  <div class="header">
    <h1>BB HitMap</h1>
    <div class="status">
      <span id="statusText">Verbinden...</span>
      <div class="status-dot" id="statusDot"></div>
    </div>
  </div>

  <div class="stats-bar">
    <div class="stat">
      <div class="stat-value" id="shotCount">0</div>
      <div class="stat-label">Schoten</div>
    </div>
    <div class="stat">
      <div class="stat-value" id="lastHitPos">--</div>
      <div class="stat-label">Laatste</div>
    </div>
    <div class="stat">
      <div class="stat-value" id="groupSize">--</div>
      <div class="stat-label">Groepering</div>
    </div>
  </div>

  <div class="target-container">
    <div class="target-wrap">
      <canvas id="targetCanvas" width="600" height="600"></canvas>
    </div>
  </div>

  <div class="grouping-bar">
    <div class="group-stat">
      <div class="group-value" id="avgOffset">--</div>
      <div class="group-label">Gem. Afwijking</div>
    </div>
    <div class="group-stat">
      <div class="group-value" id="spreadX">--</div>
      <div class="group-label">Spread X</div>
    </div>
    <div class="group-stat">
      <div class="group-value" id="spreadY">--</div>
      <div class="group-label">Spread Y</div>
    </div>
  </div>

  <div class="controls">
    <button class="btn btn-primary" id="btnReset" onclick="resetSession()">Reset</button>
    <button class="btn" id="btnCalibrate" onclick="calibrate()">Kalibreer</button>
    <button class="btn" id="btnPause" onclick="togglePause()">Pauze</button>
  </div>

  <div class="log-section" id="logSection">
    <div class="log-header">Hit Log</div>
  </div>

<script>
// ---- State ----
const hits = [];
let connected = false;
let paused = false;
let ws;
const canvas = document.getElementById('targetCanvas');
const ctx = canvas.getContext('2d');

// ---- WebSocket Verbinding ----
function connectWS() {
  const host = window.location.hostname || '192.168.4.1';
  ws = new WebSocket('ws://' + host + ':81');

  ws.onopen = () => {
    connected = true;
    document.getElementById('statusDot').classList.add('connected');
    document.getElementById('statusText').textContent = 'Verbonden';
  };

  ws.onclose = () => {
    connected = false;
    document.getElementById('statusDot').classList.remove('connected');
    document.getElementById('statusText').textContent = 'Verbinding verbroken';
    setTimeout(connectWS, 2000);
  };

  ws.onerror = () => {
    ws.close();
  };

  ws.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data);
      handleMessage(data);
    } catch (e) {
      console.error('Parse error:', e);
    }
  };
}

// ---- Berichten Afhandelen ----
function handleMessage(data) {
  switch (data.type) {
    case 'hit':
      addHit(data.x, data.y, data.i, data.n);
      break;
    case 'reset':
      hits.length = 0;
      updateStats();
      drawTarget();
      clearLog();
      break;
    case 'init':
      document.getElementById('shotCount').textContent = data.shots;
      paused = !data.active;
      updatePauseButton();
      break;
    case 'calibrated':
      showNotification('Kalibratie voltooid');
      break;
    case 'paused':
      paused = true;
      updatePauseButton();
      break;
    case 'resumed':
      paused = false;
      updatePauseButton();
      break;
  }
}

// ---- Hit Toevoegen ----
function addHit(x, y, intensity, shotNum) {
  hits.push({ x, y, intensity, num: shotNum, time: Date.now() });
  updateStats();
  drawTarget();
  addLogEntry(shotNum, x, y, intensity);

  // Visuele feedback
  canvas.classList.remove('hit-flash');
  void canvas.offsetWidth; // reflow trigger
  canvas.classList.add('hit-flash');

  // Haptic feedback (als beschikbaar)
  if (navigator.vibrate) navigator.vibrate(30);
}

// ---- Doelwit Tekenen ----
function drawTarget() {
  const w = canvas.width;
  const h = canvas.height;
  const cx = w / 2;
  const cy = h / 2;
  const maxR = w / 2 - 10;

  ctx.clearRect(0, 0, w, h);

  // Achtergrond cirkel
  ctx.beginPath();
  ctx.arc(cx, cy, maxR, 0, Math.PI * 2);
  ctx.fillStyle = '#0d1220';
  ctx.fill();

  // Concentrische ringen
  const rings = 5;
  for (let i = rings; i >= 1; i--) {
    const r = (maxR / rings) * i;
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, Math.PI * 2);
    ctx.strokeStyle = 'rgba(0, 212, 255, 0.12)';
    ctx.lineWidth = 1;
    ctx.stroke();
  }

  // Kruis in het midden
  ctx.beginPath();
  ctx.moveTo(cx - maxR, cy);
  ctx.lineTo(cx + maxR, cy);
  ctx.moveTo(cx, cy - maxR);
  ctx.lineTo(cx, cy + maxR);
  ctx.strokeStyle = 'rgba(0, 212, 255, 0.08)';
  ctx.lineWidth = 1;
  ctx.stroke();

  // Bullseye punt
  ctx.beginPath();
  ctx.arc(cx, cy, 4, 0, Math.PI * 2);
  ctx.fillStyle = 'rgba(0, 212, 255, 0.5)';
  ctx.fill();

  // Hits tekenen
  const now = Date.now();
  hits.forEach((hit, idx) => {
    // Positie omrekenen naar canvas coordinaten
    const hx = cx + (hit.x - 0.5) * maxR * 2;
    const hy = cy + (hit.y - 0.5) * maxR * 2;

    // Kleur op basis van recentheid
    const age = now - hit.time;
    let color;
    if (idx === hits.length - 1) {
      color = '#ff3333'; // Laatste hit = rood
    } else if (age < 30000) {
      color = '#ff8800'; // Recent = oranje
    } else {
      color = '#777777'; // Oud = grijs
    }

    // Intensiteit-gebaseerde grootte
    const baseR = 5;
    const intR = Math.min(hit.intensity / 500, 3);
    const radius = baseR + intR;

    // Gloed effect voor laatste hit
    if (idx === hits.length - 1) {
      ctx.beginPath();
      ctx.arc(hx, hy, radius + 8, 0, Math.PI * 2);
      const glow = ctx.createRadialGradient(hx, hy, radius, hx, hy, radius + 8);
      glow.addColorStop(0, 'rgba(255, 51, 51, 0.4)');
      glow.addColorStop(1, 'rgba(255, 51, 51, 0)');
      ctx.fillStyle = glow;
      ctx.fill();
    }

    // Hit punt
    ctx.beginPath();
    ctx.arc(hx, hy, radius, 0, Math.PI * 2);
    ctx.fillStyle = color;
    ctx.fill();

    // Nummer label bij de hit
    ctx.fillStyle = '#fff';
    ctx.font = '9px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText(hit.num, hx, hy - radius - 4);
  });

  // Gemiddeld inslagpunt tonen (als er hits zijn)
  if (hits.length >= 2) {
    const aX = hits.reduce((s, h) => s + h.x, 0) / hits.length;
    const aY = hits.reduce((s, h) => s + h.y, 0) / hits.length;
    const ax = cx + (aX - 0.5) * maxR * 2;
    const ay = cy + (aY - 0.5) * maxR * 2;

    // Gemiddeld punt kruisje
    ctx.strokeStyle = 'rgba(0, 212, 255, 0.7)';
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.moveTo(ax - 8, ay);
    ctx.lineTo(ax + 8, ay);
    ctx.moveTo(ax, ay - 8);
    ctx.lineTo(ax, ay + 8);
    ctx.stroke();

    // Groepering cirkel
    let maxDist = 0;
    hits.forEach(h => {
      const dx = (h.x - aX) * maxR * 2;
      const dy = (h.y - aY) * maxR * 2;
      const d = Math.sqrt(dx * dx + dy * dy);
      if (d > maxDist) maxDist = d;
    });

    ctx.beginPath();
    ctx.arc(ax, ay, maxDist, 0, Math.PI * 2);
    ctx.strokeStyle = 'rgba(0, 212, 255, 0.25)';
    ctx.lineWidth = 1;
    ctx.setLineDash([4, 4]);
    ctx.stroke();
    ctx.setLineDash([]);
  }
}

// ---- Statistieken Bijwerken ----
function updateStats() {
  document.getElementById('shotCount').textContent = hits.length;

  if (hits.length === 0) {
    document.getElementById('lastHitPos').textContent = '--';
    document.getElementById('groupSize').textContent = '--';
    document.getElementById('avgOffset').textContent = '--';
    document.getElementById('spreadX').textContent = '--';
    document.getElementById('spreadY').textContent = '--';
    return;
  }

  const last = hits[hits.length - 1];
  // Positie in mm relatief tot centrum
  const lxMM = ((last.x - 0.5) * 120).toFixed(0);
  const lyMM = ((last.y - 0.5) * 120).toFixed(0);
  document.getElementById('lastHitPos').textContent = lxMM + ',' + lyMM;

  if (hits.length >= 2) {
    // Groepering berekenen
    const avgX = hits.reduce((s, h) => s + h.x, 0) / hits.length;
    const avgY = hits.reduce((s, h) => s + h.y, 0) / hits.length;

    // Max spread in mm
    let maxDist = 0;
    let sumDist = 0;
    let minX = 1, maxXv = 0, minY = 1, maxYv = 0;
    hits.forEach(h => {
      const dx = (h.x - avgX) * 120;
      const dy = (h.y - avgY) * 120;
      const d = Math.sqrt(dx * dx + dy * dy);
      sumDist += d;
      if (d > maxDist) maxDist = d;
      if (h.x < minX) minX = h.x;
      if (h.x > maxXv) maxXv = h.x;
      if (h.y < minY) minY = h.y;
      if (h.y > maxYv) maxYv = h.y;
    });

    document.getElementById('groupSize').textContent = (maxDist * 2).toFixed(0) + 'mm';
    document.getElementById('avgOffset').textContent = (sumDist / hits.length).toFixed(1) + 'mm';
    document.getElementById('spreadX').textContent = ((maxXv - minX) * 120).toFixed(0) + 'mm';
    document.getElementById('spreadY').textContent = ((maxYv - minY) * 120).toFixed(0) + 'mm';

    // Afwijking van centrum
    const centerOffX = ((avgX - 0.5) * 120).toFixed(0);
    const centerOffY = ((avgY - 0.5) * 120).toFixed(0);
  } else {
    document.getElementById('groupSize').textContent = '--';
    document.getElementById('avgOffset').textContent = '--';
    document.getElementById('spreadX').textContent = '--';
    document.getElementById('spreadY').textContent = '--';
  }
}

// ---- Log ----
function addLogEntry(num, x, y, intensity) {
  const log = document.getElementById('logSection');
  const entry = document.createElement('div');
  entry.className = 'log-entry';
  const xMM = ((x - 0.5) * 120).toFixed(0);
  const yMM = ((y - 0.5) * 120).toFixed(0);
  entry.innerHTML =
    '<span class="log-num">#' + num + '</span>' +
    '<span class="log-pos">X:' + xMM + ' Y:' + yMM + 'mm</span>' +
    '<span class="log-int">' + intensity + '</span>';
  log.appendChild(entry);
  log.scrollTop = log.scrollHeight;
}

function clearLog() {
  const log = document.getElementById('logSection');
  log.innerHTML = '<div class="log-header">Hit Log</div>';
}

// ---- Controls ----
function resetSession() {
  if (!connected) return;
  if (confirm('Sessie resetten? Alle hits worden gewist.')) {
    ws.send('reset');
    hits.length = 0;
    updateStats();
    drawTarget();
    clearLog();
  }
}

function calibrate() {
  if (!connected) return;
  showNotification('Kalibreren... niet aanraken!');
  ws.send('calibrate');
}

function togglePause() {
  if (!connected) return;
  ws.send(paused ? 'resume' : 'pause');
}

function updatePauseButton() {
  document.getElementById('btnPause').textContent = paused ? 'Hervat' : 'Pauze';
}

function showNotification(msg) {
  // Eenvoudige notificatie via status text
  const st = document.getElementById('statusText');
  const orig = st.textContent;
  st.textContent = msg;
  st.style.color = '#00d4ff';
  setTimeout(() => {
    st.textContent = orig;
    st.style.color = '';
  }, 2000);
}

// ---- Init ----
drawTarget();
connectWS();
</script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}
