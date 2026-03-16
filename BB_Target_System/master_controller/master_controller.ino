/**
 * ============================================================================
 * RAF RTT TRAINING SYSTEM - MASTER CONTROLLER FIRMWARE
 * ============================================================================
 *
 * Versie: 1.0.0
 *
 * Dit is de master controller die alle targets beheert.
 * Features:
 * - ESP-NOW communicatie met targets
 * - WiFi Access Point met webserver
 * - WebSocket voor real-time updates
 * - 4 Game modi (Free Play, Sequence, Random, Shoot/No Shoot)
 * - Timer, stopwatch en puntentelling
 * - Top 3 highscores opgeslagen in EEPROM
 *
 * INSTALLATIE:
 * 1. Installeer ESP32 boards in Arduino IDE
 * 2. Installeer libraries: ArduinoJson, ESPAsyncWebServer, AsyncTCP
 * 3. Selecteer ESP32-S3 board
 * 4. Upload naar ESP32-S3
 *
 * ============================================================================
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "config.h"

// ============================================================================
// WEBINTERFACE HTML/CSS/JS (PROGMEM)
// ============================================================================
// De webinterface wordt apart in data/index.html geladen
// Hieronder staat een compact fallback

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Raf RTT Training System</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,-apple-system,sans-serif;background:#0a0a0f;color:#fff;min-height:100vh}
.container{max-width:1400px;margin:0 auto;padding:20px}
header{background:linear-gradient(135deg,#1a1a2e 0%,#16213e 100%);padding:20px;border-radius:16px;margin-bottom:20px;display:flex;justify-content:space-between;align-items:center}
h1{font-size:2rem;background:linear-gradient(90deg,#00ff88,#00d4ff);-webkit-background-clip:text;-webkit-text-fill-color:transparent}
.status-dot{width:12px;height:12px;border-radius:50%;display:inline-block;margin-right:8px}
.status-dot.online{background:#00ff88;box-shadow:0 0 10px #00ff88}
.status-dot.offline{background:#ff4444}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px}
.card{background:linear-gradient(145deg,#1a1a2e,#0f0f1a);border-radius:16px;padding:20px;border:1px solid #2a2a4a}
.card h2{font-size:1.2rem;margin-bottom:15px;color:#00d4ff}
.targets-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}
.target{aspect-ratio:1;border-radius:12px;display:flex;flex-direction:column;align-items:center;justify-content:center;cursor:pointer;transition:all 0.2s;border:2px solid transparent}
.target.inactive{background:#1a1a2e}
.target.active{background:#00ff8820;border-color:#00ff88;box-shadow:0 0 20px #00ff8840}
.target.hit{background:#ff880020;border-color:#ff8800;animation:pulse 0.3s}
.target.noshoot{background:#ff000020;border-color:#ff0000}
.target.offline{background:#333;opacity:0.5}
.target-num{font-size:2rem;font-weight:bold}
@keyframes pulse{0%,100%{transform:scale(1)}50%{transform:scale(1.1)}}
.timer-display{font-size:4rem;font-weight:bold;text-align:center;font-family:'Courier New',monospace;background:linear-gradient(90deg,#00ff88,#00d4ff);-webkit-background-clip:text;-webkit-text-fill-color:transparent}
.score-display{font-size:3rem;text-align:center;color:#ffd700}
.btn{padding:12px 24px;border:none;border-radius:8px;font-size:1rem;font-weight:600;cursor:pointer;transition:all 0.2s}
.btn-primary{background:linear-gradient(90deg,#00ff88,#00d4ff);color:#000}
.btn-danger{background:#ff4444;color:#fff}
.btn-warning{background:#ff8800;color:#000}
.btn-secondary{background:#333;color:#fff}
.btn:hover{transform:translateY(-2px);box-shadow:0 4px 15px rgba(0,255,136,0.3)}
.btn:disabled{opacity:0.5;cursor:not-allowed;transform:none}
.controls{display:flex;gap:10px;flex-wrap:wrap;justify-content:center;margin:15px 0}
.mode-select{display:flex;gap:10px;flex-wrap:wrap}
.mode-btn{flex:1;min-width:120px;padding:15px;border:2px solid #2a2a4a;border-radius:12px;background:transparent;color:#fff;cursor:pointer;transition:all 0.2s}
.mode-btn.active{border-color:#00ff88;background:#00ff8810}
.mode-btn:hover{border-color:#00d4ff}
.highscores{list-style:none}
.highscores li{display:flex;justify-content:space-between;padding:10px;border-bottom:1px solid #2a2a4a}
.highscores li:nth-child(1) .rank{color:#ffd700}
.highscores li:nth-child(2) .rank{color:#c0c0c0}
.highscores li:nth-child(3) .rank{color:#cd7f32}
.numpad{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;max-width:250px;margin:0 auto}
.numpad button{padding:20px;font-size:1.5rem;border:none;border-radius:8px;background:#2a2a4a;color:#fff;cursor:pointer}
.numpad button:hover{background:#3a3a5a}
.numpad button:active{background:#00ff88;color:#000}
.settings-group{margin-bottom:15px}
.settings-group label{display:block;margin-bottom:5px;color:#888}
.settings-group input,.settings-group select{width:100%;padding:10px;border:1px solid #2a2a4a;border-radius:8px;background:#1a1a2e;color:#fff}
.log{height:200px;overflow-y:auto;background:#0a0a0f;border-radius:8px;padding:10px;font-family:monospace;font-size:0.85rem}
.log-entry{padding:2px 0;border-bottom:1px solid #1a1a2e}
.log-entry.hit{color:#00ff88}
.log-entry.miss{color:#ff4444}
.log-entry.info{color:#00d4ff}
.connection-status{position:fixed;bottom:20px;right:20px;padding:10px 20px;border-radius:8px;font-weight:600}
.connection-status.connected{background:#00ff8820;color:#00ff88;border:1px solid #00ff88}
.connection-status.disconnected{background:#ff444420;color:#ff4444;border:1px solid #ff4444}
@media(max-width:768px){.timer-display{font-size:2.5rem}.score-display{font-size:2rem}.targets-grid{grid-template-columns:repeat(2,1fr)}}
</style></head>
<body>
<div class="container">
<header>
<h1>Raf RTT Training System</h1>
<div><span class="status-dot" id="wsStatus"></span><span id="wsStatusText">Verbinden...</span></div>
</header>

<div class="grid">
<!-- Timer & Score -->
<div class="card">
<h2>Timer & Score</h2>
<div class="timer-display" id="timer">00:00.0</div>
<div class="score-display" id="score">0</div>
<div class="controls">
<button class="btn btn-primary" id="btnStart" onclick="startGame()">START</button>
<button class="btn btn-warning" id="btnPause" onclick="pauseGame()" disabled>PAUZE</button>
<button class="btn btn-danger" id="btnStop" onclick="stopGame()" disabled>STOP</button>
<button class="btn btn-secondary" onclick="resetGame()">RESET</button>
</div>
<div class="settings-group">
<label>Speeltijd (seconden)</label>
<input type="number" id="gameTime" value="60" min="10" max="300">
</div>
</div>

<!-- Targets -->
<div class="card">
<h2>Targets</h2>
<div class="targets-grid" id="targetsGrid"></div>
<p style="text-align:center;margin-top:10px;color:#666" id="targetCount">0/8 targets online</p>
</div>

<!-- Game Modus -->
<div class="card">
<h2>Game Modus</h2>
<div class="mode-select">
<button class="mode-btn active" data-mode="freeplay" onclick="setMode('freeplay')">
<strong>Free Play</strong><br><small>Alles aan</small>
</button>
<button class="mode-btn" data-mode="sequence" onclick="setMode('sequence')">
<strong>Sequence</strong><br><small>Volgorde</small>
</button>
<button class="mode-btn" data-mode="random" onclick="setMode('random')">
<strong>Random</strong><br><small>Willekeurig</small>
</button>
<button class="mode-btn" data-mode="shootnoshoot" onclick="setMode('shootnoshoot')">
<strong>Shoot/No Shoot</strong><br><small>Groen vs Rood</small>
</button>
</div>
<div class="settings-group" style="margin-top:15px">
<label>Target tijd (seconden) - voor Sequence/Random</label>
<input type="number" id="targetTime" value="3" min="1" max="10">
</div>
</div>

<!-- Numpad -->
<div class="card">
<h2>Handmatig Target</h2>
<div class="numpad">
<button onclick="activateTarget(1)">1</button>
<button onclick="activateTarget(2)">2</button>
<button onclick="activateTarget(3)">3</button>
<button onclick="activateTarget(4)">4</button>
<button onclick="activateTarget(5)">5</button>
<button onclick="activateTarget(6)">6</button>
<button onclick="activateTarget(7)">7</button>
<button onclick="activateTarget(8)">8</button>
<button onclick="activateAll()">ALL</button>
</div>
</div>

<!-- Highscores -->
<div class="card">
<h2>Top 3 van Vandaag</h2>
<ol class="highscores" id="highscores">
<li><span class="rank">#1</span><span>---</span><span>0</span></li>
<li><span class="rank">#2</span><span>---</span><span>0</span></li>
<li><span class="rank">#3</span><span>---</span><span>0</span></li>
</ol>
<div class="controls">
<button class="btn btn-secondary" onclick="loadHighscores()">Vernieuwen</button>
<button class="btn btn-danger" onclick="clearHighscores()">Wissen</button>
</div>
<div class="settings-group" style="margin-top:15px">
<label>Speler naam</label>
<input type="text" id="playerName" placeholder="Naam" maxlength="20">
</div>
</div>

<!-- Event Log -->
<div class="card">
<h2>Event Log</h2>
<div class="log" id="log"></div>
<button class="btn btn-secondary" onclick="clearLog()" style="margin-top:10px">Log Wissen</button>
</div>
</div>
</div>

<div class="connection-status disconnected" id="connectionStatus">Niet verbonden</div>

<script>
let ws;
let gameState = {running:false,paused:false,mode:'freeplay',score:0,time:0,targets:{}};
const maxTargets = 8;

// WebSocket verbinding
function connect() {
    ws = new WebSocket('ws://'+location.hostname+':81/ws');
    ws.onopen = () => {
        document.getElementById('wsStatus').className = 'status-dot online';
        document.getElementById('wsStatusText').textContent = 'Verbonden';
        document.getElementById('connectionStatus').className = 'connection-status connected';
        document.getElementById('connectionStatus').textContent = 'Verbonden';
        log('Verbonden met master controller', 'info');
        ws.send(JSON.stringify({cmd:'getState'}));
    };
    ws.onclose = () => {
        document.getElementById('wsStatus').className = 'status-dot offline';
        document.getElementById('wsStatusText').textContent = 'Niet verbonden';
        document.getElementById('connectionStatus').className = 'connection-status disconnected';
        document.getElementById('connectionStatus').textContent = 'Niet verbonden';
        log('Verbinding verbroken, opnieuw verbinden...', 'miss');
        setTimeout(connect, 2000);
    };
    ws.onmessage = (e) => {
        try {
            const data = JSON.parse(e.data);
            handleMessage(data);
        } catch(err) { console.error('Parse error:', err); }
    };
    ws.onerror = (e) => console.error('WebSocket error:', e);
}

function handleMessage(data) {
    switch(data.event) {
        case 'state':
            gameState = {...gameState, ...data.data};
            updateUI();
            break;
        case 'hit':
            handleHit(data.data);
            break;
        case 'timer':
            document.getElementById('timer').textContent = formatTime(data.data.time);
            gameState.time = data.data.time;
            break;
        case 'score':
            document.getElementById('score').textContent = data.data.score;
            gameState.score = data.data.score;
            break;
        case 'targetUpdate':
            updateTarget(data.data);
            break;
        case 'gameEnd':
            gameEnded(data.data);
            break;
        case 'highscores':
            displayHighscores(data.data);
            break;
    }
}

function handleHit(data) {
    const target = document.querySelector(`[data-target="${data.target}"]`);
    if (target) {
        target.classList.add('hit');
        setTimeout(() => target.classList.remove('hit'), 300);
    }
    log(`Target ${data.target} geraakt! +${data.points} punten`, data.points > 0 ? 'hit' : 'miss');
    document.getElementById('score').textContent = data.totalScore;
}

function updateTarget(data) {
    const target = document.querySelector(`[data-target="${data.id}"]`);
    if (target) {
        target.className = 'target ' + data.state;
        if (data.color) {
            target.style.setProperty('--target-color', `rgb(${data.color.r},${data.color.g},${data.color.b})`);
        }
    }
    gameState.targets[data.id] = data;
    updateTargetCount();
}

function updateTargetCount() {
    const online = Object.values(gameState.targets).filter(t => t.state !== 'offline').length;
    document.getElementById('targetCount').textContent = `${online}/${maxTargets} targets online`;
}

function updateUI() {
    // Buttons
    document.getElementById('btnStart').disabled = gameState.running;
    document.getElementById('btnPause').disabled = !gameState.running;
    document.getElementById('btnStop').disabled = !gameState.running;
    document.getElementById('btnPause').textContent = gameState.paused ? 'HERVAT' : 'PAUZE';

    // Mode buttons
    document.querySelectorAll('.mode-btn').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.mode === gameState.mode);
    });

    // Timer & Score
    document.getElementById('timer').textContent = formatTime(gameState.time);
    document.getElementById('score').textContent = gameState.score;

    // Targets
    initTargets();
}

function initTargets() {
    const grid = document.getElementById('targetsGrid');
    grid.innerHTML = '';
    for (let i = 1; i <= maxTargets; i++) {
        const target = document.createElement('div');
        target.className = 'target ' + (gameState.targets[i]?.state || 'offline');
        target.dataset.target = i;
        target.innerHTML = `<span class="target-num">${i}</span>`;
        target.onclick = () => activateTarget(i);
        grid.appendChild(target);
    }
}

function formatTime(ms) {
    const totalSec = Math.floor(ms / 1000);
    const min = Math.floor(totalSec / 60);
    const sec = totalSec % 60;
    const tenths = Math.floor((ms % 1000) / 100);
    return `${min.toString().padStart(2,'0')}:${sec.toString().padStart(2,'0')}.${tenths}`;
}

function send(data) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify(data));
    }
}

function startGame() {
    const playerName = document.getElementById('playerName').value || 'Speler';
    const gameTime = parseInt(document.getElementById('gameTime').value) || 60;
    const targetTime = parseInt(document.getElementById('targetTime').value) || 3;
    send({cmd:'start', player: playerName, gameTime: gameTime, targetTime: targetTime});
    log(`Game gestart - ${gameState.mode} mode`, 'info');
}

function pauseGame() {
    send({cmd:'pause'});
    log(gameState.paused ? 'Game hervat' : 'Game gepauzeerd', 'info');
}

function stopGame() {
    send({cmd:'stop'});
    log('Game gestopt', 'info');
}

function resetGame() {
    send({cmd:'reset'});
    log('Game gereset', 'info');
}

function setMode(mode) {
    send({cmd:'setMode', mode: mode});
    gameState.mode = mode;
    document.querySelectorAll('.mode-btn').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.mode === mode);
    });
    log(`Modus: ${mode}`, 'info');
}

function activateTarget(id) {
    send({cmd:'activateTarget', target: id});
}

function activateAll() {
    send({cmd:'activateAll'});
}

function loadHighscores() {
    send({cmd:'getHighscores'});
}

function clearHighscores() {
    if (confirm('Weet je zeker dat je alle highscores wilt wissen?')) {
        send({cmd:'clearHighscores'});
        log('Highscores gewist', 'info');
    }
}

function displayHighscores(scores) {
    const list = document.getElementById('highscores');
    list.innerHTML = '';
    for (let i = 0; i < 3; i++) {
        const s = scores[i] || {name: '---', score: 0};
        const li = document.createElement('li');
        li.innerHTML = `<span class="rank">#${i+1}</span><span>${s.name}</span><span>${s.score}</span>`;
        list.appendChild(li);
    }
}

function gameEnded(data) {
    log(`Game afgelopen! Score: ${data.score}`, 'info');
    if (data.isHighscore) {
        log('NIEUWE HIGHSCORE!', 'hit');
    }
    loadHighscores();
}

function log(msg, type = '') {
    const logDiv = document.getElementById('log');
    const entry = document.createElement('div');
    entry.className = 'log-entry ' + type;
    const time = new Date().toLocaleTimeString();
    entry.textContent = `[${time}] ${msg}`;
    logDiv.insertBefore(entry, logDiv.firstChild);
    if (logDiv.children.length > 100) logDiv.lastChild.remove();
}

function clearLog() {
    document.getElementById('log').innerHTML = '';
}

// Init
connect();
initTargets();
loadHighscores();
</script>
</body></html>
)rawliteral";

// ============================================================================
// STRUCTUREN EN ENUMS
// ============================================================================

// Bericht van Target naar Master
typedef struct {
    uint8_t type;
    uint8_t targetId;
    uint16_t intensity;
    uint32_t timestamp;
    uint8_t status;
} TargetMessage;

// Bericht van Master naar Target
typedef struct {
    uint8_t cmd;
    uint8_t targetId;
    uint8_t r, g, b;
    uint8_t sound;
    uint8_t param1;
    uint8_t param2;
} MasterMessage;

// Target info
struct TargetInfo {
    bool online;
    uint8_t state;  // 0=inactive, 1=active, 2=hit, 3=noshoot
    uint32_t lastSeen;
    uint16_t lastHitIntensity;
    uint8_t macAddr[6];
};

// Highscore entry
struct HighscoreEntry {
    char name[21];
    uint32_t score;
    uint32_t date;
};

// Game modes
enum GameMode {
    MODE_FREEPLAY = 0,
    MODE_SEQUENCE = 1,
    MODE_RANDOM = 2,
    MODE_SHOOTNOSHOOT = 3
};

// Commands
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

// Sounds
enum Sounds {
    SND_NONE = 0,
    SND_HIT = 1,
    SND_MISS = 2,
    SND_START = 3,
    SND_END = 4,
    SND_CORRECT = 5,
    SND_WRONG = 6
};

// ============================================================================
// GLOBALE VARIABELEN
// ============================================================================

// Servers
AsyncWebServer server(WEB_SERVER_PORT);
AsyncWebSocket ws("/ws");

// Preferences voor opslag
Preferences preferences;

// Targets
TargetInfo targets[MAX_TARGETS + 1]; // 1-indexed
uint8_t onlineTargetCount = 0;

// Game state
volatile bool gameRunning = false;
volatile bool gamePaused = false;
GameMode currentMode = MODE_FREEPLAY;
int32_t currentScore = 0;
uint32_t gameStartTime = 0;
uint32_t gameTime = DEFAULT_GAME_TIME * 1000;
uint32_t targetTime = DEFAULT_TARGET_TIME * 1000;
uint32_t pausedTime = 0;
uint32_t lastTimerUpdate = 0;

// Sequence/Random state
uint8_t currentActiveTarget = 0;
uint8_t sequenceIndex = 0;
uint8_t sequence[MAX_TARGETS];
uint32_t targetActivatedTime = 0;
uint8_t hitsInGame = 0;
uint8_t missesInGame = 0;

// Player
char currentPlayer[21] = "Speler";

// Highscores
HighscoreEntry highscores[MAX_HIGHSCORES];

// ============================================================================
// ULTRASONIC COUNTER STATE
// ============================================================================
struct CounterState {
    bool active;
    uint16_t count;
    uint8_t ledsLit;
    uint8_t countsPerLed;
    uint8_t countsSinceLed;
    uint8_t currentRound;
    uint8_t totalRounds;
    uint8_t redTimerSeconds;
    uint8_t detectionDistance;
    bool nodeOnline;
    uint32_t lastSeen;
    String mode;  // "free", "timed", "interval"
};
CounterState counterState = {false, 0, 0, 1, 0, 0, 0, 0, 20, false, 0, "free"};

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void onEspNowReceive(const uint8_t *mac, const uint8_t *data, int len);
void onEspNowSend(const uint8_t *mac, esp_now_send_status_t status);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len);
void broadcastState();
void broadcastHit(uint8_t targetId, int points);
void broadcastTimer();
void sendToTarget(uint8_t targetId, MasterMessage &msg);
void sendToAllTargets(MasterMessage &msg);
void processHit(uint8_t targetId, uint16_t intensity);
void startGame();
void pauseGame();
void stopGame();
void resetGame();
void updateGame();
void activateNextTarget();
void activateRandomTarget();
void activateTarget(uint8_t id, bool isShootTarget = true);
void deactivateTarget(uint8_t id);
void deactivateAllTargets();
void checkTargetTimeout();
void loadHighscores();
void saveHighscores();
void checkHighscore(uint32_t score);
void generateSequence();
void broadcastCounterState();
void processCounterHit(uint8_t nodeId, uint16_t count);

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(500);

    Serial.println("\n============================================");
    Serial.println("   RAF RTT TRAINING - MASTER CONTROLLER");
    Serial.println("============================================\n");

    // Initialize targets array
    for (int i = 0; i <= MAX_TARGETS; i++) {
        targets[i].online = false;
        targets[i].state = 0;
        targets[i].lastSeen = 0;
        targets[i].lastHitIntensity = 0;
    }

    // Initialize Preferences
    preferences.begin(SCORE_NAMESPACE, false);
    loadHighscores();

    // WiFi initialiseren als AP
    Serial.println("WiFi Access Point starten...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);

    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());

    // ESP-NOW initialiseren
    if (esp_now_init() != ESP_OK) {
        Serial.println("FOUT: ESP-NOW init mislukt!");
        return;
    }

    // Callbacks registreren
    esp_now_register_recv_cb(onEspNowReceive);
    esp_now_register_send_cb(onEspNowSend);

    // Broadcast peer toevoegen
    esp_now_peer_info_t peerInfo = {};
    memset(peerInfo.peer_addr, 0xFF, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    Serial.println("ESP-NOW geinitialiseerd");

    // WebSocket handler
    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);

    // Webserver routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });

    // Counter pagina route
    // Opmerking: counter.html kan via SPIFFS worden geserveerd als het bestand
    // is geupload naar de ESP32-S3. Zie de web_interface/counter.html file.
    // Voor SPIFFS: upload counter.html naar /data/counter.html en uncomment:
    // server.serveStatic("/counter.html", SPIFFS, "/counter.html");
    //
    // Zonder SPIFFS: open counter.html direct in browser vanaf een computer
    // die verbonden is met het Raf_RTT_Training WiFi netwerk.

    server.on("/api/counter", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<512> doc;
        doc["active"] = counterState.active;
        doc["count"] = counterState.count;
        doc["ledsLit"] = counterState.ledsLit;
        doc["countsPerLed"] = counterState.countsPerLed;
        doc["countsSinceLed"] = counterState.countsSinceLed;
        doc["currentRound"] = counterState.currentRound;
        doc["totalRounds"] = counterState.totalRounds;
        doc["redTimerSeconds"] = counterState.redTimerSeconds;
        doc["detectionDistance"] = counterState.detectionDistance;
        doc["nodeOnline"] = counterState.nodeOnline;
        doc["mode"] = counterState.mode;
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<1024> doc;
        doc["running"] = gameRunning;
        doc["paused"] = gamePaused;
        doc["mode"] = currentMode;
        doc["score"] = currentScore;
        doc["time"] = gameRunning ? (millis() - gameStartTime - pausedTime) : 0;

        JsonArray targetsArr = doc.createNestedArray("targets");
        for (int i = 1; i <= MAX_TARGETS; i++) {
            JsonObject t = targetsArr.createNestedObject();
            t["id"] = i;
            t["online"] = targets[i].online;
            t["state"] = targets[i].state;
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Start server
    server.begin();
    Serial.println("Webserver gestart op poort 80");
    Serial.printf("Verbind met WiFi: %s\n", WIFI_SSID);
    Serial.printf("Open browser: http://%s\n", WiFi.softAPIP().toString().c_str());

    Serial.println("\nMaster Controller gereed!");
    Serial.println("Wacht op targets...\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // WebSocket cleanup
    ws.cleanupClients();

    // Game loop
    if (gameRunning && !gamePaused) {
        updateGame();
    }

    // Target timeout check (elke seconde)
    static unsigned long lastTimeoutCheck = 0;
    if (millis() - lastTimeoutCheck > 1000) {
        lastTimeoutCheck = millis();
        checkTargetTimeout();
    }

    // Timer broadcast (10x per seconde)
    if (gameRunning && !gamePaused) {
        if (millis() - lastTimerUpdate >= 100) {
            lastTimerUpdate = millis();
            broadcastTimer();
        }
    }
}

// ============================================================================
// ESP-NOW HANDLERS
// ============================================================================

void onEspNowReceive(const uint8_t *mac, const uint8_t *data, int len) {
    if (len != sizeof(TargetMessage)) return;

    TargetMessage *msg = (TargetMessage *)data;
    uint8_t targetId = msg->targetId;

    // Counter node (ID 9+)
    if (targetId >= 9) {
        counterState.nodeOnline = true;
        counterState.lastSeen = millis();

        switch (msg->type) {
            case MSG_HIT: // Detectie event
                if (counterState.active) {
                    processCounterHit(targetId, msg->intensity);
                }
                break;
            case MSG_HEARTBEAT:
                // Counter node heartbeat - update online status
                break;
            case 10: // MSG_COUNTER_UPDATE
                counterState.count = msg->intensity;
                broadcastCounterState();
                break;
        }
        return;
    }

    if (targetId < 1 || targetId > MAX_TARGETS) return;

    // Update target info
    targets[targetId].lastSeen = millis();
    memcpy(targets[targetId].macAddr, mac, 6);

    if (!targets[targetId].online) {
        targets[targetId].online = true;
        onlineTargetCount++;
        Serial.printf("Target %d online! Totaal: %d\n", targetId, onlineTargetCount);

        // Stuur state update naar webclients
        StaticJsonDocument<256> doc;
        doc["event"] = "targetUpdate";
        JsonObject d = doc.createNestedObject("data");
        d["id"] = targetId;
        d["state"] = "inactive";
        d["online"] = true;

        String json;
        serializeJson(doc, json);
        ws.textAll(json);
    }

    // Verwerk bericht type
    switch (msg->type) {
        case MSG_HIT:
            if (DEBUG_ENABLED) {
                Serial.printf("HIT ontvangen van target %d (intensity: %d)\n",
                              targetId, msg->intensity);
            }
            targets[targetId].lastHitIntensity = msg->intensity;
            processHit(targetId, msg->intensity);
            break;

        case MSG_HEARTBEAT:
            targets[targetId].state = msg->status;
            break;

        case MSG_PONG:
            Serial.printf("PONG van target %d\n", targetId);
            break;
    }
}

void onEspNowSend(const uint8_t *mac, esp_now_send_status_t status) {
    // Optional: log send status
}

// ============================================================================
// WEBSOCKET HANDLERS
// ============================================================================

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u verbonden\n", client->id());
            // Stuur huidige state
            broadcastState();
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;

        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;

        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->opcode != WS_TEXT) return;

    data[len] = 0; // Null terminate

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, (char *)data);
    if (error) {
        Serial.println("JSON parse error");
        return;
    }

    const char *cmd = doc["cmd"];
    if (!cmd) return;

    if (strcmp(cmd, "start") == 0) {
        if (doc.containsKey("player")) {
            strncpy(currentPlayer, doc["player"], 20);
            currentPlayer[20] = '\0';
        }
        if (doc.containsKey("gameTime")) {
            gameTime = doc["gameTime"].as<uint32_t>() * 1000;
        }
        if (doc.containsKey("targetTime")) {
            targetTime = doc["targetTime"].as<uint32_t>() * 1000;
        }
        startGame();
    }
    else if (strcmp(cmd, "pause") == 0) {
        pauseGame();
    }
    else if (strcmp(cmd, "stop") == 0) {
        stopGame();
    }
    else if (strcmp(cmd, "reset") == 0) {
        resetGame();
    }
    else if (strcmp(cmd, "setMode") == 0) {
        const char *mode = doc["mode"];
        if (strcmp(mode, "freeplay") == 0) currentMode = MODE_FREEPLAY;
        else if (strcmp(mode, "sequence") == 0) currentMode = MODE_SEQUENCE;
        else if (strcmp(mode, "random") == 0) currentMode = MODE_RANDOM;
        else if (strcmp(mode, "shootnoshoot") == 0) currentMode = MODE_SHOOTNOSHOOT;
        broadcastState();
    }
    else if (strcmp(cmd, "activateTarget") == 0) {
        uint8_t target = doc["target"];
        activateTarget(target);
    }
    else if (strcmp(cmd, "activateAll") == 0) {
        for (int i = 1; i <= MAX_TARGETS; i++) {
            if (targets[i].online) {
                activateTarget(i);
            }
        }
    }
    else if (strcmp(cmd, "getState") == 0) {
        broadcastState();
    }
    else if (strcmp(cmd, "getHighscores") == 0) {
        // Send highscores
        StaticJsonDocument<512> hsDoc;
        hsDoc["event"] = "highscores";
        JsonArray arr = hsDoc.createNestedArray("data");
        for (int i = 0; i < 3; i++) {
            JsonObject hs = arr.createNestedObject();
            hs["name"] = highscores[i].name;
            hs["score"] = highscores[i].score;
        }
        String json;
        serializeJson(hsDoc, json);
        ws.textAll(json);
    }
    else if (strcmp(cmd, "clearHighscores") == 0) {
        for (int i = 0; i < MAX_HIGHSCORES; i++) {
            strcpy(highscores[i].name, "---");
            highscores[i].score = 0;
            highscores[i].date = 0;
        }
        saveHighscores();
    }
    // ================================================================
    // COUNTER COMMANDO'S
    // ================================================================
    else if (strcmp(cmd, "counterStart") == 0) {
        counterState.active = true;
        counterState.count = 0;
        counterState.ledsLit = 0;
        counterState.countsSinceLed = 0;
        counterState.currentRound = 0;
        if (doc.containsKey("countsPerLed")) counterState.countsPerLed = doc["countsPerLed"];
        if (doc.containsKey("redTimer")) counterState.redTimerSeconds = doc["redTimer"];
        if (doc.containsKey("rounds")) counterState.totalRounds = doc["rounds"];
        if (doc.containsKey("distance")) counterState.detectionDistance = doc["distance"];

        // Stuur activate naar counter node
        MasterMessage msg;
        msg.cmd = CMD_ACTIVATE;
        msg.targetId = 9; // Counter node ID
        msg.sound = 1;
        msg.r = 0; msg.g = 255; msg.b = 0;
        msg.param1 = counterState.countsPerLed;
        sendToAllTargets(msg);

        Serial.println("Counter gestart");
        broadcastCounterState();
    }
    else if (strcmp(cmd, "counterStop") == 0) {
        counterState.active = false;
        MasterMessage msg;
        msg.cmd = CMD_DEACTIVATE;
        msg.targetId = 9;
        sendToAllTargets(msg);
        Serial.println("Counter gestopt");
        broadcastCounterState();
    }
    else if (strcmp(cmd, "counterReset") == 0) {
        counterState.count = 0;
        counterState.ledsLit = 0;
        counterState.countsSinceLed = 0;
        counterState.currentRound = 0;

        MasterMessage msg;
        msg.cmd = CMD_RESET;
        msg.targetId = 9;
        sendToAllTargets(msg);
        Serial.println("Counter gereset");
        broadcastCounterState();
    }
    else if (strcmp(cmd, "counterSetCountsPerLed") == 0) {
        counterState.countsPerLed = doc["value"].as<uint8_t>();

        MasterMessage msg;
        msg.cmd = 20; // CMD_SET_COUNTS_PER_LED
        msg.targetId = 9;
        msg.param1 = counterState.countsPerLed;
        sendToAllTargets(msg);
        broadcastCounterState();
    }
    else if (strcmp(cmd, "counterSetRedTimer") == 0) {
        counterState.redTimerSeconds = doc["value"].as<uint8_t>();
        broadcastCounterState();
    }
    else if (strcmp(cmd, "counterSetRounds") == 0) {
        counterState.totalRounds = doc["value"].as<uint8_t>();
        broadcastCounterState();
    }
    else if (strcmp(cmd, "counterSetDistance") == 0) {
        counterState.detectionDistance = doc["value"].as<uint8_t>();
        broadcastCounterState();
    }
    else if (strcmp(cmd, "counterSetMode") == 0) {
        counterState.mode = doc["mode"].as<String>();
        broadcastCounterState();
    }
    else if (strcmp(cmd, "counterRedTimer") == 0) {
        // Stuur rood licht commando naar counter node
        MasterMessage msg;
        msg.cmd = CMD_SET_COLOR;
        msg.targetId = 9;
        msg.r = 255; msg.g = 0; msg.b = 0; // ROOD
        sendToAllTargets(msg);
    }
    else if (strcmp(cmd, "counterTimeout") == 0) {
        // Timeout event - stuur fout geluid naar node
        MasterMessage msg;
        msg.cmd = CMD_PLAY_SOUND;
        msg.targetId = 9;
        msg.sound = SND_WRONG;
        sendToAllTargets(msg);

        // Broadcast timeout naar webclients
        StaticJsonDocument<128> timeoutDoc;
        timeoutDoc["event"] = "counterTimeout";
        JsonObject d = timeoutDoc.createNestedObject("data");
        d["count"] = counterState.count;
        String json;
        serializeJson(timeoutDoc, json);
        ws.textAll(json);
    }
    else if (strcmp(cmd, "getCounterState") == 0) {
        broadcastCounterState();
    }
}

// ============================================================================
// GAME LOGIC
// ============================================================================

void startGame() {
    if (gameRunning) return;

    Serial.println("=== GAME START ===");
    Serial.printf("Mode: %d, Player: %s\n", currentMode, currentPlayer);

    gameRunning = true;
    gamePaused = false;
    currentScore = 0;
    gameStartTime = millis();
    pausedTime = 0;
    hitsInGame = 0;
    missesInGame = 0;
    sequenceIndex = 0;

    // Reset alle targets
    deactivateAllTargets();

    // Mode-specifieke setup
    switch (currentMode) {
        case MODE_FREEPLAY:
            // Activeer alle online targets
            for (int i = 1; i <= MAX_TARGETS; i++) {
                if (targets[i].online) {
                    activateTarget(i);
                }
            }
            break;

        case MODE_SEQUENCE:
            generateSequence();
            activateNextTarget();
            break;

        case MODE_RANDOM:
            activateRandomTarget();
            break;

        case MODE_SHOOTNOSHOOT:
            activateRandomTarget();
            break;
    }

    // Start geluid naar alle targets
    MasterMessage msg;
    msg.cmd = CMD_PLAY_SOUND;
    msg.targetId = 0;
    msg.sound = SND_START;
    sendToAllTargets(msg);

    broadcastState();
}

void pauseGame() {
    if (!gameRunning) return;

    gamePaused = !gamePaused;

    if (gamePaused) {
        pausedTime = millis();
        Serial.println("Game gepauzeerd");
    } else {
        // Bereken gepauseerde tijd
        uint32_t pauseDuration = millis() - pausedTime;
        gameStartTime += pauseDuration;
        Serial.println("Game hervat");
    }

    broadcastState();
}

void stopGame() {
    if (!gameRunning) return;

    gameRunning = false;
    gamePaused = false;

    Serial.println("=== GAME STOP ===");
    Serial.printf("Score: %d, Hits: %d, Misses: %d\n",
                  currentScore, hitsInGame, missesInGame);

    // Deactiveer alle targets
    deactivateAllTargets();

    // End geluid
    MasterMessage msg;
    msg.cmd = CMD_PLAY_SOUND;
    msg.targetId = 0;
    msg.sound = SND_END;
    sendToAllTargets(msg);

    // Check highscore
    bool isHighscore = false;
    if (currentScore > highscores[2].score) {
        isHighscore = true;
        checkHighscore(currentScore);
    }

    // Broadcast game end
    StaticJsonDocument<256> doc;
    doc["event"] = "gameEnd";
    JsonObject data = doc.createNestedObject("data");
    data["score"] = currentScore;
    data["hits"] = hitsInGame;
    data["misses"] = missesInGame;
    data["isHighscore"] = isHighscore;

    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}

void resetGame() {
    gameRunning = false;
    gamePaused = false;
    currentScore = 0;
    hitsInGame = 0;
    missesInGame = 0;
    sequenceIndex = 0;

    deactivateAllTargets();

    // Reset targets
    MasterMessage msg;
    msg.cmd = CMD_RESET;
    msg.targetId = 0;
    sendToAllTargets(msg);

    broadcastState();
    Serial.println("Game gereset");
}

void updateGame() {
    uint32_t elapsed = millis() - gameStartTime;

    // Check game time
    if (elapsed >= gameTime) {
        stopGame();
        return;
    }

    // Mode-specifieke updates
    switch (currentMode) {
        case MODE_SEQUENCE:
        case MODE_RANDOM:
            // Check target timeout
            if (currentActiveTarget > 0 &&
                millis() - targetActivatedTime >= targetTime) {
                // Timeout - miss penalty
                missesInGame++;
                currentScore += POINTS_TIMEOUT;
                deactivateTarget(currentActiveTarget);

                if (currentMode == MODE_SEQUENCE) {
                    activateNextTarget();
                } else {
                    activateRandomTarget();
                }
            }
            break;

        case MODE_SHOOTNOSHOOT:
            if (currentActiveTarget > 0 &&
                millis() - targetActivatedTime >= targetTime) {
                deactivateTarget(currentActiveTarget);
                activateRandomTarget();
            }
            break;
    }
}

void processHit(uint8_t targetId, uint16_t intensity) {
    if (!gameRunning || gamePaused) {
        // Niet in game - alleen feedback sturen
        broadcastHit(targetId, 0);
        return;
    }

    int points = 0;
    bool validHit = false;

    switch (currentMode) {
        case MODE_FREEPLAY:
            if (targets[targetId].state == 1) { // Active
                points = POINTS_HIT;
                validHit = true;
                hitsInGame++;

                // Flash en geluid
                MasterMessage msg;
                msg.cmd = CMD_FLASH;
                msg.targetId = targetId;
                msg.r = 255; msg.g = 100; msg.b = 0;
                msg.param1 = 3;
                sendToTarget(targetId, msg);
            }
            break;

        case MODE_SEQUENCE:
        case MODE_RANDOM:
            if (targetId == currentActiveTarget) {
                // Correct target
                uint32_t reactionTime = millis() - targetActivatedTime;
                points = POINTS_HIT;

                // Bonus voor snelle reactie
                if (reactionTime < 1000) {
                    points += POINTS_BONUS_FAST;
                }

                validHit = true;
                hitsInGame++;
                deactivateTarget(targetId);

                if (currentMode == MODE_SEQUENCE) {
                    activateNextTarget();
                } else {
                    activateRandomTarget();
                }
            } else if (targets[targetId].state == 1) {
                // Verkeerd target geraakt
                points = POINTS_MISS;
                missesInGame++;
            }
            break;

        case MODE_SHOOTNOSHOOT:
            if (targets[targetId].state == 1) { // Shoot target (groen)
                points = POINTS_HIT;
                validHit = true;
                hitsInGame++;
                deactivateTarget(targetId);
                activateRandomTarget();
            } else if (targets[targetId].state == 3) { // No-shoot (rood)
                points = POINTS_NOSHOOT_HIT;
                missesInGame++;

                // Fout geluid
                MasterMessage msg;
                msg.cmd = CMD_PLAY_SOUND;
                msg.targetId = targetId;
                msg.sound = SND_WRONG;
                sendToTarget(targetId, msg);
            }
            break;
    }

    currentScore += points;
    broadcastHit(targetId, points);

    Serial.printf("Hit target %d: %d punten (totaal: %d)\n",
                  targetId, points, currentScore);
}

void activateTarget(uint8_t id, bool isShootTarget) {
    if (id < 1 || id > MAX_TARGETS || !targets[id].online) return;

    MasterMessage msg;
    msg.cmd = CMD_ACTIVATE;
    msg.targetId = id;
    msg.sound = 1;

    if (isShootTarget) {
        msg.r = COLOR_SHOOT_R;
        msg.g = COLOR_SHOOT_G;
        msg.b = COLOR_SHOOT_B;
        targets[id].state = 1;
    } else {
        msg.r = COLOR_NOSHOOT_R;
        msg.g = COLOR_NOSHOOT_G;
        msg.b = COLOR_NOSHOOT_B;
        targets[id].state = 3;
    }

    sendToTarget(id, msg);
    currentActiveTarget = id;
    targetActivatedTime = millis();

    // Update UI
    StaticJsonDocument<256> doc;
    doc["event"] = "targetUpdate";
    JsonObject data = doc.createNestedObject("data");
    data["id"] = id;
    data["state"] = isShootTarget ? "active" : "noshoot";
    JsonObject color = data.createNestedObject("color");
    color["r"] = msg.r;
    color["g"] = msg.g;
    color["b"] = msg.b;

    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}

void deactivateTarget(uint8_t id) {
    if (id < 1 || id > MAX_TARGETS) return;

    MasterMessage msg;
    msg.cmd = CMD_DEACTIVATE;
    msg.targetId = id;

    sendToTarget(id, msg);
    targets[id].state = 0;

    if (currentActiveTarget == id) {
        currentActiveTarget = 0;
    }

    // Update UI
    StaticJsonDocument<128> doc;
    doc["event"] = "targetUpdate";
    JsonObject data = doc.createNestedObject("data");
    data["id"] = id;
    data["state"] = "inactive";

    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}

void deactivateAllTargets() {
    MasterMessage msg;
    msg.cmd = CMD_DEACTIVATE;
    msg.targetId = 0; // Broadcast

    sendToAllTargets(msg);

    for (int i = 1; i <= MAX_TARGETS; i++) {
        targets[i].state = 0;
    }
    currentActiveTarget = 0;
}

void activateNextTarget() {
    if (sequenceIndex >= onlineTargetCount) {
        // Sequence compleet - opnieuw beginnen
        sequenceIndex = 0;
        generateSequence();
    }

    uint8_t nextTarget = sequence[sequenceIndex++];
    if (targets[nextTarget].online) {
        activateTarget(nextTarget);
    } else {
        // Skip offline target
        activateNextTarget();
    }
}

void activateRandomTarget() {
    // Maak lijst van online targets
    uint8_t onlineList[MAX_TARGETS];
    uint8_t count = 0;

    for (int i = 1; i <= MAX_TARGETS; i++) {
        if (targets[i].online && i != currentActiveTarget) {
            onlineList[count++] = i;
        }
    }

    if (count == 0) return;

    // Kies random target
    uint8_t idx = random(count);
    uint8_t target = onlineList[idx];

    // Voor shoot/no-shoot: 70% shoot, 30% no-shoot
    bool isShoot = (currentMode != MODE_SHOOTNOSHOOT) || (random(100) < 70);

    activateTarget(target, isShoot);
}

void generateSequence() {
    // Maak lijst van online targets
    uint8_t count = 0;
    for (int i = 1; i <= MAX_TARGETS; i++) {
        if (targets[i].online) {
            sequence[count++] = i;
        }
    }

    // Fisher-Yates shuffle
    for (int i = count - 1; i > 0; i--) {
        int j = random(i + 1);
        uint8_t temp = sequence[i];
        sequence[i] = sequence[j];
        sequence[j] = temp;
    }
}

// ============================================================================
// COMMUNICATIE
// ============================================================================

void sendToTarget(uint8_t targetId, MasterMessage &msg) {
    if (!targets[targetId].online) return;

    // Broadcast (targets filteren zelf op ID)
    uint8_t broadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(broadcast, (uint8_t *)&msg, sizeof(msg));
}

void sendToAllTargets(MasterMessage &msg) {
    uint8_t broadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(broadcast, (uint8_t *)&msg, sizeof(msg));
}

void broadcastState() {
    StaticJsonDocument<1024> doc;
    doc["event"] = "state";

    JsonObject data = doc.createNestedObject("data");
    data["running"] = gameRunning;
    data["paused"] = gamePaused;
    data["score"] = currentScore;

    const char *modes[] = {"freeplay", "sequence", "random", "shootnoshoot"};
    data["mode"] = modes[currentMode];

    uint32_t elapsed = 0;
    if (gameRunning) {
        elapsed = millis() - gameStartTime;
    }
    data["time"] = elapsed;

    JsonObject targetsObj = data.createNestedObject("targets");
    for (int i = 1; i <= MAX_TARGETS; i++) {
        JsonObject t = targetsObj.createNestedObject(String(i));
        t["online"] = targets[i].online;
        const char *states[] = {"inactive", "active", "hit", "noshoot"};
        t["state"] = states[targets[i].state];
    }

    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}

void broadcastHit(uint8_t targetId, int points) {
    StaticJsonDocument<256> doc;
    doc["event"] = "hit";

    JsonObject data = doc.createNestedObject("data");
    data["target"] = targetId;
    data["points"] = points;
    data["totalScore"] = currentScore;
    data["intensity"] = targets[targetId].lastHitIntensity;

    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}

void broadcastTimer() {
    uint32_t elapsed = millis() - gameStartTime;
    uint32_t remaining = (gameTime > elapsed) ? (gameTime - elapsed) : 0;

    StaticJsonDocument<128> doc;
    doc["event"] = "timer";
    JsonObject data = doc.createNestedObject("data");
    data["time"] = elapsed;
    data["remaining"] = remaining;

    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}

void checkTargetTimeout() {
    for (int i = 1; i <= MAX_TARGETS; i++) {
        if (targets[i].online &&
            millis() - targets[i].lastSeen > TARGET_TIMEOUT_MS) {

            targets[i].online = false;
            onlineTargetCount--;
            Serial.printf("Target %d offline (timeout)\n", i);

            // Update UI
            StaticJsonDocument<128> doc;
            doc["event"] = "targetUpdate";
            JsonObject data = doc.createNestedObject("data");
            data["id"] = i;
            data["state"] = "offline";
            data["online"] = false;

            String json;
            serializeJson(doc, json);
            ws.textAll(json);
        }
    }
}

// ============================================================================
// COUNTER FUNCTIES
// ============================================================================

void broadcastCounterState() {
    StaticJsonDocument<512> doc;
    doc["event"] = "counterUpdate";
    JsonObject data = doc.createNestedObject("data");
    data["count"] = counterState.count;
    data["ledsLit"] = counterState.ledsLit;
    data["countsPerLed"] = counterState.countsPerLed;
    data["countsSinceLed"] = counterState.countsSinceLed;
    data["round"] = counterState.currentRound;
    data["totalRounds"] = counterState.totalRounds;
    data["counting"] = counterState.active;
    data["nodeOnline"] = counterState.nodeOnline;
    data["redTimer"] = counterState.redTimerSeconds;
    data["distance"] = counterState.detectionDistance;
    data["mode"] = counterState.mode;

    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}

void processCounterHit(uint8_t nodeId, uint16_t count) {
    counterState.count = count;
    counterState.countsSinceLed++;

    // Check of er een nieuwe LED aan moet
    if (counterState.countsSinceLed >= counterState.countsPerLed) {
        counterState.countsSinceLed = 0;
        if (counterState.ledsLit < 12) {
            counterState.ledsLit++;
        }

        // Alle LEDs vol?
        if (counterState.ledsLit >= 12) {
            counterState.currentRound++;

            // Broadcast counter full event
            StaticJsonDocument<256> doc;
            doc["event"] = "counterFull";
            JsonObject data = doc.createNestedObject("data");
            data["count"] = counterState.count;
            data["round"] = counterState.currentRound;
            data["totalRounds"] = counterState.totalRounds;
            String json;
            serializeJson(doc, json);
            ws.textAll(json);

            Serial.printf("Counter VOL! Ronde %d/%d\n",
                          counterState.currentRound, counterState.totalRounds);

            // Auto-reset als er meer rondes zijn
            if (counterState.totalRounds == 0 ||
                counterState.currentRound < counterState.totalRounds) {
                // Reset wordt door webinterface afgehandeld na animatie
            }
        }
    }

    // Stuur groene feedback als hand op tijd is gedetecteerd (bij rode timer)
    if (counterState.redTimerSeconds > 0) {
        MasterMessage msg;
        msg.cmd = CMD_SET_COLOR;
        msg.targetId = nodeId;
        msg.r = 0; msg.g = 255; msg.b = 0; // GROEN = op tijd!
        sendToAllTargets(msg);
    }

    // Broadcast detection event naar webclients
    StaticJsonDocument<256> doc;
    doc["event"] = "counterDetection";
    JsonObject data = doc.createNestedObject("data");
    data["count"] = counterState.count;
    data["ledsLit"] = counterState.ledsLit;
    data["countsSinceLed"] = counterState.countsSinceLed;
    data["round"] = counterState.currentRound;

    String json;
    serializeJson(doc, json);
    ws.textAll(json);

    Serial.printf("Counter detectie: %d (LEDs: %d/12, Ronde: %d)\n",
                  counterState.count, counterState.ledsLit, counterState.currentRound);
}

// ============================================================================
// HIGHSCORES
// ============================================================================

void loadHighscores() {
    for (int i = 0; i < MAX_HIGHSCORES; i++) {
        String key = "hs" + String(i);
        String name = preferences.getString((key + "n").c_str(), "---");
        uint32_t score = preferences.getUInt((key + "s").c_str(), 0);

        strncpy(highscores[i].name, name.c_str(), 20);
        highscores[i].name[20] = '\0';
        highscores[i].score = score;
    }

    Serial.println("Highscores geladen:");
    for (int i = 0; i < 3; i++) {
        Serial.printf("  #%d: %s - %d\n", i + 1,
                      highscores[i].name, highscores[i].score);
    }
}

void saveHighscores() {
    for (int i = 0; i < MAX_HIGHSCORES; i++) {
        String key = "hs" + String(i);
        preferences.putString((key + "n").c_str(), highscores[i].name);
        preferences.putUInt((key + "s").c_str(), highscores[i].score);
    }
    Serial.println("Highscores opgeslagen");
}

void checkHighscore(uint32_t score) {
    // Vind positie
    int pos = -1;
    for (int i = 0; i < MAX_HIGHSCORES; i++) {
        if (score > highscores[i].score) {
            pos = i;
            break;
        }
    }

    if (pos == -1) return; // Niet hoog genoeg

    // Schuif lagere scores op
    for (int i = MAX_HIGHSCORES - 1; i > pos; i--) {
        highscores[i] = highscores[i - 1];
    }

    // Voeg nieuwe score toe
    strncpy(highscores[pos].name, currentPlayer, 20);
    highscores[pos].name[20] = '\0';
    highscores[pos].score = score;
    highscores[pos].date = millis(); // Zou datum moeten zijn

    saveHighscores();

    Serial.printf("NIEUWE HIGHSCORE! Positie #%d: %s - %d\n",
                  pos + 1, currentPlayer, score);
}
