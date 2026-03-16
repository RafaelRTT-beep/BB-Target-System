/*
 * ============================================================
 * BB SHOOTING TARGET SYSTEM - MASTER CONTROLLER V3 SECURE
 * ============================================================
 * Platform: ESP32-S3
 * Core:     Arduino ESP32 Core 3.x (IDF 5.1+)
 * Security: Chip-ID lock — runs only on authorized ESP32 boards
 * 
 * Game Modes:
 *   0 = Free Play      - alle targets actief, vrij schieten
 *   1 = Shoot/No-Shoot - rood=shoot(+10), groen=no-shoot(-11)
 *   2 = Sequence        - targets in volgorde raken
 *   3 = Random          - willekeurig target verschijnt
 *   4 = Manual          - handmatig targets activeren
 *   5 = Memory          - volgorde onthouden en naschieten
 *   6 = Random Multi   - multiplayer, eigen kleuren
 * 
 * Hardware:
 *   Master: ESP32-S3 (AP mode, webserver)
 *   Targets: ESP32 met piezo + WS2812B
 *   Communicatie: ESP-NOW op channel 1
 * ============================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <Update.h>

// ============================================================
// HARDWARE CONFIG
// ============================================================

#define MAX_TARGETS 20
#define TARGET_COUNT 0
#define AP_SSID      "RAF RTT TRAINING SYSTEM"
#define AP_PASS      "12345678"
#define HIT_LED_PIN  2
#define START_BTN_PIN 4

// ============================================================
// MESSAGE PROTOCOL - Must match target EXACTLY
// ============================================================

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

// ============================================================
// TARGET TRACKING
// ============================================================

struct TargetInfo {
  uint8_t  mac[6];
  uint8_t  id;
  bool     active;
  volatile bool     online;
  uint8_t  colorR, colorG, colorB;
  bool     isShoot;
  bool     shootHit;
  uint32_t hits;
  volatile uint32_t lastPong;
};

TargetInfo targets[MAX_TARGETS];
int numTargets = TARGET_COUNT;

// ============================================================
// HIT QUEUE (ISR-safe)
// ============================================================

#define HIT_QUEUE_SIZE 32

struct HitEvent {
  uint8_t  targetId;
  uint16_t intensity;
};

volatile int hitHead = 0;
volatile int hitTail = 0;
HitEvent hitQueue[HIT_QUEUE_SIZE];

int onlineIdx[MAX_TARGETS];
int onlineCount = 0;

#define DISC_QUEUE_SIZE 20
struct DiscEvent {
  uint8_t mac[6];
  uint8_t targetId;
  bool    isNew;
};
volatile int discHead = 0;
volatile int discTail = 0;
DiscEvent discQueue[DISC_QUEUE_SIZE];

// ============================================================
// GAME STATE
// ============================================================

enum GameMode  { MODE_FREEPLAY=0, MODE_SHOOTNOSHOOT=1, MODE_SEQUENCE=2, MODE_RANDOM=3, MODE_MANUAL=4, MODE_MEMORY=5, MODE_RANDOMCOLOR=6, MODE_REACTION=7, MODE_PARCOURS=8, MODE_FASTTRACK=9, MODE_TOURNAMENT=10, MODE_CAPTURE=11 };
enum GameState { STATE_IDLE=0, STATE_COUNTDOWN=1, STATE_RUNNING=2, STATE_PAUSED=3, STATE_ENDED=4, STATE_TURN_WAIT=5 };

// ============================================================
// CHIP-ID SECURITY
// ============================================================

#define SECURITY_ENABLED  true
#define MAX_AUTHORIZED    10

static const uint64_t AUTHORIZED_CHIPS[MAX_AUTHORIZED] = {
  0x0000000000000000,
  0x0000000000000000,
  0x0000000000000000,
};

bool chipIsAuthorized() {
  if (!SECURITY_ENABLED) return true;
  uint64_t chipId = ESP.getEfuseMac();
  bool anyRegistered = false;
  for (int i = 0; i < MAX_AUTHORIZED; i++) {
    if (AUTHORIZED_CHIPS[i] != 0) {
      anyRegistered = true;
      if (AUTHORIZED_CHIPS[i] == chipId) return true;
    }
  }
  if (!anyRegistered) return true;
  return false;
}

void securityHalt(uint64_t chipId) {
  Serial.println("\n!!! UNAUTHORIZED CHIP — FIRMWARE LOCKED !!!");
  Serial.printf("This chip ID: 0x%012llX\n", chipId);
  Serial.println("Add this ID to AUTHORIZED_CHIPS[] and re-flash.");
  Serial.println("Contact: Running the Target\n");
  pinMode(HIT_LED_PIN, OUTPUT);
  while (true) {
    digitalWrite(HIT_LED_PIN, HIGH);
    delay(100);
    digitalWrite(HIT_LED_PIN, LOW);
    delay(100);
  }
}

// ============================================================

GameMode  currentMode  = MODE_FREEPLAY;
GameState gameState    = STATE_IDLE;

int32_t  currentScore  = 0;
uint32_t totalHits     = 0;
uint32_t totalMisses   = 0;

unsigned long gameStartTime    = 0;
unsigned long countdownStart   = 0;
uint8_t       countdownTotal   = 0;
unsigned long pauseStart       = 0;
unsigned long totalPausedMs    = 0;

uint8_t  seqCurrentIdx   = 0;
uint8_t  seqRound        = 0;
unsigned long seqStepStart = 0;
uint8_t  seqCurHits      = 0;
bool     seqWaiting      = false;
unsigned long seqWaitStart = 0;

uint8_t  randCurrentIdx  = 0;
uint8_t  randPrevIdx     = 255;
uint8_t  randRound       = 0;
bool     randWaiting     = false;
uint16_t randCurrentDisplayTime = 3000;
unsigned long randStepStart = 0;

char     playerName[16]  = "Speler";
uint32_t finalTimeMs     = 0;

#define MEM_MAX_LENGTH 20
uint8_t  memSequence[MEM_MAX_LENGTH];
uint8_t  memShowIdx    = 0;
uint8_t  memShootIdx   = 0;
uint8_t  memPhase      = 0;
uint8_t  memFlashCount = 0;
bool     memHitPending = false;
uint8_t  memHitTarget  = 0;
bool     memHitCorrect = false;
uint8_t  memWrongCount = 0;
unsigned long memStepStart = 0;
unsigned long memHitTime   = 0;

#define RC_MAX_PLAYERS 4
struct RCPlayer {
  char     name[16];
  uint8_t  r, g, b;
  int32_t  score;
  uint16_t correctHits;
  uint16_t wrongHits;
  uint32_t totalReactionMs;
  uint8_t  targetsThisCycle;
  uint8_t  targetsHitThisCycle;
  uint16_t totalAssigned;
  bool     allCyclesDone;
  uint16_t cyclesCompleted;
  unsigned long finishTimeMs;
  bool     active;
};
static const uint8_t playerColorR[RC_MAX_PLAYERS] = {0, 255, 128, 255};
static const uint8_t playerColorG[RC_MAX_PLAYERS] = {0, 0, 0, 255};
static const uint8_t playerColorB[RC_MAX_PLAYERS] = {255, 0, 255, 255};

RCPlayer rcPlayers[RC_MAX_PLAYERS] = {
  {"Speler 1", 0,   0,   255, 0, 0, 0, 0, 0, 0, 0, false, 0, 0, true},
  {"Speler 2", 255, 0,   0  , 0, 0, 0, 0, 0, 0, 0, false, 0, 0, true},
  {"Speler 3", 128, 0,   255, 0, 0, 0, 0, 0, 0, 0, false, 0, 0, false},
  {"Speler 4", 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, false, 0, 0, false}
};
uint8_t  rcPlayerCount    = 2;
int8_t   rcTargetPlayer[20];
int8_t   rcPrevTargetPlayer[20];
uint8_t  rcActiveTargets  = 0;
uint16_t rcCycle          = 0;
bool     rcWaiting        = false;
unsigned long rcStepStart = 0;
unsigned long rcCycleLitTime = 0;

#define RX_MAX_CYCLES 60
int8_t   rxPlayerTarget[RC_MAX_PLAYERS];
uint16_t rxTimes[RC_MAX_PLAYERS][RX_MAX_CYCLES];
bool     rxHit[RC_MAX_PLAYERS];
uint16_t rxCycle        = 0;
uint16_t rxRounds       = 5;
bool     rxFixed        = false;
uint16_t rxDelayMin     = 1000;
uint16_t rxDelayMax     = 3000;
unsigned long rxLitTime = 0;
bool     rxWaiting      = true;
bool     rxAdvancing    = false;
unsigned long rxWaitStart = 0;
uint32_t rxCurrentDelay = 0;

#define PAR_MAX_TARGETS 20
uint8_t  parOrder[PAR_MAX_TARGETS];
uint8_t  parTotal       = 0;
uint8_t  parCurrent     = 0;
uint32_t parSplits[PAR_MAX_TARGETS];
bool     parRandom      = false;
uint8_t  parCustomIds[PAR_MAX_TARGETS];
uint8_t  parCustomCount = 0;
uint8_t  parCurHits     = 0;
bool     parWaiting     = false;
unsigned long parWaitStart = 0;
unsigned long parStepStart = 0;

int8_t   ftPlayerTarget[RC_MAX_PLAYERS];
uint8_t  ftPlayerHits[RC_MAX_PLAYERS];
int8_t   ftWinner       = -1;
uint8_t  ftFixedTarget[RC_MAX_PLAYERS];

#define TB_MAX_GROUP 20
#define TB_MAX_TEAMS 4

#define TB_GAME_RANDOM    0
#define TB_GAME_SEQUENCE  1
#define TB_GAME_SNS       2
#define TB_GAME_REACTION  3
#define TB_GAME_PARCOURS  4
#define TB_GAME_MEMORY    5
#define TB_GAME_RANDCOLOR 6
#define TB_GAME_FASTTRACK 7

struct TeamState {
  char     name[16];
  uint8_t  gameMode;
  uint8_t  rounds;
  uint8_t  tgts[TB_MAX_GROUP];
  uint8_t  count;
  int8_t   curTarget;
  uint8_t  hits;
  uint8_t  misses;
  int32_t  score;
  uint32_t finishMs;
  bool     done;
  bool     snsIsGreen;
  uint32_t rxLightTime;
  bool     rxWaiting;
  uint8_t  memSeq[20];
  uint8_t  memLen;
  uint8_t  memPos;
  uint8_t  memPhase;
  uint8_t  memShowIdx;
};
TeamState tbTeam[TB_MAX_TEAMS];
int8_t   tbTargetTeam[20];
int8_t   tbWinner = -1;
bool     tbBattleMode = true;
uint8_t  tbNumTeams = 2;
uint8_t  tbGameMode = TB_GAME_RANDOM;

static const uint8_t tbColor[TB_MAX_TEAMS][3] = {{0,0,255},{255,0,0},{128,0,255},{255,255,255}};
static const char* tbColorCSS[TB_MAX_TEAMS] = {"#00f","#f00","#80f","#fff"};
static const char* tbColorName[TB_MAX_TEAMS] = {"Blauw","Rood","Paars","Wit"};

struct CaptureTarget {
  int8_t   owner;
  int8_t   capturedBy;
  uint8_t  hitCount;
  bool     captured;
  bool     locked;
  unsigned long capturedAt;
};
CaptureTarget cpTargets[20];
uint8_t  cpHitsToCapture = 5;
uint16_t cpGameTime      = 2;
bool     cpRecapture     = true;
uint16_t cpLockTime      = 0;
uint8_t  cpWinCount[TB_MAX_TEAMS]  = {3, 3, 3, 3};
uint8_t  cpCaptures[TB_MAX_TEAMS]  = {0, 0, 0, 0};
int8_t   cpWinner        = -1;
unsigned long cpLastFlash = 0;
bool     cpFlashState    = false;

// ===== SETTINGS =====
struct Settings {
  uint16_t piezoThreshold;
  uint16_t fpGameTime;
  bool     snsDarkMode;
  bool     snsOffMode;
  uint8_t  snsStartDelay;
  uint16_t snsGameTime;
  uint16_t seqTimePerTarget;
  uint8_t  seqRounds;
  uint16_t seqDelayNext;
  uint8_t  seqHitsToOff;
  uint16_t randMinTime;
  uint16_t randMaxTime;
  uint8_t  randRounds;
  uint8_t  memLength;
  uint16_t memDisplayTime;
  uint8_t  memMaxWrong;
  uint16_t rcDisplayTime;
  uint16_t rcPauseTime;
  uint16_t rcRounds;
  uint16_t rxRounds;
  uint16_t rxDelayMin;
  uint16_t rxDelayMax;
  uint16_t rxPauseTime;
  bool     rxFixed;
  bool     parRandom;
  uint16_t parDelayNext;
  uint16_t parAutoOff;
  uint8_t  parHitsToOff;
  uint8_t  ftTargetsPerPlayer;
  bool     ftFixed;
  uint8_t  tbRounds;
  uint8_t  cpHitsToCapture;
  uint16_t cpGameTime;
  bool     cpRecapture;
  uint16_t cpLockTime;
  uint8_t  noShootPct;
  uint8_t  noShootCount;
} settings = {
  .piezoThreshold    = 100,
  .fpGameTime        = 0,
  .snsDarkMode       = false,
  .snsOffMode        = true,
  .snsStartDelay     = 5,
  .snsGameTime       = 60,
  .seqTimePerTarget  = 3000,
  .seqRounds         = 10,
  .seqDelayNext      = 0,
  .seqHitsToOff      = 1,
  .randMinTime       = 1000,
  .randMaxTime       = 5000,
  .randRounds        = 15,
  .memLength         = 5,
  .memDisplayTime    = 1000,
  .memMaxWrong       = 3,
  .rcDisplayTime     = 3000,
  .rcPauseTime       = 1500,
  .rcRounds          = 20,
  .rxRounds          = 5,
  .rxDelayMin        = 1000,
  .rxDelayMax        = 3000,
  .rxPauseTime       = 1500,
  .rxFixed           = false,
  .parRandom         = false,
  .parDelayNext      = 0,
  .parAutoOff        = 0,
  .parHitsToOff      = 1,
  .ftTargetsPerPlayer = 10,
  .ftFixed           = false,
  .tbRounds          = 10,
  .cpHitsToCapture   = 5,
  .cpGameTime        = 2,
  .cpRecapture       = true,
  .noShootPct        = 0,
  .noShootCount      = 0
};

Settings baseSettings;
void snapshotBaseSettings() { baseSettings = settings; }

// ===== MULTI-ZONE SYSTEM =====
#define MAX_ZONES 4

struct ZoneState {
  bool     active;
  char     name[16];
  GameMode  mode;
  GameState state;
  int32_t   score;
  uint32_t  hits;
  uint32_t  misses;
  unsigned long startTime;
  unsigned long cdStart;
  uint8_t       cdTotal;
  unsigned long pauseStart;
  unsigned long pausedMs;
  char     pName[16];
  uint32_t finalMs;
  uint8_t  tgtIdx[MAX_TARGETS];
  uint8_t  tgtCount;
  uint8_t  seqIdx;
  uint8_t  seqRnd;
  unsigned long seqStep;
  uint8_t  seqCHits;
  bool     seqW;
  unsigned long seqWS;
  uint8_t  rndIdx;
  uint8_t  rndPrev;
  uint8_t  rndRnd;
  bool     rndWait;
  uint16_t rndDisplay;
  unsigned long rndStep;
  uint8_t  mSeq[MEM_MAX_LENGTH];
  uint8_t  mShowIdx;
  uint8_t  mShootIdx;
  uint8_t  mPhase;
  uint8_t  mFlashCnt;
  bool     mHitPend;
  uint8_t  mHitTgt;
  bool     mHitCorr;
  uint8_t  mWrong;
  unsigned long mStepStart;
  unsigned long mHitTime;
  uint16_t rxMs[RX_MAX_CYCLES];
  uint16_t rxCyc;
  uint16_t rxRnds;
  bool     rxFix;
  uint16_t rxDMin;
  uint16_t rxDMax;
  unsigned long rxLit;
  bool     rxWait;
  bool     rxAdv;
  unsigned long rxWaitSt;
  uint32_t rxCurDelay;
  int8_t   rxTgtIdx;
  uint8_t  pOrd[PAR_MAX_TARGETS];
  uint8_t  pTotal;
  uint8_t  pCur;
  uint32_t pSplits[PAR_MAX_TARGETS];
  bool     pRandom;
  uint8_t  pCustIds[PAR_MAX_TARGETS];
  uint8_t  pCustCnt;
  uint8_t  pCHits;
  bool     pW;
  unsigned long pWS;
  unsigned long pSS;
  Settings cfg;
  RCPlayer rcP[RC_MAX_PLAYERS];
  uint8_t  rcPCnt;
  int8_t   rcTgtP[MAX_TARGETS];
  int8_t   rcPrevTgtP[MAX_TARGETS];
  uint8_t  rcActTgts;
  uint16_t rcCyc;
  bool     rcW;
  unsigned long rcSS;
  unsigned long rcCLT;
  int8_t   rxPT[RC_MAX_PLAYERS];
  uint16_t rxT[RC_MAX_PLAYERS][RX_MAX_CYCLES];
  bool     rxH[RC_MAX_PLAYERS];
  int8_t   ftPT[RC_MAX_PLAYERS];
  uint8_t  ftPH[RC_MAX_PLAYERS];
  int8_t   ftW;
  uint8_t  ftFT[RC_MAX_PLAYERS];
  TeamState tbT[TB_MAX_TEAMS];
  int8_t   tbTT[MAX_TARGETS];
  int8_t   tbW;
  bool     tbBM;
  uint8_t  tbNT;
  uint8_t  tbGM;
};

ZoneState zones[MAX_ZONES];
uint8_t   numZones = 1;
uint8_t   activeZone = 0;
int8_t    targetZone[MAX_TARGETS];

// ============================================================
// LEADERBOARD
// ============================================================

#define MAX_SCORES 20

struct ScoreEntry {
  char    name[16];
  char    mode[16];
  int32_t score;
  uint16_t hits;
  uint32_t timeMs;
};

ScoreEntry topScores[MAX_SCORES];
Preferences preferences;

// ============================================================
// TURN-BASED PLAY
// ============================================================

#define TURN_MAX_PLAYERS 12
struct TurnPlayer {
  char     name[20];
  int32_t  score;
  uint16_t hits;
  uint16_t misses;
  uint32_t timeMs;
  bool     done;
};

TurnPlayer turnPlayers[TURN_MAX_PLAYERS];
uint8_t  turnPlayerCount  = 0;
uint8_t  turnCurrentPlayer = 0;
bool     turnMode         = false;
bool     turnAutoNext     = false;
uint8_t  turnCountdownSec = 5;
bool     turnGlobal       = false;
GameMode turnSavedMode    = MODE_FREEPLAY;
String   turnSavedQuery;
unsigned long turnWaitStart = 0;

void resetTurnMode() {
  turnMode = false;
  turnPlayerCount = 0;
  turnCurrentPlayer = 0;
  for (int i = 0; i < TURN_MAX_PLAYERS; i++) {
    turnPlayers[i].done = false;
    turnPlayers[i].score = 0;
    turnPlayers[i].hits = 0;
    turnPlayers[i].misses = 0;
    turnPlayers[i].timeMs = 0;
    turnPlayers[i].name[0] = '\0';
  }
}

// ============================================================
// PENDING ACTIONS
// ============================================================

enum ActionType : uint8_t {
  ACT_NONE = 0,
  ACT_FLASH,
  ACT_LIGHT_OFF,
  ACT_END_GAME,
  ACT_ADVANCE_SEQ,
  ACT_ADVANCE_RAND,
  ACT_FINISH_RC,
  ACT_FINISH_RX,
  ACT_PAR_NEXT,
  ACT_FT_LIGHT,
  ACT_TB_LIGHT,
  ACT_TB_RX_LIGHT,
  ACT_TB_MEM_SHOW,
  ACT_TB_MEM_PLAY,
  ACT_LED_OFF,
  ACT_NS_OFF
};

#define MAX_PENDING 80
struct PendingAction {
  uint32_t  triggerTime;
  ActionType type;
  int8_t    targetIdx;
  int8_t    zone;
  uint8_t   r, g, b, dur;
};

PendingAction pendingActions[MAX_PENDING];
uint32_t masterLedOffTime = 0;

// ============================================================
// GLOBALS
// ============================================================

WebServer server(80);
unsigned long lastPingTime = 0;

bool     btnLastState   = HIGH;
unsigned long btnLastChange = 0;
#define  BTN_DEBOUNCE   50

// ============================================================
// UTILITY FUNCTIONS
// ============================================================

uint8_t nsPlaced = 0;
uint16_t nsTargetsLit = 0;

bool decideNoShoot() {
  if (settings.noShootCount > 0) {
    nsTargetsLit++;
    if (nsPlaced >= settings.noShootCount) return false;
    uint16_t totalRounds = settings.seqRounds;
    if (currentMode == MODE_RANDOM) totalRounds = settings.randRounds;
    else if (currentMode == MODE_PARCOURS) totalRounds = parTotal;
    else if (currentMode == MODE_FASTTRACK) totalRounds = settings.ftTargetsPerPlayer * 4;
    else if (currentMode == MODE_REACTION) totalRounds = settings.rxRounds;
    if (totalRounds == 0) totalRounds = 20;
    uint16_t remaining = totalRounds - nsTargetsLit + 1;
    uint8_t nsRemaining = settings.noShootCount - nsPlaced;
    if (remaining <= nsRemaining || (uint8_t)random(remaining) < nsRemaining) {
      nsPlaced++;
      return true;
    }
    return false;
  }
  if (settings.noShootPct == 0) return false;
  return ((uint8_t)random(100) < settings.noShootPct);
}

void queueDiscovery(const uint8_t *mac, uint8_t id, bool isNew) {
  int next = (discHead + 1) % DISC_QUEUE_SIZE;
  if (next != discTail) {
    memcpy(discQueue[discHead].mac, mac, 6);
    discQueue[discHead].targetId = id;
    discQueue[discHead].isNew = isNew;
    discHead = next;
  }
}

void queueHit(uint8_t id, uint16_t intensity) {
  int next = (hitHead + 1) % HIT_QUEUE_SIZE;
  if (next != hitTail) {
    hitQueue[hitHead].targetId  = id;
    hitQueue[hitHead].intensity = intensity;
    hitHead = next;
  } else {
    Serial.printf("HIT QUEUE FULL T%d dropped!\n", id);
  }
}

bool dequeueHit(HitEvent &evt) {
  if (hitHead == hitTail) return false;
  evt = hitQueue[hitTail];
  hitTail = (hitTail + 1) % HIT_QUEUE_SIZE;
  return true;
}

void refreshOnline() {
  onlineCount = 0;
  if (numZones > 1 && activeZone < MAX_ZONES && zones[activeZone].tgtCount > 0
      && currentMode != MODE_TOURNAMENT) {
    for (int i = 0; i < zones[activeZone].tgtCount; i++) {
      uint8_t ti = zones[activeZone].tgtIdx[i];
      if (ti < numTargets && targets[ti].online) {
        onlineIdx[onlineCount++] = ti;
      }
    }
  } else {
    for (int i = 0; i < numTargets; i++) {
      if (targets[i].online) {
        onlineIdx[onlineCount++] = i;
      }
    }
  }
  Serial.printf("Online targets (zone %c): %d\n", 'A'+activeZone, onlineCount);
}

void processDiscovery() {
  DiscEvent evt;
  while (discHead != discTail) {
    evt = discQueue[discTail];
    discTail = (discTail + 1) % DISC_QUEUE_SIZE;
    if (evt.isNew) {
      if (numTargets >= MAX_TARGETS) continue;
      int idx = numTargets;
      memcpy(targets[idx].mac, evt.mac, 6);
      targets[idx].id = evt.targetId;
      targets[idx].active = false;
      targets[idx].online = true;
      targets[idx].isShoot = true;
      targets[idx].shootHit = false;
      targets[idx].lastPong = millis();
      targets[idx].hits = 0;
      targets[idx].colorR = targets[idx].colorG = targets[idx].colorB = 0;
      esp_now_peer_info_t peer = {};
      memcpy(peer.peer_addr, evt.mac, 6);
      peer.channel = 1;
      peer.encrypt = false;
      peer.ifidx = WIFI_IF_AP;
      esp_now_add_peer(&peer);
      numTargets++;
      if (targetZone[idx] < 0) {
        targetZone[idx] = 0;
        if (zones[0].tgtCount < MAX_TARGETS) {
          zones[0].tgtIdx[zones[0].tgtCount++] = idx;
        }
      }
      Serial.printf("NEW TARGET: T%d MAC=%02X:%02X:%02X:%02X:%02X:%02X (total=%d)\n",
        evt.targetId, evt.mac[0], evt.mac[1], evt.mac[2],
        evt.mac[3], evt.mac[4], evt.mac[5], numTargets);
    } else {
      int idx = findTargetByID(evt.targetId);
      if (idx < 0) continue;
      esp_now_del_peer(targets[idx].mac);
      memcpy(targets[idx].mac, evt.mac, 6);
      esp_now_peer_info_t peer = {};
      memcpy(peer.peer_addr, evt.mac, 6);
      peer.channel = 1;
      peer.encrypt = false;
      peer.ifidx = WIFI_IF_AP;
      esp_now_add_peer(&peer);
      Serial.printf("MAC UPDATE T%d -> %02X:%02X:%02X:%02X:%02X:%02X\n",
        evt.targetId, evt.mac[0], evt.mac[1], evt.mac[2],
        evt.mac[3], evt.mac[4], evt.mac[5]);
    }
  }
}

void clearPending() {
  for (int i = 0; i < MAX_PENDING; i++) {
    if (pendingActions[i].zone == activeZone || pendingActions[i].zone < 0) {
      pendingActions[i].type = ACT_NONE;
      pendingActions[i].zone = -1;
    }
  }
}
void clearAllPending() {
  for (int i = 0; i < MAX_PENDING; i++) { pendingActions[i].type = ACT_NONE; pendingActions[i].zone = -1; }
}

void schedulePending(uint32_t ms, ActionType type, int8_t idx = -1,
                     uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t dur = 0) {
  for (int i = 0; i < MAX_PENDING; i++) {
    if (pendingActions[i].type == ACT_NONE) {
      pendingActions[i].triggerTime = millis() + ms;
      pendingActions[i].type = type;
      pendingActions[i].targetIdx = idx;
      pendingActions[i].zone = activeZone;
      pendingActions[i].r = r;
      pendingActions[i].g = g;
      pendingActions[i].b = b;
      pendingActions[i].dur = dur;
      return;
    }
  }
}

void flashMasterLed(uint32_t durationMs = 50) {
  digitalWrite(HIT_LED_PIN, HIGH);
  masterLedOffTime = millis() + durationMs;
}

// ============================================================
// ESP-NOW COMMUNICATION
// ============================================================

void sendToTarget(int idx, uint8_t msgType, uint8_t r, uint8_t g, uint8_t b, uint8_t extra, uint16_t intensity) {
  if (idx < 0 || idx >= numTargets) return;
  TargetMessage msg = {};
  msg.msgType   = msgType;
  msg.targetId  = targets[idx].id;
  msg.timestamp = millis();
  msg.intensity = intensity;
  msg.colorR    = r;
  msg.colorG    = g;
  msg.colorB    = b;
  msg.extra     = extra;
  esp_err_t result = esp_now_send(targets[idx].mac, (uint8_t*)&msg, sizeof(msg));
  if (result != ESP_OK) {
    Serial.printf("SEND FAIL T%d type=%d err=%d\n", targets[idx].id, msgType, result);
  }
}

void sendLightOn(int idx, uint8_t r, uint8_t g, uint8_t b) {
  if (idx < 0 || idx >= numTargets) return;
  sendToTarget(idx, MSG_LIGHT_ON, r, g, b, 0, 0);
  targets[idx].active = true;
  targets[idx].colorR = r;
  targets[idx].colorG = g;
  targets[idx].colorB = b;
}

void sendLightOff(int idx) {
  if (idx < 0 || idx >= numTargets) return;
  sendToTarget(idx, MSG_LIGHT_OFF, 0, 0, 0, 0, 0);
  targets[idx].active = false;
  targets[idx].colorR = targets[idx].colorG = targets[idx].colorB = 0;
}

void sendFlash(int idx, uint8_t r, uint8_t g, uint8_t b, uint8_t dur100ms) {
  sendToTarget(idx, MSG_FLASH, r, g, b, dur100ms, 0);
}

void sendAllOff() {
  for (int i = 0; i < numTargets; i++) {
    if (!isMyTarget(i)) continue;
    sendLightOff(i);
  }
}

void sendBuzz(int idx, uint8_t type) {
  if (idx >= 0 && idx < numTargets && targets[idx].online) {
    sendToTarget(idx, MSG_BUZZ, type, 0, 0, 0, 0);
  }
}

void sendBuzzTeam(int team, uint8_t type) {
  for (int i = 0; i < tbTeam[team].count; i++) {
    int t = tbTeam[team].tgts[i];
    if (targets[t].online) sendToTarget(t, MSG_BUZZ, type, 0, 0, 0, 0);
  }
}

void sendBuzzAll(uint8_t type) {
  for (int i = 0; i < numTargets; i++) {
    if (!isMyTarget(i)) continue;
    if (targets[i].online) {
      sendToTarget(i, MSG_BUZZ, type, 0, 0, 0, 0);
    }
  }
}

void broadcastThreshold(uint16_t threshold) {
  for (int i = 0; i < numTargets; i++) {
    sendToTarget(i, MSG_SET_THRESHOLD, 0, 0, 0, 0, threshold);
  }
}

void pingTargets() {
  for (int i = 0; i < numTargets; i++) {
    sendToTarget(i, MSG_PING, 0, 0, 0, 0, 0);
  }
}

int findTargetByID(uint8_t id) {
  for (int i = 0; i < numTargets; i++) {
    if (targets[i].id == id) return i;
  }
  return -1;
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(TargetMessage)) return;
  TargetMessage msg;
  memcpy(&msg, data, sizeof(msg));
  int idx = findTargetByID(msg.targetId);
  if (idx < 0 && msg.targetId > 0 && numTargets < MAX_TARGETS) {
    queueDiscovery(info->src_addr, msg.targetId, true);
  }
  else if (idx >= 0 && info->src_addr && memcmp(targets[idx].mac, info->src_addr, 6) != 0) {
    queueDiscovery(info->src_addr, msg.targetId, false);
  }
  if (idx >= 0) {
    targets[idx].online = true;
    targets[idx].lastPong = millis();
  }
  switch (msg.msgType) {
    case MSG_HIT:
      queueHit(msg.targetId, msg.intensity);
      break;
    case MSG_PONG:
    case MSG_ANNOUNCE:
      break;
  }
}

// ============================================================
// GAME LOGIC
// ============================================================

unsigned long getElapsed() {
  if (finalTimeMs > 0) return finalTimeMs;
  if (gameState == STATE_RUNNING) {
    if (currentMode == MODE_MEMORY && memPhase < 5) return 0;
    return millis() - gameStartTime - totalPausedMs;
  } else if (gameState == STATE_PAUSED) {
    return pauseStart - gameStartTime - totalPausedMs;
  }
  return 0;
}

uint32_t getGameTimeMs() {
  switch (currentMode) {
    case MODE_FREEPLAY:     return (uint32_t)settings.fpGameTime * 1000;
    case MODE_SHOOTNOSHOOT: return (uint32_t)settings.snsGameTime * 1000;
    case MODE_CAPTURE:      return (uint32_t)settings.cpGameTime * 60UL * 1000UL;
    default: return 0;
  }
}

// ===== ZONE CONTEXT SWITCHING =====
void initZones() {
  for (int z = 0; z < MAX_ZONES; z++) {
    zones[z].active = (z == 0);
    snprintf(zones[z].name, 16, "Zone %c", 'A' + z);
    zones[z].mode = MODE_FREEPLAY;
    zones[z].state = STATE_IDLE;
    zones[z].score = 0;
    zones[z].hits = 0;
    zones[z].misses = 0;
    zones[z].startTime = 0;
    zones[z].cdStart = 0;
    zones[z].cdTotal = 0;
    zones[z].pauseStart = 0;
    zones[z].pausedMs = 0;
    strcpy(zones[z].pName, "Speler");
    zones[z].finalMs = 0;
    zones[z].tgtCount = 0;
    zones[z].seqIdx = 0; zones[z].seqRnd = 0;
    zones[z].rndIdx = 0; zones[z].rndPrev = 255; zones[z].rndRnd = 0;
    zones[z].rndWait = false; zones[z].rndDisplay = 3000;
    zones[z].mShowIdx = 0; zones[z].mShootIdx = 0;
    zones[z].mPhase = 0; zones[z].mFlashCnt = 0;
    zones[z].mWrong = 0;
    zones[z].rxCyc = 0; zones[z].rxRnds = 10;
    zones[z].rxFix = false; zones[z].rxDMin = 1000; zones[z].rxDMax = 3000;
    zones[z].rxWait = true; zones[z].rxAdv = false;
    zones[z].pTotal = 0; zones[z].pCur = 0;
    zones[z].pRandom = false; zones[z].pCustCnt = 0;
    zones[z].cfg = settings;
    zones[z].rcPCnt = 2;
    for (int p = 0; p < RC_MAX_PLAYERS; p++) {
      memset(&zones[z].rcP[p], 0, sizeof(RCPlayer));
      snprintf(zones[z].rcP[p].name, 16, "Speler %d", p + 1);
      zones[z].rcP[p].r = playerColorR[p];
      zones[z].rcP[p].g = playerColorG[p];
      zones[z].rcP[p].b = playerColorB[p];
      zones[z].rcP[p].active = (p < 2);
    }
  }
  for (int i = 0; i < MAX_TARGETS; i++) targetZone[i] = -1;
}

inline bool isMyTarget(int idx) {
  if (numZones <= 1) return true;
  if (currentMode == MODE_TOURNAMENT) return true;
  return (idx >= 0 && idx < numTargets && targetZone[idx] == (int8_t)activeZone);
}

int8_t findZoneForTarget(uint8_t targetIdx) {
  for (int z = 0; z < numZones; z++) {
    if (!zones[z].active) continue;
    for (int i = 0; i < zones[z].tgtCount; i++) {
      if (zones[z].tgtIdx[i] == targetIdx) return z;
    }
  }
  return -1;
}

void loadZone(uint8_t z) {
  if (z >= MAX_ZONES) return;
  if (numZones <= 1) { activeZone = 0; return; }
  ZoneState &zs = zones[z];
  activeZone = z;
  currentMode = zs.mode;
  gameState = zs.state;
  currentScore = zs.score;
  totalHits = zs.hits;
  totalMisses = zs.misses;
  gameStartTime = zs.startTime;
  countdownStart = zs.cdStart;
  countdownTotal = zs.cdTotal;
  pauseStart = zs.pauseStart;
  totalPausedMs = zs.pausedMs;
  strncpy(playerName, zs.pName, 15); playerName[15] = '\0';
  finalTimeMs = zs.finalMs;
  seqCurrentIdx = zs.seqIdx;
  seqRound = zs.seqRnd;
  seqStepStart = zs.seqStep;
  seqCurHits = zs.seqCHits;
  seqWaiting = zs.seqW;
  seqWaitStart = zs.seqWS;
  randCurrentIdx = zs.rndIdx;
  randPrevIdx = zs.rndPrev;
  randRound = zs.rndRnd;
  randWaiting = zs.rndWait;
  randCurrentDisplayTime = zs.rndDisplay;
  randStepStart = zs.rndStep;
  memcpy(memSequence, zs.mSeq, MEM_MAX_LENGTH);
  memShowIdx = zs.mShowIdx;
  memShootIdx = zs.mShootIdx;
  memPhase = zs.mPhase;
  memFlashCount = zs.mFlashCnt;
  memHitPending = zs.mHitPend;
  memHitTarget = zs.mHitTgt;
  memHitCorrect = zs.mHitCorr;
  memWrongCount = zs.mWrong;
  memStepStart = zs.mStepStart;
  memHitTime = zs.mHitTime;
  rxCycle = zs.rxCyc;
  rxRounds = zs.rxRnds;
  rxFixed = zs.rxFix;
  rxDelayMin = zs.rxDMin;
  rxDelayMax = zs.rxDMax;
  rxLitTime = zs.rxLit;
  rxWaiting = zs.rxWait;
  rxAdvancing = zs.rxAdv;
  rxWaitStart = zs.rxWaitSt;
  rxCurrentDelay = zs.rxCurDelay;
  memcpy(parOrder, zs.pOrd, PAR_MAX_TARGETS);
  parTotal = zs.pTotal;
  parCurrent = zs.pCur;
  memcpy(parSplits, zs.pSplits, sizeof(parSplits));
  parRandom = zs.pRandom;
  memcpy(parCustomIds, zs.pCustIds, PAR_MAX_TARGETS);
  parCustomCount = zs.pCustCnt;
  parCurHits = zs.pCHits;
  parWaiting = zs.pW;
  parWaitStart = zs.pWS;
  parStepStart = zs.pSS;
  settings = zs.cfg;
  onlineCount = 0;
  if (currentMode == MODE_TOURNAMENT) {
    for (int i = 0; i < numTargets; i++) {
      if (targets[i].online) onlineIdx[onlineCount++] = i;
    }
  } else {
    for (int i = 0; i < (int)zs.tgtCount; i++) {
      uint8_t ti = zs.tgtIdx[i];
      if (ti < numTargets && targets[ti].online) {
        onlineIdx[onlineCount++] = ti;
      }
    }
  }
  memcpy(rcPlayers, zs.rcP, sizeof(rcPlayers));
  rcPlayerCount = zs.rcPCnt;
  memcpy(rcTargetPlayer, zs.rcTgtP, sizeof(rcTargetPlayer));
  memcpy(rcPrevTargetPlayer, zs.rcPrevTgtP, sizeof(rcPrevTargetPlayer));
  rcActiveTargets = zs.rcActTgts;
  rcCycle = zs.rcCyc;
  rcWaiting = zs.rcW;
  rcStepStart = zs.rcSS;
  rcCycleLitTime = zs.rcCLT;
  memcpy(rxPlayerTarget, zs.rxPT, sizeof(rxPlayerTarget));
  memcpy(rxTimes, zs.rxT, sizeof(rxTimes));
  memcpy(rxHit, zs.rxH, sizeof(rxHit));
  memcpy(ftPlayerTarget, zs.ftPT, sizeof(ftPlayerTarget));
  memcpy(ftPlayerHits, zs.ftPH, sizeof(ftPlayerHits));
  ftWinner = zs.ftW;
  memcpy(ftFixedTarget, zs.ftFT, sizeof(ftFixedTarget));
  memcpy(tbTeam, zs.tbT, sizeof(tbTeam));
  memcpy(tbTargetTeam, zs.tbTT, sizeof(tbTargetTeam));
  tbWinner = zs.tbW;
  tbBattleMode = zs.tbBM;
  tbNumTeams = zs.tbNT;
  tbGameMode = zs.tbGM;
}

void saveZone(uint8_t z) {
  if (z >= MAX_ZONES) return;
  if (numZones <= 1) { zones[0].state = gameState; zones[0].mode = currentMode; return; }
  ZoneState &zs = zones[z];
  zs.mode = currentMode;
  zs.state = gameState;
  zs.score = currentScore;
  zs.hits = totalHits;
  zs.misses = totalMisses;
  zs.startTime = gameStartTime;
  zs.cdStart = countdownStart;
  zs.cdTotal = countdownTotal;
  zs.pauseStart = pauseStart;
  zs.pausedMs = totalPausedMs;
  strncpy(zs.pName, playerName, 15); zs.pName[15] = '\0';
  zs.finalMs = finalTimeMs;
  zs.seqIdx = seqCurrentIdx;
  zs.seqRnd = seqRound;
  zs.seqStep = seqStepStart;
  zs.seqCHits = seqCurHits;
  zs.seqW = seqWaiting;
  zs.seqWS = seqWaitStart;
  zs.rndIdx = randCurrentIdx;
  zs.rndPrev = randPrevIdx;
  zs.rndRnd = randRound;
  zs.rndWait = randWaiting;
  zs.rndDisplay = randCurrentDisplayTime;
  zs.rndStep = randStepStart;
  memcpy(zs.mSeq, memSequence, MEM_MAX_LENGTH);
  zs.mShowIdx = memShowIdx;
  zs.mShootIdx = memShootIdx;
  zs.mPhase = memPhase;
  zs.mFlashCnt = memFlashCount;
  zs.mHitPend = memHitPending;
  zs.mHitTgt = memHitTarget;
  zs.mHitCorr = memHitCorrect;
  zs.mWrong = memWrongCount;
  zs.mStepStart = memStepStart;
  zs.mHitTime = memHitTime;
  zs.rxCyc = rxCycle;
  zs.rxRnds = rxRounds;
  zs.rxFix = rxFixed;
  zs.rxDMin = rxDelayMin;
  zs.rxDMax = rxDelayMax;
  zs.rxLit = rxLitTime;
  zs.rxWait = rxWaiting;
  zs.rxAdv = rxAdvancing;
  zs.rxWaitSt = rxWaitStart;
  zs.rxCurDelay = rxCurrentDelay;
  memcpy(zs.pOrd, parOrder, PAR_MAX_TARGETS);
  zs.pTotal = parTotal;
  zs.pCur = parCurrent;
  memcpy(zs.pSplits, parSplits, sizeof(parSplits));
  zs.pRandom = parRandom;
  memcpy(zs.pCustIds, parCustomIds, PAR_MAX_TARGETS);
  zs.pCustCnt = parCustomCount;
  zs.pCHits = parCurHits;
  zs.pW = parWaiting;
  zs.pWS = parWaitStart;
  zs.pSS = parStepStart;
  zs.cfg = settings;
  memcpy(zs.rcP, rcPlayers, sizeof(rcPlayers));
  zs.rcPCnt = rcPlayerCount;
  memcpy(zs.rcTgtP, rcTargetPlayer, sizeof(rcTargetPlayer));
  memcpy(zs.rcPrevTgtP, rcPrevTargetPlayer, sizeof(rcPrevTargetPlayer));
  zs.rcActTgts = rcActiveTargets;
  zs.rcCyc = rcCycle;
  zs.rcW = rcWaiting;
  zs.rcSS = rcStepStart;
  zs.rcCLT = rcCycleLitTime;
  memcpy(zs.rxPT, rxPlayerTarget, sizeof(rxPlayerTarget));
  memcpy(zs.rxT, rxTimes, sizeof(rxTimes));
  memcpy(zs.rxH, rxHit, sizeof(rxHit));
  memcpy(zs.ftPT, ftPlayerTarget, sizeof(ftPlayerTarget));
  memcpy(zs.ftPH, ftPlayerHits, sizeof(ftPlayerHits));
  zs.ftW = ftWinner;
  memcpy(zs.ftFT, ftFixedTarget, sizeof(ftFixedTarget));
  memcpy(zs.tbT, tbTeam, sizeof(tbTeam));
  memcpy(zs.tbTT, tbTargetTeam, sizeof(tbTargetTeam));
  zs.tbW = tbWinner;
  zs.tbBM = tbBattleMode;
  zs.tbNT = tbNumTeams;
  zs.tbGM = tbGameMode;
}

void startGame() {
  currentScore = 0;
  totalHits = 0;
  totalMisses = 0;
  totalPausedMs = 0;
  finalTimeMs = 0;
  seqRound = 0;
  randRound = 0;
  randPrevIdx = 255;
  randWaiting = false;
  memPhase = 0;
  memShowIdx = 0;
  memShootIdx = 0;
  memFlashCount = 0;
  memHitPending = false;
  memWrongCount = 0;
  rcCycle = 0;
  rcWaiting = false;
  rcActiveTargets = 0;
  parCurrent = 0;
  parTotal = 0;
  ftWinner = -1;
  for (int i = 0; i < RC_MAX_PLAYERS; i++) { ftPlayerTarget[i] = -1; ftPlayerHits[i] = 0; }
  nsPlaced = 0; nsTargetsLit = 0;
  tbWinner = -1;
  for (int t = 0; t < TB_MAX_TEAMS; t++) {
    tbTeam[t].count = 0; tbTeam[t].hits = 0; tbTeam[t].misses = 0;
    tbTeam[t].score = 0; tbTeam[t].finishMs = 0; tbTeam[t].done = false;
    tbTeam[t].curTarget = -1; tbTeam[t].snsIsGreen = true;
    tbTeam[t].rxWaiting = false; tbTeam[t].rxLightTime = 0;
    tbTeam[t].memLen = 2; tbTeam[t].memPos = 0; tbTeam[t].memPhase = 0; tbTeam[t].memShowIdx = 0;
    if (tbTeam[t].name[0] == 0) { char buf[16]; snprintf(buf, 16, "Team %c", 'A'+t); strcpy(tbTeam[t].name, buf); }
  }
  for (int i = 0; i < 20; i++) { rcTargetPlayer[i] = -1; rcPrevTargetPlayer[i] = -1; }

  if (numZones > 1) {
    ZoneState &zs = zones[activeZone];
    for (int i = 0; i < zs.tgtCount; i++) {
      uint8_t ti = zs.tgtIdx[i];
      if (ti < numTargets) {
        targets[ti].hits = 0;
        targets[ti].active = false;
        targets[ti].shootHit = false;
      }
    }
  } else {
    for (int i = 0; i < numTargets; i++) {
      targets[i].hits = 0;
      targets[i].active = false;
      targets[i].shootHit = false;
    }
  }

  uint8_t delay_s = 0;
  if (server.hasArg("cd")) delay_s = server.arg("cd").toInt();

  if (delay_s > 0) {
    gameState = STATE_COUNTDOWN;
    countdownStart = millis();
    countdownTotal = delay_s;
    sendAllOff();
    Serial.printf("COUNTDOWN %ds\n", delay_s);
  } else {
    beginPlaying();
  }
}

void beginPlaying() {
  gameState = STATE_RUNNING;
  gameStartTime = millis();
  totalPausedMs = 0;
  refreshOnline();

  if (onlineCount == 0) {
    Serial.println("GEEN TARGETS ONLINE - game afgebroken");
    gameState = STATE_IDLE;
    return;
  }

  switch (currentMode) {
    case MODE_FREEPLAY:
      for (int i = 0; i < numTargets; i++) {
        if (!isMyTarget(i)) continue;
        if (targets[i].online) sendLightOn(i, 255, 0, 0);
      }
      break;
    case MODE_SHOOTNOSHOOT:
      if (settings.snsDarkMode && !settings.snsOffMode) {
        sendAllOff();
      } else {
        for (int i = 0; i < numTargets; i++) {
          if (!isMyTarget(i)) continue;
          if (!targets[i].online) continue;
          if (targets[i].isShoot) sendLightOn(i, 255, 0, 0);
          else sendLightOn(i, 0, 255, 0);
        }
      }
      break;
    case MODE_SEQUENCE: {
      sendAllOff();
      seqCurrentIdx = onlineIdx[0];
      seqRound = 0;
      seqCurHits = 0;
      seqWaiting = false;
      seqStepStart = millis();
      bool ns = decideNoShoot();
      targets[seqCurrentIdx].isShoot = !ns;
      if (ns) {
        sendLightOn(seqCurrentIdx, 0, 255, 0);
        schedulePending(1000, ACT_NS_OFF, seqCurrentIdx);
      } else {
        sendLightOn(seqCurrentIdx, 255, 0, 0);
      }
      break;
    }
    case MODE_RANDOM:
      sendAllOff();
      randRound = 0;
      pickRandomTarget();
      break;
    case MODE_MANUAL:
      sendAllOff();
      break;
    case MODE_MEMORY:
      generateMemorySequence();
      startMemoryShowing();
      break;
    case MODE_RANDOMCOLOR:
      resetRCPlayers();
      rcCycle = 0;
      for (int i = 0; i < 20; i++) { rcTargetPlayer[i] = -1; rcPrevTargetPlayer[i] = -1; }
      startRCCycle();
      break;
    case MODE_REACTION:
      resetRCPlayers();
      rxCycle = 0;
      rxDelayMin = settings.rxDelayMin;
      rxDelayMax = settings.rxDelayMax;
      rxRounds = settings.rxRounds;
      rxFixed = settings.rxFixed;
      for (int p = 0; p < RC_MAX_PLAYERS; p++) {
        rxPlayerTarget[p] = -1;
        rxHit[p] = false;
        for (int c = 0; c < RX_MAX_CYCLES; c++) rxTimes[p][c] = 0;
      }
      startReactionCycle();
      break;
    case MODE_PARCOURS:
      startParcours();
      break;
    case MODE_FASTTRACK:
      startFastTrack();
      break;
    case MODE_TOURNAMENT:
      startTournament();
      break;
    case MODE_CAPTURE:
      startCapturePoints();
      break;
  }
  Serial.printf("GAME START mode=%d\n", currentMode);
}

void stopGame() {
  if (gameState == STATE_RUNNING || gameState == STATE_PAUSED) {
    if (finalTimeMs == 0) finalTimeMs = millis() - gameStartTime - totalPausedMs;
    gameState = STATE_ENDED;
    sendAllOff();
    if (currentMode == MODE_RANDOMCOLOR || currentMode == MODE_REACTION || currentMode == MODE_FASTTRACK) {
      for (int i = 0; i < rcPlayerCount; i++) {
        if (rcPlayers[i].active) {
          tryInsertScore(rcPlayers[i].name, rcPlayers[i].score, rcPlayers[i].correctHits, finalTimeMs, getModeName(currentMode));
        }
      }
    } else if (currentMode == MODE_TOURNAMENT) {
      for (int t = 0; t < (int)tbNumTeams; t++) {
        if (tbTeam[t].count > 0) {
          uint32_t ft = tbTeam[t].finishMs > 0 ? tbTeam[t].finishMs : finalTimeMs;
          tryInsertScore(tbTeam[t].name, tbTeam[t].score, tbTeam[t].hits, ft, getModeName(currentMode));
        }
      }
    } else {
      tryInsertScore(playerName, currentScore, totalHits, finalTimeMs, getModeName(currentMode));
    }
    if (turnMode) {
      turnMode = false;
      Serial.println("TURN: Geannuleerd door stop");
    }
    Serial.printf("GAME STOP: score=%d time=%dms\n", currentScore, finalTimeMs);
  } else if (gameState == STATE_TURN_WAIT) {
    gameState = STATE_ENDED;
    turnMode = false;
    sendAllOff();
  } else {
    gameState = STATE_IDLE;
    sendAllOff();
  }
}

void endGame() {
  if (gameState == STATE_ENDED || gameState == STATE_IDLE || gameState == STATE_TURN_WAIT) return;
  if (finalTimeMs == 0) finalTimeMs = millis() - gameStartTime - totalPausedMs;
  gameState = STATE_ENDED;
  if (currentMode != MODE_TOURNAMENT && currentMode != MODE_CAPTURE) sendBuzzAll(2);

  if (currentMode == MODE_CAPTURE && cpWinner < 0) {
    int maxCap = -1;
    for (int t = 0; t < (int)tbNumTeams; t++) {
      if ((int)cpCaptures[t] > maxCap) { maxCap = cpCaptures[t]; cpWinner = t; }
    }
    if (cpWinner >= 0) {
      sendAllOff();
      for (int i = 0; i < numTargets; i++) {
        if (!isMyTarget(i) || !targets[i].online) continue;
        sendFlash(i, tbColor[cpWinner][0], tbColor[cpWinner][1], tbColor[cpWinner][2], 5);
      }
    }
  }

  if (currentMode != MODE_TOURNAMENT && currentMode != MODE_CAPTURE) {
    for (int i = 0; i < numTargets; i++) {
      if (!isMyTarget(i)) continue;
      sendFlash(i, 255, 0, 0, 2);
      schedulePending(250, ACT_FLASH, i, 255, 0, 0, 2);
      schedulePending(500, ACT_FLASH, i, 255, 0, 0, 2);
    }
    for (int i = 0; i < numTargets; i++) {
      if (!isMyTarget(i)) continue;
      schedulePending(700, ACT_LIGHT_OFF, i);
    }
  }

  if (currentMode == MODE_RANDOMCOLOR || currentMode == MODE_REACTION) {
    for (int i = 0; i < rcPlayerCount; i++) {
      if (rcPlayers[i].active) {
        int avgMs = rcPlayers[i].correctHits > 0 ? rcPlayers[i].totalReactionMs / rcPlayers[i].correctHits : 0;
        tryInsertScore(rcPlayers[i].name, rcPlayers[i].correctHits, rcPlayers[i].correctHits, avgMs, getModeName(currentMode));
      }
    }
  } else if (currentMode == MODE_FASTTRACK) {
    for (int i = 0; i < rcPlayerCount; i++) {
      if (rcPlayers[i].active) {
        uint32_t ft = rcPlayers[i].finishTimeMs > 0 ? rcPlayers[i].finishTimeMs : finalTimeMs;
        tryInsertScore(rcPlayers[i].name, rcPlayers[i].score, ftPlayerHits[i], ft, getModeName(currentMode));
      }
    }
  } else if (currentMode == MODE_TOURNAMENT) {
    for (int t = 0; t < tbNumTeams; t++) {
      if (tbTeam[t].count > 0) {
        uint32_t ft = tbTeam[t].finishMs > 0 ? tbTeam[t].finishMs : finalTimeMs;
        tryInsertScore(tbTeam[t].name, tbTeam[t].score, tbTeam[t].hits, ft, getModeName(currentMode));
      }
    }
  } else if (currentMode == MODE_CAPTURE) {
    for (int t = 0; t < (int)tbNumTeams; t++) {
      if (tbTeam[t].count > 0) {
        tryInsertScore(tbTeam[t].name, cpCaptures[t], cpCaptures[t], finalTimeMs, getModeName(currentMode));
      }
    }
  } else {
    tryInsertScore(playerName, currentScore, totalHits, finalTimeMs, getModeName(currentMode));
  }

  if (turnMode && turnCurrentPlayer < turnPlayerCount) {
    TurnPlayer &tp = turnPlayers[turnCurrentPlayer];
    tp.score = currentScore;
    tp.hits = totalHits;
    tp.misses = totalMisses;
    tp.timeMs = finalTimeMs;
    tp.done = true;
    if (turnCurrentPlayer + 1 < turnPlayerCount) {
      gameState = STATE_TURN_WAIT;
      turnWaitStart = millis();
      turnCurrentPlayer++;
    } else {
      gameState = STATE_ENDED;
    }
  }
}

void pauseGame() {
  if (gameState == STATE_RUNNING) {
    gameState = STATE_PAUSED;
    pauseStart = millis();
  } else if (gameState == STATE_PAUSED) {
    totalPausedMs += millis() - pauseStart;
    gameState = STATE_RUNNING;
  }
}

void resetGame() {
  gameState = STATE_IDLE;
  currentScore = 0;
  totalHits = 0;
  totalMisses = 0;
  finalTimeMs = 0;
  clearPending();
  if (currentMode == MODE_TOURNAMENT || numZones <= 1) {
    for (int i = 0; i < numTargets; i++) targets[i].hits = 0;
  } else {
    ZoneState &zs = zones[activeZone];
    for (int i = 0; i < zs.tgtCount; i++) {
      uint8_t ti = zs.tgtIdx[i];
      if (ti < numTargets) targets[ti].hits = 0;
    }
  }
  sendAllOff();
  if (turnMode) { turnMode = false; turnPlayerCount = 0; turnCurrentPlayer = 0; }
}

// ---- Sequence mode ----
void advanceSequence() {
  sendLightOff(seqCurrentIdx);
  if (settings.seqDelayNext > 0) {
    seqWaiting = true;
    seqWaitStart = millis();
  } else {
    seqLightNext();
  }
}

void seqLightNext() {
  int pos = 0;
  for (int i = 0; i < onlineCount; i++) {
    if (onlineIdx[i] == seqCurrentIdx) { pos = i; break; }
  }
  pos = (pos + 1) % onlineCount;
  seqCurrentIdx = onlineIdx[pos];
  seqRound++;
  seqCurHits = 0;
  seqWaiting = false;
  if (seqRound >= settings.seqRounds) { endGame(); return; }
  seqStepStart = millis();
  bool nsSeq = decideNoShoot();
  targets[seqCurrentIdx].isShoot = !nsSeq;
  if (nsSeq) {
    sendLightOn(seqCurrentIdx, 0, 255, 0);
    schedulePending(1000, ACT_NS_OFF, seqCurrentIdx);
  } else {
    sendLightOn(seqCurrentIdx, 255, 0, 0);
  }
}

void handleSequenceLogic() {
  if (seqWaiting) {
    if (millis() - seqWaitStart >= settings.seqDelayNext) seqLightNext();
    return;
  }
  if (settings.seqTimePerTarget > 0 && millis() - seqStepStart >= settings.seqTimePerTarget) {
    sendFlash(seqCurrentIdx, 255, 0, 0, 2);
    currentScore -= 3; totalMisses++;
    advanceSequence();
  }
}

// ---- Random mode ----
void pickRandomTarget() {
  sendAllOff();
  refreshOnline();
  if (onlineCount == 0) return;
  int next;
  int tries = 0;
  do {
    next = onlineIdx[random(0, onlineCount)];
    tries++;
  } while (next == randPrevIdx && onlineCount > 1 && tries < 20);
  randPrevIdx = next;
  randCurrentIdx = next;
  randWaiting = false;
  randStepStart = millis();
  bool nsRand = decideNoShoot();
  targets[randCurrentIdx].isShoot = !nsRand;
  if (nsRand) {
    sendLightOn(randCurrentIdx, 0, 255, 0);
    schedulePending(1000, ACT_NS_OFF, randCurrentIdx);
  } else {
    sendLightOn(randCurrentIdx, 255, 0, 0);
  }
}

void advanceRandom() {
  randRound++;
  if (randRound >= settings.randRounds) { endGame(); return; }
  sendLightOff(randCurrentIdx);
  randWaiting = true;
  randCurrentDisplayTime = random(settings.randMinTime, settings.randMaxTime + 1);
  randStepStart = millis();
}

void handleRandomLogic() {
  if (randWaiting) {
    if (millis() - randStepStart >= randCurrentDisplayTime) { randWaiting = false; pickRandomTarget(); }
  } else {
    if (millis() - randStepStart >= settings.randMaxTime) {
      sendFlash(randCurrentIdx, 255, 0, 0, 2);
      currentScore -= 3; totalMisses++;
      advanceRandom();
    }
  }
}

// ---- Memory mode ----
void generateMemorySequence() {
  if (onlineCount == 0) return;
  for (int i = 0; i < settings.memLength; i++) {
    memSequence[i] = onlineIdx[random(0, onlineCount)];
    if (i > 0) {
      while (memSequence[i] == memSequence[i - 1] && onlineCount > 1) {
        memSequence[i] = onlineIdx[random(0, onlineCount)];
      }
    }
  }
}

void startMemoryShowing() {
  memPhase = 0;
  memShowIdx = 0;
  memShootIdx = 0;
  memHitPending = false;
  memWrongCount = 0;
  sendAllOff();
  memStepStart = millis();
}

void handleMemoryLogic() {
  if (memPhase == 0) {
    if (millis() - memStepStart >= 500) {
      sendLightOn(memSequence[0], 0, 255, 0);
      memPhase = 1;
      memStepStart = millis();
    }
  }
  else if (memPhase == 1) {
    if (millis() - memStepStart >= settings.memDisplayTime) {
      sendLightOff(memSequence[memShowIdx]);
      memShowIdx++;
      if (memShowIdx >= settings.memLength) {
        memPhase = 3;
        memFlashCount = 0;
        memStepStart = millis();
        return;
      }
      memPhase = 2;
      memStepStart = millis();
    }
  }
  else if (memPhase == 2) {
    if (millis() - memStepStart >= 250) {
      sendLightOn(memSequence[memShowIdx], 0, 255, 0);
      memPhase = 1;
      memStepStart = millis();
    }
  }
  else if (memPhase == 3) {
    if (millis() - memStepStart >= 350) {
      memStepStart = millis();
      if (memFlashCount % 2 == 0) {
        for (int i = 0; i < numTargets; i++) {
          if (!isMyTarget(i)) continue;
          sendLightOn(i, 0, 255, 0);
        }
      } else {
        sendAllOff();
      }
      memFlashCount++;
      if (memFlashCount >= 6) {
        memPhase = 4;
        memStepStart = millis();
      }
    }
  }
  else if (memPhase == 4) {
    if (millis() - memStepStart >= 300) {
      for (int i = 0; i < numTargets; i++) {
        if (!isMyTarget(i)) continue;
        sendLightOn(i, 0, 0, 255);
      }
      memPhase = 5;
      memShootIdx = 0;
      memHitPending = false;
      gameStartTime = millis();
      totalPausedMs = 0;
    }
  }
}

// ---- Random Multi mode ----
void resetRCPlayers() {
  for (int i = 0; i < RC_MAX_PLAYERS; i++) {
    rcPlayers[i].r = playerColorR[i];
    rcPlayers[i].g = playerColorG[i];
    rcPlayers[i].b = playerColorB[i];
    rcPlayers[i].score = 0;
    rcPlayers[i].correctHits = 0;
    rcPlayers[i].wrongHits = 0;
    rcPlayers[i].totalReactionMs = 0;
    rcPlayers[i].targetsThisCycle = 0;
    rcPlayers[i].targetsHitThisCycle = 0;
    rcPlayers[i].totalAssigned = 0;
    rcPlayers[i].allCyclesDone = false;
    rcPlayers[i].cyclesCompleted = 0;
    rcPlayers[i].finishTimeMs = 0;
    rcPlayers[i].active = (i < rcPlayerCount);
  }
}

void startRCCycle() {
  refreshOnline();
  uint8_t rcNumLit = random(1, onlineCount + 1);
  if (rcNumLit < 1) rcNumLit = 1;
  uint8_t indices[MAX_TARGETS];
  for (int i = 0; i < onlineCount; i++) indices[i] = onlineIdx[i];
  for (int i = onlineCount - 1; i > 0; i--) {
    int j = random(0, i + 1);
    uint8_t tmp = indices[i]; indices[i] = indices[j]; indices[j] = tmp;
  }
  for (int i = 0; i < numTargets; i++) {
    if (!isMyTarget(i)) continue;
    rcTargetPlayer[i] = -1;
  }
  for (int p = 0; p < rcPlayerCount; p++) {
    rcPlayers[p].targetsThisCycle = 0;
    rcPlayers[p].targetsHitThisCycle = 0;
  }
  rcActiveTargets = 0;
  for (int n = 0; n < rcNumLit; n++) {
    int t = indices[n];
    uint16_t minAssigned = 65535;
    for (int p = 0; p < rcPlayerCount; p++) {
      if (rcPlayers[p].totalAssigned + rcPlayers[p].targetsThisCycle < minAssigned) {
        minAssigned = rcPlayers[p].totalAssigned + rcPlayers[p].targetsThisCycle;
      }
    }
    uint8_t candidates[RC_MAX_PLAYERS];
    uint8_t numCandidates = 0;
    for (int p = 0; p < rcPlayerCount; p++) {
      if (rcPlayers[p].totalAssigned + rcPlayers[p].targetsThisCycle == minAssigned) {
        if (p != rcPrevTargetPlayer[t] || rcPlayerCount == 1) {
          candidates[numCandidates++] = p;
        }
      }
    }
    if (numCandidates == 0) {
      for (int p = 0; p < rcPlayerCount; p++) {
        if (rcPlayers[p].totalAssigned + rcPlayers[p].targetsThisCycle == minAssigned) {
          candidates[numCandidates++] = p;
        }
      }
    }
    int player = candidates[random(0, numCandidates)];
    rcTargetPlayer[t] = player;
    rcPrevTargetPlayer[t] = player;
    rcPlayers[player].targetsThisCycle++;
    sendLightOn(t, rcPlayers[player].r, rcPlayers[player].g, rcPlayers[player].b);
    rcActiveTargets++;
  }
  for (int p = 0; p < rcPlayerCount; p++) {
    rcPlayers[p].totalAssigned += rcPlayers[p].targetsThisCycle;
  }
  rcCycleLitTime = millis();
  rcWaiting = false;
}

void finishRCCycle() {
  for (int i = 0; i < numTargets; i++) {
    if (!isMyTarget(i)) continue;
    if (rcTargetPlayer[i] >= 0) {
      sendLightOff(i);
      rcTargetPlayer[i] = -1;
    }
  }
  rcActiveTargets = 0;
  rcCycle++;
  for (int p = 0; p < rcPlayerCount; p++) {
    if (!rcPlayers[p].allCyclesDone) {
      rcPlayers[p].cyclesCompleted = rcCycle;
      if (rcCycle >= settings.rcRounds) {
        rcPlayers[p].allCyclesDone = true;
        rcPlayers[p].finishTimeMs = getElapsed();
      }
    }
  }
  bool allDone = true;
  for (int p = 0; p < rcPlayerCount; p++) {
    if (!rcPlayers[p].allCyclesDone) { allDone = false; break; }
  }
  if (allDone) { endGame(); return; }
  rcWaiting = true;
  rcStepStart = millis();
}

void handleRCLogic() {
  if (rcWaiting) {
    if (millis() - rcStepStart >= settings.rcPauseTime) startRCCycle();
  } else {
    if (millis() - rcCycleLitTime >= settings.rcDisplayTime) finishRCCycle();
  }
}

// ============================================================
// REACTION MODE
// ============================================================

void startReactionCycle() {
  refreshOnline();
  rxAdvancing = false;
  uint8_t playerCount = rcPlayerCount;
  if (playerCount > onlineCount) playerCount = onlineCount;
  for (int p = 0; p < RC_MAX_PLAYERS; p++) {
    rxHit[p] = false;
    rxPlayerTarget[p] = -1;
  }
  if (rxFixed) {
    for (int p = 0; p < playerCount; p++) {
      if (p < onlineCount) rxPlayerTarget[p] = onlineIdx[p];
    }
  } else {
    uint8_t indices[MAX_TARGETS];
    for (int i = 0; i < onlineCount; i++) indices[i] = onlineIdx[i];
    for (int i = onlineCount - 1; i > 0; i--) {
      int j = random(0, i + 1);
      uint8_t tmp = indices[i]; indices[i] = indices[j]; indices[j] = tmp;
    }
    for (int p = 0; p < playerCount; p++) rxPlayerTarget[p] = indices[p];
  }
  sendAllOff();
  rxCurrentDelay = random(rxDelayMin, rxDelayMax + 1);
  rxWaiting = true;
  rxWaitStart = millis();
}

void lightReactionTargets() {
  for (int p = 0; p < rcPlayerCount; p++) {
    if (rxPlayerTarget[p] >= 0) {
      bool nsRx = decideNoShoot();
      targets[rxPlayerTarget[p]].isShoot = !nsRx;
      if (nsRx) {
        sendLightOn(rxPlayerTarget[p], 0, 255, 0);
        schedulePending(1000, ACT_NS_OFF, rxPlayerTarget[p]);
      } else {
        sendLightOn(rxPlayerTarget[p], rcPlayers[p].r, rcPlayers[p].g, rcPlayers[p].b);
      }
    }
  }
  rxLitTime = millis();
  rxWaiting = false;
}

void finishReactionCycle() {
  for (int p = 0; p < rcPlayerCount; p++) {
    if (!rxHit[p] && rxCycle < RX_MAX_CYCLES) {
      rxTimes[p][rxCycle] = 9999;
      currentScore -= 3; totalMisses++;
    }
    if (rxPlayerTarget[p] >= 0) sendLightOff(rxPlayerTarget[p]);
  }
  rxCycle++;
  if (rxCycle >= settings.rxRounds) { endGame(); return; }
  startReactionCycle();
}

void handleReactionLogic() {
  if (rxWaiting) {
    if (millis() - rxWaitStart >= rxCurrentDelay) lightReactionTargets();
  } else if (!rxAdvancing) {
    bool allHit = true;
    for (int p = 0; p < rcPlayerCount; p++) {
      if (rxPlayerTarget[p] >= 0 && !rxHit[p]) { allHit = false; break; }
    }
    if (allHit) {
      rxAdvancing = true;
      schedulePending(500, ACT_FINISH_RX);
    }
    if (millis() - rxLitTime >= 5000) finishReactionCycle();
  }
}

// ============================================================
// PARCOURS MODE
// ============================================================

void startParcours() {
  refreshOnline();
  if (settings.parRandom) {
    parTotal = onlineCount;
    if (parTotal == 0) return;
    for (int i = 0; i < parTotal; i++) parOrder[i] = onlineIdx[i];
    for (int i = parTotal - 1; i > 0; i--) {
      int j = random(0, i + 1);
      uint8_t tmp = parOrder[i]; parOrder[i] = parOrder[j]; parOrder[j] = tmp;
    }
  } else if (parCustomCount > 0) {
    parTotal = 0;
    for (int i = 0; i < parCustomCount && parTotal < PAR_MAX_TARGETS; i++) {
      int idx = findTargetByID(parCustomIds[i]);
      if (idx >= 0 && targets[idx].online) parOrder[parTotal++] = idx;
    }
    if (parTotal == 0) return;
  } else {
    parTotal = onlineCount;
    if (parTotal == 0) return;
    for (int i = 0; i < parTotal; i++) parOrder[i] = onlineIdx[i];
  }
  parCurrent = 0;
  parCurHits = 0;
  parWaiting = false;
  for (int i = 0; i < PAR_MAX_TARGETS; i++) parSplits[i] = 0;
  bool nsPar = decideNoShoot();
  targets[parOrder[0]].isShoot = !nsPar;
  if (nsPar) {
    sendLightOn(parOrder[0], 0, 255, 0);
    parWaiting = true;
    parWaitStart = millis();
  } else {
    sendLightOn(parOrder[0], 255, 0, 0);
  }
  parStepStart = millis();
}

void handleParcoursLogic() {
  if (parWaiting) {
    uint16_t delayMs = settings.parDelayNext > 0 ? settings.parDelayNext : 300;
    if (millis() - parWaitStart >= delayMs) {
      parWaiting = false;
      if (parCurrent > 0 && targets[parOrder[parCurrent - 1]].isShoot) {
        sendLightOff(parOrder[parCurrent - 1]);
      }
      if (parCurrent < parTotal) {
        bool nsPL = decideNoShoot();
        targets[parOrder[parCurrent]].isShoot = !nsPL;
        if (nsPL) {
          sendLightOn(parOrder[parCurrent], 0, 255, 0);
          parCurrent++;
          if (parCurrent >= parTotal) { endGame(); return; }
          parWaiting = true;
          parWaitStart = millis();
        } else {
          sendLightOn(parOrder[parCurrent], 255, 0, 0);
          parStepStart = millis();
        }
      }
    }
    return;
  }
  if (settings.parAutoOff > 0 && parCurrent < parTotal) {
    if (millis() - parStepStart >= settings.parAutoOff) {
      sendFlash(parOrder[parCurrent], 255, 0, 0, 2);
      currentScore -= 5; totalMisses++;
      parSplits[parCurrent] = getElapsed();
      parCurrent++;
      parCurHits = 0;
      if (parCurrent >= parTotal) {
        finalTimeMs = millis() - gameStartTime - totalPausedMs;
        schedulePending(500, ACT_END_GAME);
      } else {
        parWaiting = true;
        parWaitStart = millis();
      }
    }
  }
}

// ============================================================
// FAST TRACK MODE
// ============================================================

int8_t ftPickTarget(int playerIdx) {
  if (settings.ftFixed) return ftFixedTarget[playerIdx];
  if (onlineCount == 0) return -1;
  int tries = 0;
  while (tries < 50) {
    int candidate = onlineIdx[random(0, onlineCount)];
    bool inUse = false;
    for (int p = 0; p < rcPlayerCount; p++) {
      if (p != playerIdx && ftPlayerTarget[p] == candidate) { inUse = true; break; }
    }
    if (!inUse) return candidate;
    tries++;
  }
  return onlineIdx[random(0, onlineCount)];
}

void startFastTrack() {
  refreshOnline();
  ftWinner = -1;
  for (int p = 0; p < rcPlayerCount; p++) {
    ftPlayerHits[p] = 0;
    rcPlayers[p].r = playerColorR[p];
    rcPlayers[p].g = playerColorG[p];
    rcPlayers[p].b = playerColorB[p];
    rcPlayers[p].score = 0;
    rcPlayers[p].correctHits = 0;
    rcPlayers[p].totalReactionMs = 0;
    rcPlayers[p].finishTimeMs = 0;
    rcPlayers[p].allCyclesDone = false;
    if (p < onlineCount) ftFixedTarget[p] = onlineIdx[p % onlineCount];
    else ftFixedTarget[p] = onlineIdx[0];
  }
  for (int p = 0; p < rcPlayerCount; p++) {
    if (!rcPlayers[p].active) { ftPlayerTarget[p] = -1; continue; }
    int t = ftPickTarget(p);
    ftPlayerTarget[p] = t;
    if (t < 0) continue;
    bool nsFT = decideNoShoot();
    targets[t].isShoot = !nsFT;
    if (nsFT) {
      sendLightOn(t, 0, 255, 0);
      schedulePending(1000, ACT_NS_OFF, t);
    } else {
      sendLightOn(t, rcPlayers[p].r, rcPlayers[p].g, rcPlayers[p].b);
    }
  }
}

void ftHandleWin(int winnerIdx) {
  ftWinner = winnerIdx;
  rcPlayers[winnerIdx].finishTimeMs = getElapsed();
  sendAllOff();
  uint8_t wr = rcPlayers[winnerIdx].r;
  uint8_t wg = rcPlayers[winnerIdx].g;
  uint8_t wb = rcPlayers[winnerIdx].b;
  for (int i = 0; i < numTargets; i++) {
    if (!isMyTarget(i)) continue;
    if (targets[i].online) {
      sendFlash(i, wr, wg, wb, 5);
      schedulePending(600, ACT_FLASH, i, wr, wg, wb, 5);
      schedulePending(1200, ACT_FLASH, i, wr, wg, wb, 5);
    }
  }
  finalTimeMs = millis() - gameStartTime - totalPausedMs;
  schedulePending(1800, ACT_END_GAME);
}

// ============================================================
// TOURNAMENT MODE
// ============================================================

const char* tbGameModeName(uint8_t m) {
  switch(m) {
    case TB_GAME_RANDOM: return "Random";
    case TB_GAME_SEQUENCE: return "Sequence";
    case TB_GAME_SNS: return "Shoot/No-Shoot";
    case TB_GAME_REACTION: return "Reaction";
    case TB_GAME_PARCOURS: return "Parcours";
    case TB_GAME_MEMORY: return "Memory";
    case TB_GAME_RANDCOLOR: return "Random Multi";
    case TB_GAME_FASTTRACK: return "Fast Track";
    default: return "?";
  }
}

int8_t tbPickRandom(int team) {
  TeamState &ts = tbTeam[team];
  if (ts.count == 0) return -1;
  if (ts.count == 1) return ts.tgts[0];
  int tries = 0;
  while (tries < 30) {
    int pick = ts.tgts[random(0, ts.count)];
    if (pick != ts.curTarget) return pick;
    tries++;
  }
  return ts.tgts[random(0, ts.count)];
}

int8_t tbPickSequence(int team) {
  TeamState &ts = tbTeam[team];
  if (ts.count == 0) return -1;
  return ts.tgts[ts.hits % ts.count];
}

void tbLightTarget(int team, int8_t idx) {
  if (idx < 0 || team < 0 || team >= TB_MAX_TEAMS) return;
  tbTeam[team].curTarget = idx;
  if (tbTeam[team].gameMode == TB_GAME_SNS) {
    bool green = random(100) < 70;
    tbTeam[team].snsIsGreen = green;
    if (green) sendLightOn(idx, 0, 255, 0);
    else sendLightOn(idx, 255, 0, 0);
  } else {
    sendLightOn(idx, tbColor[team][0], tbColor[team][1], tbColor[team][2]);
  }
}

void tbMemGenSequence(int team) {
  TeamState &ts = tbTeam[team];
  for (int i = 0; i < ts.memLen && i < 20; i++) {
    ts.memSeq[i] = random(0, ts.count);
  }
}

void tbMemStartShow(int team) {
  TeamState &ts = tbTeam[team];
  ts.memPhase = 0;
  ts.memShowIdx = 0;
  tbMemGenSequence(team);
  schedulePending(500, ACT_TB_MEM_SHOW, team);
}

uint8_t tbRoundsForTeam(int team) {
  if (tbTeam[team].gameMode == TB_GAME_PARCOURS) return tbTeam[team].count;
  uint8_t r = tbTeam[team].rounds > 0 ? tbTeam[team].rounds : settings.tbRounds;
  return r;
}

void tbActivateNext(int team) {
  TeamState &ts = tbTeam[team];
  if (ts.done || ts.count == 0 || gameState != STATE_RUNNING) return;
  int8_t next = -1;
  switch (ts.gameMode) {
    case TB_GAME_RANDOM:
    case TB_GAME_SNS:
      next = tbPickRandom(team);
      tbLightTarget(team, next);
      break;
    case TB_GAME_SEQUENCE:
    case TB_GAME_PARCOURS:
      next = tbPickSequence(team);
      tbLightTarget(team, next);
      break;
    case TB_GAME_REACTION:
      ts.rxWaiting = true;
      ts.curTarget = -1;
      {
        uint32_t delay = random(1000, 4000);
        next = tbPickRandom(team);
        ts.memSeq[0] = next;
        schedulePending(delay, ACT_TB_RX_LIGHT, team);
      }
      break;
    case TB_GAME_MEMORY:
      tbMemStartShow(team);
      break;
    case TB_GAME_RANDCOLOR: {
      next = tbPickRandom(team);
      ts.curTarget = next;
      if (next >= 0) {
        if (random(100) < 60) {
          ts.snsIsGreen = true;
          sendLightOn(next, tbColor[team][0], tbColor[team][1], tbColor[team][2]);
        } else {
          ts.snsIsGreen = false;
          int other = (team + 1 + random(TB_MAX_TEAMS - 1)) % TB_MAX_TEAMS;
          sendLightOn(next, tbColor[other][0], tbColor[other][1], tbColor[other][2]);
        }
      }
      break;
    }
    case TB_GAME_FASTTRACK:
      ts.memPos = 0;
      for (int i = 0; i < ts.count; i++) {
        sendLightOn(ts.tgts[i], tbColor[team][0], tbColor[team][1], tbColor[team][2]);
      }
      ts.curTarget = ts.tgts[0];
      break;
  }
}

void startTournament() {
  refreshOnline();
  tbWinner = -1;
  for (int t = 0; t < TB_MAX_TEAMS; t++) {
    tbTeam[t].count = 0;
    tbTeam[t].hits = 0;
    tbTeam[t].misses = 0;
    tbTeam[t].score = 0;
    tbTeam[t].finishMs = 0;
    tbTeam[t].done = false;
    tbTeam[t].curTarget = -1;
    tbTeam[t].snsIsGreen = true;
    tbTeam[t].rxWaiting = false;
    tbTeam[t].memLen = 2;
    tbTeam[t].memPos = 0;
    tbTeam[t].memPhase = 0;
    tbTeam[t].memShowIdx = 0;
  }
  for (int i = 0; i < numTargets; i++) {
    int t = tbTargetTeam[i];
    if (t >= 0 && t < (int)tbNumTeams && targets[i].online) {
      if (tbTeam[t].count < TB_MAX_GROUP) {
        tbTeam[t].tgts[tbTeam[t].count++] = i;
      }
    }
  }
  for (int t = 0; t < (int)tbNumTeams; t++) {
    if (tbTeam[t].count == 0) continue;
    tbActivateNext(t);
  }
}

void tbHandleWin(int team) {
  tbTeam[team].done = true;
  tbTeam[team].finishMs = millis() - gameStartTime - totalPausedMs;
  if (tbTeam[team].curTarget >= 0) sendLightOff(tbTeam[team].curTarget);
  tbTeam[team].curTarget = -1;
  if (tbBattleMode) {
    tbWinner = team;
    sendAllOff();
    for (int i = 0; i < numTargets; i++) {
      if (!isMyTarget(i)) continue;
      if (targets[i].online) {
        sendFlash(i, 255, 255, 0, 5);
        schedulePending(600, ACT_FLASH, i, 255, 255, 0, 5);
        schedulePending(1200, ACT_FLASH, i, 255, 255, 0, 5);
      }
    }
    finalTimeMs = millis() - gameStartTime - totalPausedMs;
    schedulePending(1800, ACT_END_GAME);
  } else {
    for (int i = 0; i < tbTeam[team].count; i++) {
      sendFlash(tbTeam[team].tgts[i], 255, 255, 0, 3);
    }
    bool allDone = true;
    for (int t = 0; t < (int)tbNumTeams; t++) {
      if (tbTeam[t].count > 0 && !tbTeam[t].done) { allDone = false; break; }
    }
    if (allDone) {
      tbWinner = -1;
      for (int t = 0; t < (int)tbNumTeams; t++) {
        if (tbTeam[t].count > 0 && tbTeam[t].finishMs > 0) {
          if (tbWinner < 0 || tbTeam[t].finishMs < tbTeam[tbWinner].finishMs) tbWinner = t;
        }
      }
      if (tbWinner < 0) tbWinner = 0;
      finalTimeMs = millis() - gameStartTime - totalPausedMs;
      schedulePending(500, ACT_END_GAME);
    }
  }
}

void tbProcessHit(int team, int idx) {
  TeamState &ts = tbTeam[team];
  if (ts.done) return;
  uint8_t rounds = tbRoundsForTeam(team);
  switch (ts.gameMode) {
    case TB_GAME_RANDOM:
    case TB_GAME_SEQUENCE:
    case TB_GAME_PARCOURS: {
      ts.hits++;
      ts.score += 10;
      currentScore += 10;
      sendFlash(idx, 255, 255, 0, 1);
      sendLightOff(idx);
      if (ts.hits >= rounds) tbHandleWin(team);
      else schedulePending(200, ACT_TB_LIGHT, team);
      break;
    }
    case TB_GAME_SNS: {
      if (ts.snsIsGreen) {
        ts.hits++; ts.score += 10; currentScore += 10;
        sendFlash(idx, 255, 255, 0, 1);
      } else {
        ts.hits++; ts.misses++;
        ts.score = max((int32_t)0, ts.score - 10);
        sendFlash(idx, 255, 255, 255, 2);
      }
      sendLightOff(idx);
      if (ts.hits >= rounds) tbHandleWin(team);
      else schedulePending(300, ACT_TB_LIGHT, team);
      break;
    }
    case TB_GAME_REACTION: {
      if (ts.rxWaiting) break;
      ts.hits++; ts.score += 10; currentScore += 10;
      sendFlash(idx, 255, 255, 0, 1);
      sendLightOff(idx);
      if (ts.hits >= rounds) tbHandleWin(team);
      else schedulePending(300, ACT_TB_LIGHT, team);
      break;
    }
    case TB_GAME_MEMORY: {
      if (ts.memPhase != 1) break;
      int8_t expectedIdx = ts.tgts[ts.memSeq[ts.memPos]];
      if (idx == expectedIdx) {
        sendFlash(idx, 255, 255, 0, 1);
        ts.memPos++;
        ts.score += 10; currentScore += 10;
        if (ts.memPos >= ts.memLen) {
          ts.hits++;
          sendLightOff(idx);
          if (ts.hits >= rounds) tbHandleWin(team);
          else {
            ts.memLen = min((uint8_t)20, (uint8_t)(ts.memLen + 1));
            schedulePending(800, ACT_TB_LIGHT, team);
          }
        } else {
          sendLightOff(idx);
          int8_t nextExp = ts.tgts[ts.memSeq[ts.memPos]];
          sendLightOn(nextExp, tbColor[team][0], tbColor[team][1], tbColor[team][2]);
          ts.curTarget = nextExp;
        }
      } else {
        ts.misses++;
        ts.score = max((int32_t)0, ts.score - 5);
        sendFlash(idx, 255, 0, 0, 2);
        sendLightOff(ts.curTarget);
        ts.curTarget = -1;
        schedulePending(1000, ACT_TB_LIGHT, team);
      }
      break;
    }
    case TB_GAME_RANDCOLOR: {
      if (ts.snsIsGreen) {
        ts.hits++; ts.score += 10; currentScore += 10;
        sendFlash(idx, 255, 255, 0, 1);
      } else {
        ts.hits++; ts.misses++;
        ts.score = max((int32_t)0, ts.score - 10);
        sendFlash(idx, 255, 255, 255, 2);
      }
      sendLightOff(idx);
      if (ts.hits >= rounds) tbHandleWin(team);
      else schedulePending(300, ACT_TB_LIGHT, team);
      break;
    }
    case TB_GAME_FASTTRACK: {
      if (!targets[idx].active) break;
      ts.score += 10; currentScore += 10;
      sendFlash(idx, 255, 255, 0, 1);
      sendLightOff(idx);
      ts.memPos++;
      if (ts.memPos >= ts.count) {
        ts.hits++;
        if (ts.hits >= rounds) tbHandleWin(team);
        else schedulePending(300, ACT_TB_LIGHT, team);
      }
      break;
    }
  }
}

// ===== CAPTURE POINTS FUNCTIONS =====

void startCapturePoints() {
  refreshOnline();
  cpWinner = -1;
  cpFlashState = false;
  cpLastFlash = millis();
  cpHitsToCapture = settings.cpHitsToCapture;
  cpGameTime = settings.cpGameTime;
  cpRecapture = settings.cpRecapture;
  cpLockTime = settings.cpLockTime;
  for (int t = 0; t < TB_MAX_TEAMS; t++) {
    cpCaptures[t] = 0;
    tbTeam[t].count = 0; tbTeam[t].hits = 0; tbTeam[t].misses = 0;
    tbTeam[t].score = 0; tbTeam[t].done = false;
  }
  for (int i = 0; i < numTargets; i++) {
    cpTargets[i].owner = tbTargetTeam[i];
    cpTargets[i].capturedBy = -1;
    cpTargets[i].hitCount = 0;
    cpTargets[i].captured = false;
    cpTargets[i].locked = false;
    cpTargets[i].capturedAt = 0;
    int t = tbTargetTeam[i];
    if (t >= 0 && t < (int)tbNumTeams && targets[i].online) {
      if (tbTeam[t].count < TB_MAX_GROUP) {
        tbTeam[t].tgts[tbTeam[t].count++] = i;
      }
    }
  }
  for (int i = 0; i < numTargets; i++) {
    if (!isMyTarget(i) || !targets[i].online) continue;
    int t = cpTargets[i].owner;
    if (t >= 0 && t < TB_MAX_TEAMS) {
      sendLightOn(i, tbColor[t][0], tbColor[t][1], tbColor[t][2]);
    }
  }
}

void cpProcessHit(int idx) {
  if (cpWinner >= 0) return;
  CaptureTarget &ct = cpTargets[idx];
  if (ct.owner < 0) return;
  if (ct.locked) return;
  if (ct.captured && !cpRecapture) return;
  ct.hitCount++;
  sendFlash(idx, 255, 255, 0, 1);
  if (ct.hitCount >= cpHitsToCapture) {
    int prevOwner = ct.captured ? ct.capturedBy : ct.owner;
    int newOwner = -1;
    if (tbNumTeams == 2) {
      newOwner = (prevOwner == 0) ? 1 : 0;
    } else {
      for (int t = 0; t < (int)tbNumTeams; t++) {
        if (t != prevOwner) { newOwner = t; break; }
      }
    }
    if (ct.captured && ct.capturedBy >= 0) {
      if (cpCaptures[ct.capturedBy] > 0) cpCaptures[ct.capturedBy]--;
    }
    ct.captured = true;
    ct.capturedBy = newOwner;
    ct.hitCount = 0;
    ct.capturedAt = millis();
    if (cpLockTime == 0 || !cpRecapture) ct.locked = true;
    if (newOwner >= 0) cpCaptures[newOwner]++;
    if (newOwner >= 0) {
      sendLightOn(idx, tbColor[newOwner][0], tbColor[newOwner][1], tbColor[newOwner][2]);
    }
    cpCheckWin();
  }
}

void cpCheckWin() {
  for (int t = 0; t < (int)tbNumTeams; t++) {
    if (cpCaptures[t] >= cpWinCount[t]) {
      cpWinner = t;
      sendAllOff();
      for (int i = 0; i < numTargets; i++) {
        if (!isMyTarget(i) || !targets[i].online) continue;
        sendFlash(i, tbColor[t][0], tbColor[t][1], tbColor[t][2], 5);
      }
      finalTimeMs = millis() - gameStartTime - totalPausedMs;
      schedulePending(2000, ACT_END_GAME);
      return;
    }
  }
}

void handleCaptureLogic() {
  if (gameState != STATE_RUNNING || currentMode != MODE_CAPTURE || cpWinner >= 0) return;
  unsigned long now = millis();
  if (cpLockTime > 0 && cpRecapture) {
    unsigned long lockMs = (unsigned long)cpLockTime * 60UL * 1000UL;
    for (int i = 0; i < numTargets; i++) {
      if (!isMyTarget(i) || !targets[i].online) continue;
      CaptureTarget &ct = cpTargets[i];
      if (ct.captured && !ct.locked && ct.capturedAt > 0 && (now - ct.capturedAt) >= lockMs) {
        ct.locked = true;
        int t = ct.capturedBy;
        if (t >= 0) sendLightOn(i, tbColor[t][0], tbColor[t][1], tbColor[t][2]);
      }
    }
  }
  if (now - cpLastFlash >= 100) {
    cpLastFlash = now;
    for (int i = 0; i < numTargets; i++) {
      if (!isMyTarget(i) || !targets[i].online) continue;
      CaptureTarget &ct = cpTargets[i];
      if (!ct.captured || ct.capturedBy < 0) continue;
      unsigned long flashInterval;
      if (ct.locked) {
        flashInterval = 800;
      } else if (cpLockTime > 0 && ct.capturedAt > 0) {
        unsigned long lockMs = (unsigned long)cpLockTime * 60UL * 1000UL;
        unsigned long elapsed = now - ct.capturedAt;
        float progress = (float)elapsed / (float)lockMs;
        if (progress > 1.0f) progress = 1.0f;
        flashInterval = 600 - (unsigned long)(500.0f * progress);
      } else {
        flashInterval = 500;
      }
      bool on = ((now / flashInterval) % 2) == 0;
      int t = ct.capturedBy;
      if (on) sendLightOn(i, tbColor[t][0], tbColor[t][1], tbColor[t][2]);
      else sendLightOff(i);
    }
  }
}

// ---- Hit processing ----

void processHit(HitEvent &hit) {
  int idx = findTargetByID(hit.targetId);
  if (idx < 0) return;
  
  // Hit ontvangen = target is weer online
  targets[idx].online = true;
  targets[idx].lastPong = millis();
  
  targets[idx].hits++;
  totalHits++;
  flashMasterLed(60);  // Onboard LED direct aan
  
  if (gameState != STATE_RUNNING) return;
  
  switch (currentMode) {
    case MODE_FREEPLAY:
      currentScore += 10;
      sendFlash(idx, 255, 255, 0, 1);      // 100ms geel = raak
      break;
      
    case MODE_SHOOTNOSHOOT:
      if (targets[idx].isShoot) {
        currentScore += 10;
        targets[idx].shootHit = true;
        if (settings.snsOffMode) {
          sendFlash(idx, 255, 0, 0, 2);     // Off mode: korte flash rood = goed
          schedulePending(250, ACT_LIGHT_OFF, idx);
        } else {
          sendFlash(idx, 255, 0, 0, 20);    // On mode: 2 sec rood = goed
        }
      } else {
        currentScore -= 3; totalMisses++;
        if (settings.snsOffMode) {
          sendFlash(idx, 0, 255, 0, 2);     // Off mode: korte flash groen = fout
          schedulePending(250, ACT_LIGHT_OFF, idx);
        } else {
          sendFlash(idx, 0, 255, 0, 20);    // On mode: 2 sec groen = fout
        }
      }
      // Check of ALLE shoot-targets geraakt zijn
      {
        bool allShootDone = true;
        for (int i = 0; i < numTargets; i++) {
          if (!isMyTarget(i)) continue;
          if (!targets[i].online) continue;
          if (targets[i].isShoot && !targets[i].shootHit) {
            allShootDone = false;
            break;
          }
        }
        if (allShootDone) {
          finalTimeMs = millis() - gameStartTime - totalPausedMs;
          schedulePending(300, ACT_END_GAME);
        }
      }
      break;
      
    case MODE_SEQUENCE:
      if (seqWaiting) break;  // Wacht op delay, negeer hits
      // No-shoot check: groene target geraakt = straf
      if ((settings.noShootPct > 0 || settings.noShootCount > 0) && !targets[idx].isShoot) {
        currentScore -= 3; totalMisses++;
        sendFlash(idx, 0, 255, 0, 3);  // groene flash = fout!
        Serial.printf("SEQ: NO-SHOOT geraakt T%d! -3\n", targets[idx].id);
        break;
      }
      if (idx == seqCurrentIdx) {
        seqCurHits++;
        sendFlash(idx, 255, 255, 0, 1);    // 100ms geel = raak
        if (seqCurHits >= settings.seqHitsToOff) {
          currentScore += 10;
          advanceSequence();
        } else {
          currentScore += 3;  // Partial hit bonus
        }
      } else {
        currentScore -= 3; totalMisses++;
        sendFlash(idx, 255, 0, 0, 1);    // 100ms rood = fout target
      }
      break;
      
    case MODE_RANDOM:
      if (randWaiting) break;
      // No-shoot check
      if ((settings.noShootPct > 0 || settings.noShootCount > 0) && !targets[idx].isShoot) {
        currentScore -= 3; totalMisses++;
        sendFlash(idx, 0, 255, 0, 3);
        Serial.printf("RAND: NO-SHOOT geraakt T%d! -3\n", targets[idx].id);
        break;
      }
      if (idx == randCurrentIdx) {
        currentScore += 10;
        sendFlash(idx, 255, 255, 0, 1);    // 100ms geel = raak
        Serial.printf("RANDOM: HIT! Target T%d geraakt\n", targets[idx].id);
        advanceRandom();                   // Direct, geen delay
      } else {
        currentScore -= 3; totalMisses++;
        sendFlash(idx, 255, 0, 0, 1);    // 100ms rood = fout target
        Serial.printf("RANDOM: FOUT target T%d, verwacht T%d\n", targets[idx].id, targets[randCurrentIdx].id);
      }
      break;
      
    case MODE_MANUAL:
      if (targets[idx].active) {
        currentScore += 10;
        sendFlash(idx, 255, 255, 0, 1);
        targets[idx].active = false;
      }
      break;
      
    case MODE_MEMORY:
      if (memPhase != 5) break;       // Alleen hits tijdens shoot phase
      
      if (idx == memSequence[memShootIdx]) {
        // GOED → groen branden (blijft groen)
        currentScore += 10;
        sendLightOn(idx, 0, 255, 0);  // groen = correct, blijft aan
        memShootIdx++;
        Serial.printf("MEMORY: Correct T%d! (%d/%d)\n", targets[idx].id, memShootIdx, settings.memLength);
        
        if (memShootIdx >= settings.memLength) {
          // GEWONNEN! Celebration flash
          for (int i = 0; i < numTargets; i++) {
            if (!isMyTarget(i)) continue;
            if (targets[i].online) {
              sendFlash(i, 0, 255, 0, 3);
              schedulePending(350, ACT_FLASH, i, 0, 255, 0, 3);
              schedulePending(700, ACT_FLASH, i, 0, 255, 0, 3);
            }
          }
          finalTimeMs = millis() - gameStartTime - totalPausedMs;
          schedulePending(1100, ACT_END_GAME);
          return;
        }
        
        // Als volgende target hetzelfde is → terug naar blauw
        if (memSequence[memShootIdx] == idx) {
          sendLightOn(idx, 0, 0, 255);
        }
      } else {
        // FOUT → rood flash, daarna terug naar blauw
        memWrongCount++;
        currentScore -= 3; totalMisses++;
        sendFlash(idx, 255, 0, 0, 2);  // rood flash
        // Na flash → target gaat vanzelf terug naar vorige kleur (blauw) via scheduled action
        schedulePending(200, ACT_FLASH, idx, 0, 0, 255, 1);  // terug blauw na 200ms
        Serial.printf("MEMORY: Fout! Verwacht T%d, kreeg T%d (fouten: %d/%d)\n", 
          targets[memSequence[memShootIdx]].id, targets[idx].id, memWrongCount, settings.memMaxWrong);
        
        if (memWrongCount >= settings.memMaxWrong) {
          Serial.printf("MEMORY: %d FOUTEN - GAME OVER!\n", settings.memMaxWrong);
          sendBuzzAll(3);
          for (int i = 0; i < numTargets; i++) {
            if (!isMyTarget(i)) continue;
            if (targets[i].online) {
              sendFlash(i, 255, 0, 0, 3);
              schedulePending(350, ACT_FLASH, i, 255, 0, 0, 3);
              schedulePending(700, ACT_FLASH, i, 255, 0, 0, 3);
            }
          }
          finalTimeMs = millis() - gameStartTime - totalPausedMs;
          schedulePending(1100, ACT_END_GAME);
          return;
        }
      }
      break;
      
    case MODE_RANDOMCOLOR:
      if (rcWaiting) break;
      
      // Check of dit target actief is
      if (idx >= 0 && idx < numTargets && rcTargetPlayer[idx] >= 0) {
        int player = rcTargetPlayer[idx];
        uint32_t reactionMs = millis() - rcCycleLitTime;
        
        // Punten voor de speler wiens kleur op dit target stond
        rcPlayers[player].correctHits++;
        rcPlayers[player].targetsHitThisCycle++;
        rcPlayers[player].totalReactionMs += reactionMs;
        
        // Score: 10 basis + snelheidsbonus
        int speedBonus = 0;
        if (reactionMs < 500) speedBonus = 10;
        else if (reactionMs < 1000) speedBonus = 7;
        else if (reactionMs < 1500) speedBonus = 4;
        else if (reactionMs < 2000) speedBonus = 2;
        
        rcPlayers[player].score += (10 + speedBonus);
        currentScore += (10 + speedBonus);
        
        // Target uit
        sendFlash(idx, 255, 255, 0, 1);  // kort geel = raak
        sendLightOff(idx);
        rcTargetPlayer[idx] = -1;
        if (rcActiveTargets > 0) rcActiveTargets--;

        Serial.printf("RC: %s raakt T%d! +%d (%dms) [%d/%d]\n", 
          rcPlayers[player].name, targets[idx].id, 10 + speedBonus, reactionMs,
          rcPlayers[player].correctHits, settings.rcRounds);
        
        // Check of alle spelers klaar zijn (total hits >= rcRounds)
        bool allDone = true;
        for (int p = 0; p < rcPlayerCount; p++) {
          if (rcPlayers[p].correctHits < settings.rcRounds) { allDone = false; break; }
        }
        if (allDone) {
          endGame();
        } else if (rcPlayers[player].correctHits < settings.rcRounds) {
          // Direct nieuw target voor deze speler (nooit dezelfde)
          refreshOnline();
          if (onlineCount == 0) break;  // geen targets meer beschikbaar
          int newTarget = -1;
          int tries = 0;
          do {
            newTarget = onlineIdx[random(0, onlineCount)];
            tries++;
          } while ((newTarget == idx || rcTargetPlayer[newTarget] >= 0) && tries < 30);
          
          if (newTarget >= 0 && rcTargetPlayer[newTarget] < 0) {
            rcTargetPlayer[newTarget] = player;
            rcPrevTargetPlayer[newTarget] = player;
            sendLightOn(newTarget, rcPlayers[player].r, rcPlayers[player].g, rcPlayers[player].b);
            rcActiveTargets++;
            rcCycleLitTime = millis();
          }
        }
      } else {
        // Fout target - straf voor dichtstbijzijnde actieve speler (of gewoon mis)
        sendFlash(idx, 255, 255, 255, 1);  // witte flash = mis
        Serial.printf("RC: Hit op inactief target %d\n", idx + 1);
      }
      break;
      
    case MODE_REACTION:
      if (rxWaiting) break;  // targets zijn nog niet aan
      // No-shoot check: groen target geraakt = straf
      if ((settings.noShootPct > 0 || settings.noShootCount > 0) && !targets[idx].isShoot) {
        currentScore -= 3; totalMisses++;
        sendFlash(idx, 0, 255, 0, 3);
        // Markeer als 'hit' zodat cycle kan doorgaan
        for (int p = 0; p < rcPlayerCount; p++) {
          if (rxPlayerTarget[p] == idx && !rxHit[p]) {
            rxHit[p] = true;
            if (rxCycle < RX_MAX_CYCLES) rxTimes[p][rxCycle] = 9999;  // miss marker
            Serial.printf("RX: %s raakt NO-SHOOT T%d! -3\n", rcPlayers[p].name, targets[idx].id);
            break;
          }
        }
        break;
      }
      // Welke speler hoort bij dit target?
      for (int p = 0; p < rcPlayerCount; p++) {
        if (rxPlayerTarget[p] == idx && !rxHit[p]) {
          uint32_t reactionMs = millis() - rxLitTime;
          rxHit[p] = true;
          if (rxCycle < RX_MAX_CYCLES) rxTimes[p][rxCycle] = (uint16_t)min((uint32_t)9998, reactionMs);
          
          // Geel = geraakt
          sendLightOn(idx, 255, 255, 0);
          
          rcPlayers[p].correctHits++;
          rcPlayers[p].totalReactionMs += reactionMs;
          currentScore += 10;
          
          Serial.printf("RX: %s → %dms (cyclus %d)\n", rcPlayers[p].name, reactionMs, rxCycle + 1);
          break;
        }
      }
      break;
      
    case MODE_PARCOURS:
      if (parWaiting) break;  // Wacht op delay, negeer hits
      // No-shoot check: groen parcours target geraakt = straf + permanent flash
      if ((settings.noShootPct > 0 || settings.noShootCount > 0) && !targets[idx].isShoot) {
        currentScore -= 3; totalMisses++;
        sendFlash(idx, 0, 255, 0, 20);  // lang groen flashen = fout (blijft flashen)
        Serial.printf("PAR: NO-SHOOT geraakt T%d! -3 (flasht permanent)\n", targets[idx].id);
        break;
      }
      if (idx == parOrder[parCurrent]) {
        parCurHits++;
        
        if (parCurHits >= settings.parHitsToOff) {
          // Target klaar!
          uint32_t elapsed = getElapsed();
          parSplits[parCurrent] = elapsed;
          currentScore += 10;
          sendLightOn(idx, 255, 255, 0);  // Geel = klaar
          
          Serial.printf("PARCOURS: T%d geraakt! (%d/%d) split=%dms\n", 
            targets[idx].id, parCurrent + 1, parTotal, elapsed);
          
          parCurrent++;
          parCurHits = 0;
          
          if (parCurrent >= parTotal) {
            finalTimeMs = millis() - gameStartTime - totalPausedMs;
            schedulePending(500, ACT_END_GAME);
          } else {
            uint16_t delayMs = settings.parDelayNext > 0 ? settings.parDelayNext : 300;
            parWaiting = true;
            parWaitStart = millis();
          }
        } else {
          // Partial hit — blauw flash
          sendFlash(idx, 255, 255, 0, 1);
          currentScore += 3;
        }
      }
      break;
      
    case MODE_FASTTRACK:
      if (ftWinner >= 0) break;  // game already won
      // No-shoot check
      if ((settings.noShootPct > 0 || settings.noShootCount > 0) && !targets[idx].isShoot) {
        currentScore -= 3; totalMisses++;
        sendFlash(idx, 0, 255, 0, 3);
        Serial.printf("FT: NO-SHOOT geraakt T%d! -3\n", targets[idx].id);
        break;
      }
      {
        // Which player owns this target?
        int player = -1;
        for (int p = 0; p < rcPlayerCount; p++) {
          if (ftPlayerTarget[p] == idx) { player = p; break; }
        }

        if (player >= 0) {
          ftPlayerHits[player]++;
          rcPlayers[player].score += 10;
          rcPlayers[player].correctHits++;
          currentScore += 10;
          
          sendFlash(idx, 255, 255, 0, 1);  // geel = raak
          
          Serial.printf("FASTTRACK: %s raakt T%d! (%d/%d)\n",
            rcPlayers[player].name, targets[idx].id, 
            ftPlayerHits[player], settings.ftTargetsPerPlayer);
          
          // Check win condition
          if (ftPlayerHits[player] >= settings.ftTargetsPerPlayer) {
            ftHandleWin(player);
          } else {
            // Assign next target for this player
            sendLightOff(idx);
            int next = ftPickTarget(player);
            ftPlayerTarget[player] = next;
            if (next >= 0) schedulePending(200, ACT_FT_LIGHT, player);
          }
        }
      }
      break;
      
    case MODE_TOURNAMENT:
      if (tbWinner >= 0) break;
      {
        // Find which team's current target was hit
        int team = -1;
        for (int t = 0; t < (int)tbNumTeams; t++) {
          if (tbTeam[t].curTarget == idx && !tbTeam[t].done) { team = t; break; }
        }
        // For Fast Track mode: any of the team's targets can be hit (all lit at once)
        if (team < 0) {
          for (int t = 0; t < (int)tbNumTeams; t++) {
            if (tbTeam[t].done || tbTeam[t].gameMode != TB_GAME_FASTTRACK) continue;
            for (int j = 0; j < tbTeam[t].count; j++) {
              if (tbTeam[t].tgts[j] == idx) { team = t; break; }
            }
            if (team >= 0) break;
          }
        }
        // For Memory mode, also check if it's any of the team's targets during replay
        if (team < 0) {
          for (int t = 0; t < (int)tbNumTeams; t++) {
            if (tbTeam[t].done || tbTeam[t].gameMode != TB_GAME_MEMORY || tbTeam[t].memPhase != 1) continue;
            for (int j = 0; j < tbTeam[t].count; j++) {
              if (tbTeam[t].tgts[j] == idx) { team = t; break; }
            }
            if (team >= 0) break;
          }
        }
        if (team >= 0) tbProcessHit(team, idx);
      }
      break;

    case MODE_CAPTURE:
      cpProcessHit(idx);
      break;
  }

  Serial.printf("HIT target=%d score=%d\n", hit.targetId, currentScore);
}

// ============================================================
// LEADERBOARD
// ============================================================

const char* getModeName(GameMode m) {
  switch (m) {
    case MODE_FREEPLAY:     return "Free Play";
    case MODE_SHOOTNOSHOOT: return "Shoot/No-Shoot";
    case MODE_SEQUENCE:     return "Sequence";
    case MODE_RANDOM:       return "Random";
    case MODE_MANUAL:       return "Manual";
    case MODE_MEMORY:       return "Memory";
    case MODE_RANDOMCOLOR:  return "Random Multi";
    case MODE_REACTION:     return "Reaction";
    case MODE_PARCOURS:     return "Parcours";
    case MODE_FASTTRACK:    return "Fast Track";
    case MODE_TOURNAMENT:   return "Zone";
    case MODE_CAPTURE:      return "Capture";
    default:                return "Onbekend";
  }
}

void loadScores() {
  preferences.begin("scores3", true);  // Bumped: ScoreEntry now includes hits
  for (int i = 0; i < MAX_SCORES; i++) {
    String k = "s" + String(i);
    if (preferences.isKey(k.c_str())) {
      preferences.getBytes(k.c_str(), &topScores[i], sizeof(ScoreEntry));
    } else {
      strcpy(topScores[i].name, "---");
      strcpy(topScores[i].mode, "");
      topScores[i].score = 0;
      topScores[i].hits = 0;
      topScores[i].timeMs = 0;
    }
  }
  preferences.end();
}

void saveScores() {
  preferences.begin("scores3", false);
  for (int i = 0; i < MAX_SCORES; i++) {
    String k = "s" + String(i);
    preferences.putBytes(k.c_str(), &topScores[i], sizeof(ScoreEntry));
  }
  preferences.end();
}

void tryInsertScore(const char* name, int32_t score, uint16_t hits, uint32_t timeMs, const char* modeName) {
  int pos = -1;
  for (int i = 0; i < MAX_SCORES; i++) {
    if (strcmp(topScores[i].name, "---") == 0) {
      pos = i;
      break;
    }
    if (hits > topScores[i].hits) {
      pos = i;
      break;
    }
    if (hits == topScores[i].hits && timeMs < topScores[i].timeMs) {
      pos = i;
      break;
    }
  }
  if (pos < 0) return;

  for (int i = MAX_SCORES - 1; i > pos; i--) {
    topScores[i] = topScores[i - 1];
  }

  strncpy(topScores[pos].name, name, 15);
  topScores[pos].name[15] = '\0';
  strncpy(topScores[pos].mode, modeName, 15);
  topScores[pos].mode[15] = '\0';
  topScores[pos].score = score;
  topScores[pos].hits = hits;
  topScores[pos].timeMs = timeMs;

  saveScores();
  Serial.printf("LEADERBOARD: #%d %s [%s] hits=%d score=%d time=%dms\n", pos+1, name, modeName, hits, score, timeMs);
}

// ============================================================
// WEB SERVER - MAIN PAGE
// ============================================================

void handleRoot() {
  static const char page[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="nl"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>RAF RTT Training System</title>
<style>
:root{--r:#DC0000;--w:#fff;--b:#000;--bg:#f5f5f5;--card:#fff;--shadow:0 2px 8px rgba(0,0,0,.08);--radius:12px;--trans:all .25s ease}
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent;-webkit-touch-callout:none}
body{font-family:-apple-system,'Segoe UI',Roboto,sans-serif;background:var(--bg);color:var(--b);touch-action:manipulation;-webkit-text-size-adjust:100%;min-height:100dvh;padding-bottom:72px}
.wrap{max-width:600px;margin:0 auto;padding:8px}
/* Screens */
.scr{display:none}.scr.active{display:block}
/* Header */
.hdr{display:flex;align-items:center;justify-content:space-between;padding:10px 12px;background:var(--r);color:var(--w);border-radius:0 0 16px 16px;margin:-8px -8px 12px -8px}
.hdr-logo{font-weight:900;font-size:1.3em;letter-spacing:2px}
.hdr-badge{background:rgba(255,255,255,.2);padding:4px 10px;border-radius:20px;font-size:.8em;font-weight:600}
/* Player bar */
.pbar{display:flex;gap:8px;align-items:center;margin-bottom:12px}
.pbar input{flex:1;padding:10px 12px;border:2px solid #ddd;border-radius:var(--radius);font-size:1em;background:var(--w);min-height:48px}
.pbar input:focus{border-color:var(--r);outline:none}
.nfc-dot{width:12px;height:12px;border-radius:50%;background:#ccc;flex-shrink:0}
.nfc-dot.on{background:#4CAF50;box-shadow:0 0 6px #4CAF50}
/* Category tabs */
.cat-tabs{display:flex;gap:6px;margin-bottom:12px;overflow-x:auto;padding:2px 0}
.cat-tab{padding:8px 16px;border-radius:20px;border:2px solid #ddd;background:var(--w);font-weight:600;font-size:.85em;cursor:pointer;white-space:nowrap;min-height:40px;transition:var(--trans)}
.cat-tab.active{background:var(--r);color:var(--w);border-color:var(--r)}
/* Mode grid */
.mgrid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.mcard{background:var(--card);border:2px solid #eee;border-radius:var(--radius);padding:12px;cursor:pointer;transition:var(--trans);position:relative;min-height:80px}
.mcard:active{transform:scale(.97)}
.mcard.sel{border-color:var(--r);box-shadow:0 0 0 2px rgba(220,0,0,.15)}
.mcard-icon{font-size:1.6em;margin-bottom:4px}
.mcard-name{font-weight:700;font-size:.85em;margin-bottom:2px}
.mcard-desc{font-size:.72em;color:#666;line-height:1.3}
.mcard-play{position:absolute;bottom:8px;right:8px;width:36px;height:36px;border-radius:50%;background:var(--r);color:var(--w);border:none;font-size:1em;cursor:pointer;display:flex;align-items:center;justify-content:center;box-shadow:0 2px 6px rgba(220,0,0,.3)}
.mcard-play:active{transform:scale(.9)}
/* Settings overlay */
.overlay{position:fixed;inset:0;background:rgba(0,0,0,.4);z-index:200;display:none;align-items:flex-end;justify-content:center}
.overlay.open{display:flex}
.overlay-panel{background:var(--w);border-radius:20px 20px 0 0;width:100%;max-width:600px;max-height:88dvh;overflow-y:auto;padding:16px 16px 24px;animation:slideUp .3s ease}
@keyframes slideUp{from{transform:translateY(100%)}to{transform:translateY(0)}}
@keyframes cpPulse{0%,100%{opacity:1}50%{opacity:.3}}
.animate-pulse{animation:cpPulse 1s ease-in-out infinite}
.ov-hdr{display:flex;align-items:center;gap:10px;margin-bottom:16px;padding-bottom:12px;border-bottom:2px solid #eee}
.ov-back{width:40px;height:40px;border-radius:50%;border:2px solid #ddd;background:var(--w);font-size:1.1em;cursor:pointer;display:flex;align-items:center;justify-content:center}
.ov-title{font-weight:800;font-size:1.1em;flex:1}
/* Form elements */
.field{margin-bottom:14px}
.field label{display:block;font-weight:600;font-size:.85em;margin-bottom:4px;color:#333}
.field input[type=number],.field input[type=text],.field select{width:100%;padding:10px 12px;border:2px solid #ddd;border-radius:10px;font-size:.95em;background:var(--w);min-height:44px}
.field input:focus,.field select:focus{border-color:var(--r);outline:none}
.chk{display:flex;align-items:center;gap:8px;padding:10px 0;font-size:.9em;min-height:48px;cursor:pointer}
.chk input[type=checkbox]{width:22px;height:22px;accent-color:var(--r)}
.slider-f{margin:6px 0}
.slider-f input[type=range]{width:100%;height:36px;-webkit-appearance:none;appearance:none;background:transparent;cursor:pointer}
.slider-f input[type=range]::-webkit-slider-track{height:8px;background:#ccc;border-radius:4px}
.slider-f input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:28px;height:28px;background:var(--r);border-radius:50%;margin-top:-10px;box-shadow:0 1px 4px rgba(0,0,0,.3)}
.slider-val{text-align:center;font-weight:700;color:var(--r);font-size:.95em}
/* Toggle buttons */
.tog-row{display:flex;gap:6px;margin:6px 0}
.tog-btn{flex:1;padding:10px;border-radius:10px;border:2px solid #ddd;background:var(--w);font-weight:700;font-size:.85em;cursor:pointer;text-align:center;min-height:48px;transition:var(--trans)}
.tog-btn.on{background:var(--r);color:var(--w);border-color:var(--r)}
/* Player name rows */
.prow{display:flex;align-items:center;gap:8px;margin:6px 0}
.pdot{width:18px;height:18px;border-radius:4px;flex-shrink:0}
.prow input{flex:1;padding:8px 10px;border:2px solid #ddd;border-radius:8px;font-size:.9em;min-height:44px}
/* Start button */
.start-btn{width:100%;padding:16px;background:var(--r);color:var(--w);border:none;border-radius:var(--radius);font-size:1.15em;font-weight:800;cursor:pointer;margin-top:16px;min-height:56px;letter-spacing:1px;box-shadow:0 4px 12px rgba(220,0,0,.3)}
.start-btn:active{transform:scale(.98);box-shadow:0 2px 6px rgba(220,0,0,.2)}
/* Game view */
.game-timer{font-family:'SF Mono',Menlo,monospace;font-size:2.5em;font-weight:700;text-align:center;padding:8px 0}
.game-state{text-align:center;margin-bottom:8px}
.state-badge{display:inline-block;padding:4px 14px;border-radius:20px;font-weight:700;font-size:.8em}
.sb-cd{background:#FFA500;color:var(--w)}.sb-run{background:var(--r);color:var(--w)}
.sb-pause{background:#FFD700;color:var(--b)}.sb-end{background:var(--b);color:var(--w)}
.game-score{text-align:center;font-size:3em;font-weight:900;color:var(--r);line-height:1.1}
.game-stats{display:flex;justify-content:center;gap:20px;font-size:1.1em;margin:6px 0 10px}
.game-stats span{font-weight:600}
.game-remaining{text-align:center;font-size:.9em;color:#666;margin-bottom:8px;min-height:22px}
.game-tgrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(52px,1fr));gap:4px;margin:8px 0}
.gt-box{padding:6px 4px;border:2px solid var(--b);border-radius:8px;text-align:center;font-size:.7em;font-weight:700;transition:var(--trans);background:#f9f9f9}
.gt-box.on{color:var(--w);border-color:transparent;box-shadow:0 0 8px rgba(0,0,0,.25)}
.gt-box.off{opacity:.3;background:#eee;border-color:#999}
/* Player cards (multiplayer) */
.pcards{display:flex;flex-direction:column;gap:8px;margin:10px 0}
.pcard{border:2px solid #ddd;border-radius:var(--radius);padding:10px 12px;background:var(--w);box-shadow:var(--shadow)}
.pcard-hdr{display:flex;align-items:center;gap:8px;margin-bottom:6px}
.pcard-dot{width:14px;height:14px;border-radius:3px;flex-shrink:0}
.pcard-name{font-weight:700;font-size:.95em;flex:1}
.pcard-score{font-size:1.3em;font-weight:800}
.pcard-bar{height:6px;background:#eee;border-radius:3px;margin:6px 0;overflow:hidden}
.pcard-fill{height:100%;border-radius:3px;transition:width .3s}
.pcard-stats{display:flex;gap:12px;font-size:.75em;color:#666}
/* Game controls */
.gctrl{display:flex;gap:8px;margin:12px 0}
.gctrl button{flex:1;padding:14px;border-radius:var(--radius);font-weight:700;font-size:.9em;cursor:pointer;min-height:52px;border:2px solid}
.btn-pause{background:var(--w);color:var(--r);border-color:var(--r)}
.btn-stop{background:var(--b);color:var(--w);border-color:var(--b)}
/* Results */
.res-header{text-align:center;padding:16px 0 8px}
.res-header h2{font-size:1.4em;color:var(--r)}
.res-bigscore{text-align:center;font-size:3.5em;font-weight:900;color:var(--r);padding:8px 0}
.res-stats{display:flex;justify-content:center;gap:16px;font-size:.95em;margin-bottom:16px}
.res-stats div{text-align:center}
.res-stats .rs-val{font-size:1.4em;font-weight:700}
.res-stats .rs-lbl{font-size:.75em;color:#666}
.save-row{display:flex;gap:8px;margin:12px 0}
.save-row input{flex:1;min-height:48px}
.save-row button{padding:10px 16px;background:var(--r);color:var(--w);border:none;border-radius:10px;font-weight:700;cursor:pointer;min-height:48px}
.res-btns{display:flex;gap:8px;margin:12px 0}
.res-btns button{flex:1;padding:12px;border-radius:var(--radius);font-weight:700;font-size:.9em;cursor:pointer;min-height:48px}
.btn-again{background:var(--r);color:var(--w);border:2px solid var(--r)}
.btn-other{background:var(--w);color:var(--b);border:2px solid #ddd}
/* Medal cards */
.medal-card{border:2px solid;border-radius:var(--radius);padding:12px;margin-bottom:8px;position:relative}
.medal-rank{font-size:1.4em;margin-right:8px}
/* Leaderboard */
.lb-table{width:100%;border-collapse:collapse;margin-top:8px;font-size:.85em}
.lb-table th{background:var(--r);color:var(--w);padding:8px;text-align:left;font-size:.8em}
.lb-table td{padding:6px 8px;border-bottom:1px solid #eee}
.lb-table tr:first-child td{font-weight:700}
/* Extra screen */
.extra-card{background:var(--card);border-radius:var(--radius);padding:12px;margin-bottom:10px;box-shadow:var(--shadow)}
.extra-card h3{color:var(--r);font-size:.9em;margin-bottom:8px;padding-bottom:6px;border-bottom:2px solid #eee}
.act-row{display:flex;align-items:center;gap:4px;margin:4px 0}
.act-row span{min-width:28px;font-weight:700;font-size:.85em}
.cbtn{padding:6px 4px;border-radius:6px;border:2px solid;font-weight:700;font-size:.7em;cursor:pointer;flex:1;text-align:center;min-height:36px}
.cbtn-r{background:var(--r);color:var(--w);border-color:var(--r)}
.cbtn-g{background:#00AA00;color:var(--w);border-color:#00AA00}
.cbtn-b{background:#0044DD;color:var(--w);border-color:#0044DD}
.cbtn-off{background:var(--b);color:var(--w);border-color:var(--b)}
.alloff-btn{width:100%;padding:10px;background:var(--b);color:var(--w);border:none;border-radius:10px;font-weight:700;cursor:pointer;margin-top:6px;min-height:48px}
.link-btn{display:block;background:var(--r);color:var(--w);text-align:center;padding:12px;border-radius:10px;text-decoration:none;font-weight:700;margin:6px 0;min-height:48px;line-height:24px}
/* Zones screen */
.zone-nums{display:flex;gap:8px;margin:8px 0}
.zone-nums button{flex:1;padding:12px;border-radius:10px;border:2px solid #ddd;background:var(--w);font-weight:700;font-size:1em;cursor:pointer;min-height:48px;transition:var(--trans)}
.zone-nums button.on{background:var(--r);color:var(--w);border-color:var(--r)}
.zt-row{display:flex;align-items:center;gap:4px;margin:4px 0;padding:6px;background:#f9f9f9;border-radius:8px}
.zt-row span{min-width:32px;font-weight:700;font-size:.85em}
.zbtn{flex:1;padding:6px 2px;border-radius:6px;border:2px solid;font-weight:700;font-size:.72em;cursor:pointer;text-align:center;min-height:36px;transition:var(--trans)}
.zone-summary{display:flex;gap:6px;margin:10px 0;flex-wrap:wrap}
.zone-summary div{flex:1;text-align:center;padding:6px;border:2px solid;border-radius:8px;font-size:.8em;font-weight:700;min-width:80px}
/* Bottom nav */
.bnav{position:fixed;bottom:0;left:0;right:0;background:var(--w);border-top:2px solid #eee;display:flex;z-index:100;box-shadow:0 -2px 8px rgba(0,0,0,.06)}
.bnav.hidden{display:none}
.bnav button{flex:1;padding:10px 0;border:none;background:transparent;font-size:.8em;font-weight:600;color:#999;cursor:pointer;display:flex;flex-direction:column;align-items:center;gap:2px;min-height:56px}
.bnav button.active{color:var(--r)}
.bnav button .nav-icon{font-size:1.3em}
/* Toast */
.toast{position:fixed;top:12px;left:50%;transform:translateX(-50%);background:#333;color:var(--w);padding:10px 20px;border-radius:10px;font-size:.85em;z-index:9999;opacity:0;transition:opacity .3s;pointer-events:none}
.toast.show{opacity:1}
/* Route builder */
.route-list{min-height:40px;border:2px dashed #ddd;border-radius:10px;padding:6px;display:flex;flex-wrap:wrap;gap:4px;margin:6px 0}
.route-tag{display:inline-flex;align-items:center;gap:4px;padding:4px 8px;background:var(--r);color:var(--w);border-radius:6px;font-size:.8em;font-weight:700}
.route-tag .rx{cursor:pointer;color:rgba(255,255,255,.7)}
.route-avail{display:flex;flex-wrap:wrap;gap:6px;margin:6px 0}
.route-avail button{padding:8px 14px;border-radius:8px;border:2px solid var(--r);background:var(--w);font-weight:700;font-size:.85em;cursor:pointer;min-width:48px;min-height:44px}
.route-actions{display:flex;gap:6px}
.route-actions button{flex:1;padding:8px;border-radius:8px;border:2px solid #ddd;background:var(--w);font-size:.78em;font-weight:600;cursor:pointer;min-height:40px}
/* Zone bar (top, for multi-zone during game) */
.zone-bar{display:flex;gap:4px;padding:4px 8px;background:#111;border-radius:10px;margin-bottom:8px;overflow-x:auto;align-items:center}
.zone-bar button{padding:6px 12px;border-radius:6px;border:1px solid #444;background:#1a1a1a;color:#aaa;font-size:.8em;cursor:pointer;white-space:nowrap;min-height:36px;font-weight:600}
.zone-bar button.zact{background:var(--r);color:var(--w);border-color:var(--r)}
/* Confirm dialog */
.confirm-overlay{position:fixed;inset:0;background:rgba(0,0,0,.5);z-index:300;display:none;align-items:center;justify-content:center}
.confirm-overlay.open{display:flex}
.confirm-box{background:var(--w);border-radius:16px;padding:24px;max-width:320px;width:90%;text-align:center}
.confirm-box p{font-size:1em;margin-bottom:16px;font-weight:600}
.confirm-box .cb-btns{display:flex;gap:8px}
.confirm-box .cb-btns button{flex:1;padding:12px;border-radius:10px;font-weight:700;cursor:pointer;min-height:48px;font-size:.95em}
@media(min-width:500px){.mgrid{grid-template-columns:1fr 1fr 1fr}}
/* Small phones (≤360px) */
@media(max-width:360px){
  .wrap{padding:4px}
  .hdr{padding:8px 8px;margin:-4px -4px 8px -4px}
  .hdr-logo{font-size:1.05em;letter-spacing:1px}
  .hdr svg{height:32px!important}
  .hdr-badge{font-size:.7em;padding:3px 7px}
  .game-timer{font-size:2em}
  .game-score{font-size:2.2em}
  .game-stats{font-size:.95em;gap:12px}
  .game-tgrid{grid-template-columns:repeat(auto-fill,minmax(44px,1fr));gap:3px}
  .gt-box{font-size:.6em;padding:5px 2px}
  .gctrl button{padding:10px;font-size:.8em;min-height:44px}
  .mcard{padding:8px;min-height:64px}
  .mcard .mc-icon{font-size:1.4em}
  .mcard .mc-title{font-size:.85em}
  .mcard .mc-desc{font-size:.65em}
  .mgrid{gap:6px}
  .start-btn{padding:14px;font-size:1em;min-height:48px}
  .pbar input{min-height:42px;font-size:.9em}
  .cat-tab{padding:6px 12px;font-size:.8em;min-height:36px}
  .bnav button{min-height:48px;font-size:.7em;padding:6px 0}
  .lb-table{font-size:.75em}
  .lb-table th,.lb-table td{padding:4px 3px}
  .overlay-panel{padding:16px 10px;max-width:100%;width:100%}
  .field input,.field select{font-size:.9em}
  body{padding-bottom:56px}
}
/* Medium phones (361-399px) */
@media(min-width:361px) and (max-width:399px){
  .hdr svg{height:36px!important}
  .hdr-logo{font-size:1.15em}
  .game-timer{font-size:2.2em}
  .game-tgrid{grid-template-columns:repeat(auto-fill,minmax(48px,1fr))}
  .lb-table{font-size:.8em}
}
/* Landscape phones */
@media(max-height:420px) and (orientation:landscape){
  .game-timer{font-size:1.8em;padding:4px 0}
  .game-score{font-size:2em}
  .game-stats{margin:2px 0 4px}
  .gctrl{margin:6px 0}
  .gctrl button{padding:8px;min-height:40px}
  body{padding-bottom:52px}
  .bnav button{min-height:44px;padding:4px 0}
  .hdr{padding:6px 10px;border-radius:0 0 10px 10px}
}
/* Tablets & wider screens */
@media(min-width:768px){
  .wrap{max-width:700px}
  .mgrid{grid-template-columns:1fr 1fr 1fr;gap:12px}
  .game-timer{font-size:3em}
  .game-score{font-size:3.5em}
  .game-tgrid{grid-template-columns:repeat(auto-fill,minmax(64px,1fr));gap:6px}
}
</style>
</head><body>
<div class="wrap">
<!-- GLOBAL HEADER — altijd zichtbaar -->
<div class="hdr">
  <div style="display:flex;align-items:center;gap:8px">
    <svg viewBox="0 0 75.89 80.38" style="height:44px" xmlns="http://www.w3.org/2000/svg"><style>.lw{fill:#fff;stroke:#fff;stroke-miterlimit:10;stroke-width:.25px}</style><path class="lw" d="M28.14,54.37c.77,1.63,1.99,2.77,3,4.28,2.61.02,5.25.04,7.92.05,0-.53,0-.99,0-1.46.05,0,.04,0,.09-.02.15.48.29.96.45,1.48.23,0,.45,0,.75,0-.32-1.12-.62-2.15-.93-3.23-1.9.02-3.52-1.1-5.28-2.04l-.22-.13s.05-.97.33-2.12l3.53-1.6c.26-.12.48-.31.63-.56l1.6-2.56-3.88.22h-.08c-.11,0-.21-.02-.32-.02h-.06s-.11-.03-.11-.03h-.05s-1.98-.58-1.98-.58l-3.61,3.17c-.07.07-.14.14-.21.21l-.04.04-.09.08-.9.6-.38,2.63-.02.05h.01c-.08.23-.13.43-.17.6v.95s0,0,0,0Z"/><path class="lw" d="M56,14.94c.1.16.2.34.31.47.44.58.89,1.16,1.33,1.73.35.45.7.9,1.04,1.35.76.7,1.3,1.06,1.67,1.25,0,0,0,0,0,0h6.24s0,0,0,0h0,0s-.83-3.57-.83-3.57h0c-.17.6-.34,1.2-.88,1.47l-.21-.96c.03.35-.31.8-.6,1.11-.84,0-1.75-.4-2.51-.56-.48-.1-1.28-.23-1.37-.83-.31-.35-.24-1.22,2.37-3.39l2.15-.53.46.72.32-.91.56-.14.39,1.36,1.49-2.94v-1.5s-1.95.69-1.95.69l1.97-1.33v-.58c-2.23-.07-6.8-.43-9.99-2.03l-.84-.45c-.01.35-.05.68-.11.99-.1.55-.21,1.11-.31,1.64-.45,2.3-.87,4.49-.67,6.92h0ZM60.82,8.15l.9,1.09c-3.84-.35-4.08-1.11-4.08-1.11l3.18.02h0Z"/><path class="lw" d="M53.61,58.72c.9,0,1.79.01,2.69.02,0-.53,0-.99,0-1.46.05,0,.04,0,.09-.02.15.48.29.96.45,1.48.23,0,.45,0,.75,0-.32-1.12-.62-2.15-.93-3.23-1.06.01-2.04-.33-3-.8,0-.03,0-.06,0-.09-.81-.49-.94-1-.94-1l-.37-1.85s-.04-.03-.05-.04c0,0,.01,0,.02,0l-.18-1.16-.77-5.49.06-.41.02.02.9-6.25c.04-.26.04-.53,0-.79l-1.02-6.43c-.08-.48-.33-.89-.68-1.16l-3.89-4.11c-.44-.29-3.37-2.09-5.99-.18l-.35.28v.04s-1.05.91-1.05.91c0,0-2.53,1.99-5.06,3.68l-7.73,4.71-3.21,1.17c-.14.05-.27.09-.38.14l-.29.11v.02c-.21.09-.38.18-.52.27l-.65.65c-.4,1.13-.51,2.27-.65,3.43,0,0-.01,0-.02,0-.02.08-.04.16-.06.23-.21.79-.51,1.48-.99,2l-3.38,3.77-2.1,5.02c-.06.14-.12.27-.16.39l-.13.3h.01c-.08.23-.13.43-.17.6v.95c.77,1.63,2,2.77,3.01,4.28,2.61.02,5.25.04,7.92.05,0-.53,0-.99,0-1.46.05,0,.04,0,.09-.02.15.48.29.96.45,1.48.23,0,.45,0,.75,0-.32-1.12-.62-2.15-.93-3.23-1.9.02-3.52-1.1-5.28-2.04l-.22-.13,1.06-1.14,2.15.02c.32,0,.64-.09.91-.28l2.3-1.53s.06-.06.09-.09l.84-.57v.05l1.88-1.26s0,0,.01,0c.07-.07.15-.15.22-.22,0,0,0,0,0,0l8.5-7.48-4.01,3.99,2.15.61s0,0,.01,0c.1,0,.2.02.31.02h0l8.01-.45-2.56-3.78,3.75,4.35c.52.6.73,1.09.81,1.42.03.14.04.28.05.43l-.12,1.56v1.71s-.03,1.41.54,2.3c0,0,.9,1.68,1.96,2.99.67.94,1.45,1.79,2.13,2.8.97,0,1.95.01,2.92.02,0-.03,0-.06,0-.09h0Z"/><path class="lw" d="M66.59,58.74h2.7v-1.52c.05,0,.04,0,.09-.02.15.48.29.96.45,1.48h0l.02.06h.74l-.02-.05h0c-.32-1.12-.62-2.15-.93-3.23-1.06.01-2.04-.33-3-.8,0,0-.79-.39-1.63-.97-.62-.5-.94-1.14-.94-1.14l-.62-.93s-.04-.01-.06-.01c0,0,0,0,.01-.01l-.6-.99-5.67-9.76c-.42.64-.93,1.2-1.41,1.72-.08.08-.15.17-.23.25-.35.38-.89.48-1.34.23-.17.26-.36.51-.55.75-.22.27-.53.41-.85.41-.01,0-.03,0-.04,0l-.1.7c-.01.08-.03.15-.05.22l.41,2.9,1.71.82c.7.34,1.07.7,1.27.96.07.11.14.24.21.38l.46,1.49s0,.02.02.04l.62,1.53s.49,1.32,1.34,1.89c0,0,1.37,1.1,2.79,1.87,1.38.83,2.26,1.71,2.26,1.71h0,0s.37,0,.37,0c.28,0,.56,0,.84,0h1.71s0,0,0,0Z"/><path class="lw" d="M28.95,29.08c2.15-.65,4.31-1.31,6.46-1.97.11-.03.2-.15.43-.33-.62-.55-1.15-1.03-1.81-1.62l-17.17,4.46,4.26,2,7.66-2.49.18-.06h0Z"/><path class="lw" d="M10.55,26.66l3.82,1.79,13.03-2.63c1.99-.36,3.99-.72,5.99-1.08-.47-.83-.88-1.57-1.38-2.47l-21.97,3.46.5.93h0Z"/><path class="lw" d="M7.36,20.78l1.84,3.39,17.36-1.84.08-.06c1.73-.17,3.47-.35,5.23-.53-.45-.91-.83-1.66-1.22-2.46l-23.27,1.5h0Z"/><path class="lw" d="M29.98,17.73l-.04-.08-.1-.22c-.24-.52-.48-1.01-.73-1.55l-24.72-.57,1.86,3.42,24.22.03-.4-.85c-.03-.06-.06-.13-.09-.19h0Z"/><path class="lw" d="M28.88,15.38l-.82-1.68c-.11-.22-.21-.44-.32-.66l-.11-.22c-.03-.06-.06-.12-.09-.18L1.42,9.83l1.79,3.29,25.68,2.25Z"/><path class="lw" d="M22.34,11.14c1.62.29,3.25.58,4.89.88-.42-.89-.74-1.57-1.11-2.35L4.04,4.8l-1.71,3.02,20.01,3.33h0Z"/><path class="lw" d="M10.32,4.14l-.04-.03,7.84,2.34c2.45.75,4.91,1.51,7.39,2.28-.13-.86-.44-1.45-1.17-1.84-1.77-.95-3.53-1.92-5.3-2.87-.34-.18-.71-.31-1.08-.43C14.21,2.44,10.45,1.29,6.68.15l-1.4,2.47,1.78.53,3.27.99h0Z"/><path class="lw" d="M25.79,6.66c.23.21.83.84,1.33,1.93l.33.71,3.16,1.12-2.89-.54,1.11,2.36,2.88.58-2.66-.11,1.32,2.81,2.71.4-2.51.03,1.39,2.96,2.26.07-2.1.27,1.18,2.51,1.94.06-1.81.23.82,1.75.83.69.91-.12-.78.23,1.97,2.11-.03.21.11.1c.74-.55,1.25-.96,1.35-1.04l.84-.73c.06-.09.14-.17.23-.24l.35-.28s.02-.01.03-.02c1.3-.95,2.85-1.28,4.5-.96l-8.72-10.54,1.34-5.12-6.95-2.82h-6.55c.37.2.74.4,1.1.59.42.22.74.5.98.79h0Z"/><path class="lw" d="M33.68,29.58c1.03-.69,2.07-1.43,2.93-2.06l-.25-.22-13.73,5.01,3.74,1.73,7.31-4.46h0Z"/><path class="lw" d="M16.23,36.86c1.27.69,2.74,1.01,3.93,1.17.07-.26.15-.52.24-.79.06-.17.16-.32.28-.45l.65-.65c.06-.06.14-.12.21-.17.14-.09.3-.17.47-.25.08-.05.16-.09.25-.13h.04c-1.22-.04-2.05-.09-2.05-.09-.54,0-1.15-.1-1.81-.29-.09-.02-.14-.03-.14-.03h0c-.29-.09-.59-.19-.9-.31-1.07-.41-1.85-.8-1.85-.81l-.05-.02-7.67-3.65c.07-1.77-1.16-2.85-1.16-2.85l-.17.99c-.05-.11-.1-.22-.14-.34,0,0-.22-1.61-1.17-2.09l-.16.62s-.46-1.21-1.76-1.27c0,0-.58-.18-1.49.29,0,0,.85.26.96.81l-.69-.15s-.04,1.01,1.49,1.74l-.82.07-.27.03s.82,2.67,3.35,2.67l-1.32.64c.56.35,1.18.34,1.18.34l1.17-.08,7.69,4.21c.16.09.8.46,1.7.85h0Z"/><path class="lw" d="M62.67,26.56c-.2-.44-.52-.86-.83-1.22-.27-.32-.68-.54-.9-.91.47.06,1.1.19,1.33.69.15-.58.11-1.26-.06-1.84-.29-1-1.18-1.26-1.86-1.85-.3-.26-.65-.6-1.01-.84l-.37-.33c-.37-.28-.74-.59-1.07-.9-.81-1.06-1.63-2.1-2.44-3.16-.22-.28-.39-.61-.53-.83-.33-3.35.44-6.24.97-9.2.12-.67.15-1.3-.09-2.03h-9.21l3.64,4.18c-.02.06-.04.11-.06.17-.02-.02-.04-.03-.06-.05l.13-.12-5.27-4.04s-.56.24-.68,1.3c-.22,1.53-.45,3.12-.7,4.84,1.89.19,3.69.37,5.5.55,0,.07.02.14.03.22-1.89.3-3.78.61-5.76.92-.3,2.07-.6,4.14-.94,6.48,1.96-1.31,3.74-2.5,5.52-3.69.02.04.05.09.07.13-.15.18-.3.37-.46.53-1.53,1.5-3.05,3.03-4.62,4.47-.03.03-.06.06-.09.08.75.92,1.56,1.87,2.41,2.8l1.12,1.31,3.41-5.99.12.08-2.35,6.76,3.56,3.76.66-5.79.14.02.51,7.83s.01.07.02.1l.08.5s-.02.03-.03.04l.93,5.83c.06.39.06.78,0,1.17l-.64,4.44c1.01-1.23,1.75-2.95,1.83-4.38.59,1.28.35,2.11.1,3.35,1.14-1.26,2.33-2.41,2.25-4.36-.02-.44-.18-.98-.33-1.53.52.45.96,1.16,1.24,1.76.22.48.24.83.2,1.17,1.13-1.86.71-5.01.71-5.01l1.35,1.35c1.27-1.83.42-4.38.42-4.38.45.91,1.8.51,1.8.51-.83-.93-.98-3.34-1-3.57.21.07.4.15.59.27.25.17.8.66.77.99.29-.83.37-1.73-.04-2.63h0ZM52.79,8.31c-.59.7-.24,1.59-.18,2.51-.35-.51-.69-1.01-1.05-1.53.33-.66.62-1.21.9-1.77.42.07.79.14,1.16.2,0,.06.01.12.02.18-.28.14-.57.28-.85.42h0Z"/><g><path class="lw" d="M.09,79.65l2.1-2.16,3.2-3.04,3.55-3.38,2.49-2.37,1.69-1.61,17.98.02c3.15,0,3.87.82,2.16,2.44l-3.09,2.94c-1.7,1.62-4.13,2.43-7.28,2.43h-2.77s4.7,4.75,4.7,4.75h-7s-3.47-4.76-3.47-4.76h-4.64s-4.99,4.74-4.99,4.74H0M24.9,68.98h-8.73s-4.26,3.51-4.26,3.51h9.29c.76.01,1.34-.04,1.74-.13.56-.15,1.07-.43,1.51-.86l1.6-1.52c.45-.42-.84-.98-1.14-1.01-1.43-.15.76,0,0,0Z"/><path class="lw" d="M47.42,69.5v10.53h-3.98v-10.53l-7.87.04v-2.32h19.75v2.25l-7.9.03Z"/><path class="lw" d="M67.94,69.35v10.9h-3.79v-10.9l-3.44.02h-1.83s-2.22-2.38-2.22-2.38h18.8l-3.7,2.35h0l-3.82.02Z"/></g></svg>
    <div>
      <div class="hdr-logo">RAF RTT</div>
      <div style="font-size:.55em;opacity:.85;letter-spacing:1px;font-weight:600">TRAINING SYSTEM</div>
    </div>
  </div>
  <div style="text-align:right">
    <span class="hdr-badge" id="onlineBadge">0 targets</span>
    <div style="font-size:.45em;opacity:.6;margin-top:2px">&#xAE; Patent Pending</div>
  </div>
</div>
<!-- Zone Bar (visible when multi-zone) -->
<div id="zoneBar" class="zone-bar" style="display:none"></div>

<!-- SCREEN: HOME -->
<div id="scrHome" class="scr active">
<div class="pbar">
  <div class="nfc-dot" id="nfcDot"></div>
  <input type="text" id="pname" placeholder="Naam speler..." maxlength="15">
  <button id="muteBtn" onclick="toggleMute()" style="width:48px;height:48px;border-radius:10px;border:2px solid #ddd;background:var(--w);font-size:1.2em;cursor:pointer;flex-shrink:0">&#x1F50A;</button>
</div>
<div class="cat-tabs" id="catTabs">
  <button class="cat-tab active" onclick="filterCat('all')">Alles</button>
  <button class="cat-tab" onclick="filterCat('solo')">Solo</button>
  <button class="cat-tab" onclick="filterCat('multi')">Multiplayer</button>
  <button class="cat-tab" onclick="filterCat('team')">Team</button>
</div>
<div class="mgrid" id="modeGrid"></div>
</div>

<!-- SCREEN: SETTINGS (overlay) -->
<div class="overlay" id="scrSet">
<div class="overlay-panel">
  <div class="ov-hdr">
    <button class="ov-back" onclick="closeSettings()">&#x2190;</button>
    <span class="ov-title" id="setTitle">Instellingen</span>
  </div>
  <div id="setBody"></div>
  <div class="field" style="margin-top:12px">
    <label>Aftellen: <span id="cdVal" style="color:var(--r)">3s</span></label>
    <div class="slider-f"><input type="range" id="cdSlider" min="0" max="10" value="3" oninput="E('cdVal').textContent=this.value+'s'"></div>
  </div>
  <button class="start-btn" onclick="doStart()">START GAME &#9654;</button>
</div>
</div>

<!-- SCREEN: GAME VIEW -->
<div id="scrGame" class="scr">
<div id="gPlayerName" style="text-align:center;font-size:1em;font-weight:700;padding:8px 0 2px;color:var(--b)"></div>
<div class="game-state"><span id="gStateBadge" class="state-badge sb-run">ACTIEF</span></div>
<div class="game-timer" id="gTimer">0.000</div>
<div class="game-score" id="gScore">0</div>
<div class="game-stats">
  <span>&#10004; <span id="gHits">0</span></span>
  <span style="color:var(--r)">&#10008; <span id="gMisses">0</span></span>
</div>
<div class="game-remaining" id="gRemain"></div>
<div id="gNsBadge" style="display:none;text-align:center;font-size:.8em;color:#0a0;font-weight:700;margin:-4px 0 4px">&#x26A0;&#xFE0F; No-Shoot <span id="gNsPct">0</span></div>
<div id="gTurnBadge" style="display:none;text-align:center;font-size:.9em;font-weight:700;color:var(--r);margin:-4px 0 4px">&#x1F3AF; <span id="gTurnName"></span> (<span id="gTurnNum"></span>)</div>
<div class="game-tgrid" id="gTgrid"></div>
<div class="pcards" id="gPcards"></div>
<div class="gctrl">
  <button class="btn-pause" onclick="cmd('pause')">&#9208; PAUZE</button>
  <button class="btn-stop" onclick="confirmStop()">&#9209; STOP</button>
</div>
</div>

<!-- SCREEN: RESULTS -->
<div id="scrRes" class="scr">
<div class="res-header"><h2 id="rTitle">AFGELOPEN</h2></div>
<div class="res-bigscore" id="rScore">0</div>
<div class="res-stats">
  <div><div class="rs-val" id="rHits">0</div><div class="rs-lbl">Raak</div></div>
  <div><div class="rs-val" id="rMisses">0</div><div class="rs-lbl">Mis</div></div>
  <div><div class="rs-val" id="rTime">00:00</div><div class="rs-lbl">Tijd</div></div>
</div>
<div id="rPcards" class="pcards"></div>
<div class="save-row">
  <input type="text" id="rName" placeholder="Naam..." maxlength="15">
  <button onclick="saveScore()">Opslaan</button>
</div>
<div class="res-btns">
  <button class="btn-again" onclick="doStart()">Opnieuw &#9654;</button>
  <button class="btn-other" onclick="goHome()">Andere Modus</button>
</div>
<div class="extra-card">
  <h3>&#x1F3C6; Leaderboard</h3>
  <table class="lb-table"><thead><tr><th>#</th><th>Naam</th><th>Modus</th><th>Raak</th><th>Tijd</th></tr></thead><tbody id="lb"></tbody></table>
</div>
</div>

<!-- SCREEN: TURN WAIT -->
<div id="scrTurn" class="scr">
<div class="res-header"><h2 id="twTitle">VOLGENDE BEURT</h2></div>
<div id="twPrevResult" style="text-align:center;margin:12px 0"></div>
<div id="twNextInfo" style="text-align:center;font-size:1.3em;font-weight:700;margin:16px 0"></div>
<div id="twCountdown" style="text-align:center;font-size:3em;font-weight:700;color:var(--r);margin:12px 0"></div>
<div id="twRanking" style="margin:12px 16px"></div>
<div style="display:flex;gap:8px;margin:16px;flex-wrap:wrap">
  <button class="btn-again" onclick="cmd('nextturn')" id="twStartBtn" style="flex:1">&#9654; Start Beurt</button>
  <button class="btn-stop" onclick="confirmStop()" style="flex:1">&#9209; Annuleer</button>
</div>
</div>

<!-- SCREEN: EXTRA -->
<div id="scrExtra" class="scr">
<div class="extra-card">
  <h3>&#x1F3AF; Handmatig Activeren</h3>
  <div id="manualBtns"></div>
  <button class="alloff-btn" onclick="cmd('activate?t=0')">ALLE UIT</button>
</div>
<div class="extra-card">
  <h3>Piezo Gevoeligheid</h3>
  <div class="slider-f"><input type="range" id="thSlider" min="10" max="500" value="100" step="5"></div>
  <div class="slider-val">Threshold: <span id="thVal">100</span></div>
  <div style="text-align:center;font-size:.75em;color:#888">&#x2190; Gevoeliger | Minder gevoelig &#x2192;</div>
</div>
<div class="extra-card">
  <h3>&#x1F3C6; TOP 12</h3>
  <table class="lb-table"><thead><tr><th>#</th><th>Naam</th><th>Modus</th><th>Raak</th><th>Tijd</th></tr></thead><tbody id="lbExtra"></tbody></table>
  <div style="display:flex;gap:8px;margin-top:10px">
    <button onclick="emailScores()" style="flex:1;padding:10px;background:var(--r);color:var(--w);border:none;border-radius:10px;font-weight:700;cursor:pointer;min-height:48px">&#x2709; Email Scores</button>
    <button onclick="if(confirm('Leaderboard wissen?'))cmd('resetscores')" style="flex:1;padding:10px;background:var(--w);color:var(--b);border:2px solid #ddd;border-radius:10px;font-weight:700;cursor:pointer;min-height:48px">Reset</button>
  </div>
</div>
<div class="extra-card">
  <h3>&#x1F4F1; NFC Tags</h3>
  <a href="/nfc" class="link-btn">NFC Tag Setup</a>
</div>
<div class="extra-card">
  <h3>&#x1F527; Firmware</h3>
  <a href="/update" class="link-btn" style="margin-bottom:8px">Master Updaten</a>
  <div style="font-weight:700;font-size:.85em;margin:8px 0 4px">Targets:</div>
  <div id="otaBtns"></div>
</div>
<div id="fwInfo" style="text-align:center;font-size:.7em;color:#999;padding:8px 0"></div>
</div>

<!-- SCREEN: ZONES -->
<div id="scrZones" class="scr">
<div class="extra-card">
  <h3>Zone Configuratie</h3>
  <div style="font-size:.85em;color:#666;margin-bottom:8px">Verdeel targets over zones voor meerdere telefoons.</div>
  <div class="zone-nums" id="zoneNums">
    <button class="on" onclick="setNumZones(1)">1</button>
    <button onclick="setNumZones(2)">2</button>
    <button onclick="setNumZones(3)">3</button>
    <button onclick="setNumZones(4)">4</button>
  </div>
  <button onclick="autoSplitZones()" style="width:100%;padding:10px;background:#f0f0f0;border:2px solid #ddd;border-radius:10px;font-weight:700;cursor:pointer;margin:8px 0;min-height:48px">Auto-verdeel</button>
  <div id="zoneAssign"></div>
  <div class="zone-summary" id="zoneSummary"></div>
</div>
</div>

<!-- Bottom Nav -->
<nav class="bnav" id="bnav">
  <button class="active" onclick="navTo('scrHome')"><span class="nav-icon">&#x1F3AE;</span>Spel</button>
  <button onclick="navTo('scrZones')"><span class="nav-icon">&#x1F4CD;</span>Zones</button>
  <button onclick="navTo('scrExtra')"><span class="nav-icon">&#x2699;</span>Extra</button>
</nav>

<!-- Confirm dialog -->
<div class="confirm-overlay" id="confirmDlg">
<div class="confirm-box">
  <p>Spel stoppen?</p>
  <div class="cb-btns">
    <button onclick="closeConfirm()" style="background:var(--w);color:var(--b);border:2px solid #ddd">Annuleer</button>
    <button onclick="doStop()" style="background:var(--r);color:var(--w);border:none">Stoppen</button>
  </div>
</div>
</div>

<!-- Toast -->
<div class="toast" id="toast"></div>
</div>

<script>
// ========== UTILITIES ==========
function E(id){return document.getElementById(id)}
function fmtMs(ms){return (ms/1000).toFixed(3)}
function fmtSec(ms){return (ms/1000).toFixed(3)}
function toast(msg){var t=E('toast');t.textContent=msg;t.classList.add('show');clearTimeout(t._to);t._to=setTimeout(function(){t.classList.remove('show')},2000)}
function cmd(p){fetch('/'+p+(p.indexOf('?')>=0?'&':'?')+'z='+curZone).catch(function(){})}

// ========== FASTCLICK ==========
(function(){var tX,tY,tT;
document.addEventListener('touchstart',function(e){var t=e.changedTouches[0];tX=t.pageX;tY=t.pageY;tT=Date.now()},{passive:true});
document.addEventListener('touchend',function(e){if(Date.now()-tT>300)return;var t=e.changedTouches[0];if(Math.abs(t.pageX-tX)>10||Math.abs(t.pageY-tY)>10)return;var el=document.elementFromPoint(t.clientX,t.clientY);if(!el)return;if(el.tagName==='INPUT'&&el.type==='range')return;if(el.tagName==='BUTTON'||el.tagName==='INPUT'||el.tagName==='SELECT'||el.tagName==='SUMMARY'||el.onclick){e.preventDefault();el.click();}},{passive:false});
})();

// ========== NFC TAG ==========
var nfcPlayer=null;
(function(){var p=new URLSearchParams(window.location.search);if(p.has('player')){nfcPlayer=decodeURIComponent(p.get('player')).substring(0,15);window.history.replaceState({},'','/')}})();
window.addEventListener('DOMContentLoaded',function(){
  if(nfcPlayer){var pn=E('pname');if(pn)pn.value=nfcPlayer;E('nfcDot').classList.add('on');toast('NFC: '+nfcPlayer+' ingeladen');trnAddFromNfc(nfcPlayer)}
  buildModeGrid();
  var thS=E('thSlider');thS.addEventListener('input',function(){E('thVal').textContent=this.value});
  thS.addEventListener('change',function(){cmd('threshold?v='+this.value)});
  loadSettings();
});

// ========== AUDIO ==========
var audioCtx=null,audioMuted=false,audioUnlocked=false,audioStale=false;
function initAudio(){
  if(audioCtx&&audioCtx.state==='closed')audioCtx=null;
  if(!audioCtx){try{audioCtx=new(window.AudioContext||window.webkitAudioContext)({sampleRate:44100})}catch(e){return false}}
  if(audioCtx.state==='suspended')try{audioCtx.resume()}catch(e){}
  if(!audioUnlocked&&audioCtx.state==='running'){try{var b=audioCtx.createBuffer(1,1,44100);var s=audioCtx.createBufferSource();s.buffer=b;s.connect(audioCtx.destination);s.start(0);audioUnlocked=true}catch(e){}}
  return audioCtx&&audioCtx.state==='running';
}
function resumeAudio(){if(!audioCtx)return;if(audioCtx.state==='suspended')try{audioCtx.resume()}catch(e){};if(audioCtx.state==='running'&&audioUnlocked){try{var b=audioCtx.createBuffer(1,1,44100);var s=audioCtx.createBufferSource();s.buffer=b;s.connect(audioCtx.destination);s.start(0)}catch(e){}}}
document.addEventListener('touchstart',function(){initAudio();resumeAudio()},{passive:true});
document.addEventListener('touchend',resumeAudio,{passive:true});
document.addEventListener('click',function(){initAudio();resumeAudio()});
document.addEventListener('visibilitychange',function(){if(document.hidden)audioStale=true;else{setTimeout(resumeAudio,100);setTimeout(resumeAudio,500)}});
function toggleMute(){audioMuted=!audioMuted;E('muteBtn').innerHTML=audioMuted?'&#x1F507;':'&#x1F50A;'}
function beep(freq,dur,vol,type){
  if(audioMuted||!audioCtx||audioCtx.state!=='running'){initAudio();if(!audioCtx||audioCtx.state!=='running')return}
  try{var now=audioCtx.currentTime;var o=audioCtx.createOscillator();var g=audioCtx.createGain();o.connect(g);g.connect(audioCtx.destination);o.frequency.value=freq;o.type=type||'sine';g.gain.setValueAtTime(vol||.3,now);g.gain.setValueAtTime(vol||.3,now+dur/1000-.05);g.gain.exponentialRampToValueAtTime(.001,now+dur/1000);o.start(now);o.stop(now+dur/1000+.05);o.onended=function(){try{o.disconnect();g.disconnect()}catch(e){}}}catch(e){}
}
function sndCountdown(){beep(1000,350,.7)}
function sndStart(){beep(2200,1500,.8)}
function sndEnd(){beep(800,2000,.5)}
function sndHit(){beep(1200,80,.3);setTimeout(function(){beep(1600,60,.2)},80)}function sndMiss(){beep(200,250,.35)}
function sndWin(){beep(1000,120,.3);setTimeout(function(){beep(1300,120,.3)},130);setTimeout(function(){beep(1600,250,.4)},260)}
var lastCd=-1,lastState=-1,prevHits=0,prevMisses=0;
function audioKeepAlive(){
  if(!audioCtx||audioCtx.state!=='running')return;
  try{var b=audioCtx.createBuffer(1,1,44100);var s=audioCtx.createBufferSource();s.buffer=b;s.connect(audioCtx.destination);s.start(0)}catch(e){}
}
function checkAudio(d){
  if(audioStale){prevHits=d.hits;prevMisses=d.misses||0;lastState=d.state;lastCd=-1;audioStale=false;return}
  if(d.state==1){audioKeepAlive();if(d.countdown!==lastCd&&d.countdown>0)sndCountdown()}
  lastCd=d.state==1?d.countdown:-1;
  if(d.state==2&&(lastState==1||lastState==0)){resumeAudio();sndStart();prevHits=d.hits;prevMisses=d.misses||0}
  if(d.state==4&&lastState==2)sndEnd();
  if(d.state==2){if(d.hits>prevHits)sndHit();if((d.misses||0)>prevMisses)sndMiss();prevHits=d.hits;prevMisses=d.misses||0}
  if(d.state==0){prevHits=0;prevMisses=0}
  lastState=d.state;
}

// ========== MODE DEFINITIONS ==========
var MODES=[
  {id:0,name:'Free Play',icon:'\u{1F3AF}',desc:'Vrij schieten zonder regels',cat:'solo'},
  {id:1,name:'Shoot/No-Shoot',icon:'\u{1F6AB}',desc:'Raak groen, vermijd rood',cat:'solo'},
  {id:2,name:'Sequence',icon:'\u{1F522}',desc:'Schiet targets in volgorde',cat:'solo'},
  {id:3,name:'Random',icon:'\u{1F3B2}',desc:'Willekeurige targets op tijd',cat:'solo'},
  {id:4,name:'Manual',icon:'\u270B',desc:'Handmatige target controle',cat:'solo'},
  {id:5,name:'Memory',icon:'\u{1F9E0}',desc:'Onthoud en herhaal de volgorde',cat:'solo'},
  {id:6,name:'Random Color',icon:'\u{1F3A8}',desc:'Schiet alleen jouw kleur',cat:'multi'},
  {id:7,name:'Reaction',icon:'\u26A1',desc:'Wie reageert het snelst?',cat:'multi'},
  {id:8,name:'Parcours',icon:'\u{1F3C3}',desc:'Volg de route, op tijd',cat:'solo'},
  {id:9,name:'Fast Track',icon:'\u{1F3C1}',desc:'Wie schiet het snelst alle targets?',cat:'multi'},
  {id:10,name:'Tournament',icon:'\u2694\uFE0F',desc:'Team vs team competitie',cat:'team'},
  {id:11,name:'Capture Points',icon:'\u{1F3F0}',desc:'Verover vijandige targets',cat:'team'}
];
var curMode=0,curCat='all';

function buildModeGrid(){
  var g=E('modeGrid');g.innerHTML='';
  MODES.forEach(function(m){
    if(curCat!=='all'&&m.cat!==curCat)return;
    var d=document.createElement('div');d.className='mcard'+(m.id===curMode?' sel':'');d.dataset.mode=m.id;
    d.innerHTML='<div class="mcard-icon">'+m.icon+'</div><div class="mcard-name">'+m.name+'</div><div class="mcard-desc">'+m.desc+'</div><button class="mcard-play" onclick="event.stopPropagation();quickStart('+m.id+')">&#9654;</button>';
    d.onclick=function(){curMode=m.id;openSettings(m.id)};
    g.appendChild(d);
  });
}
function filterCat(c){
  curCat=c;
  var tabs=E('catTabs').children;
  var cats=['all','solo','multi','team'];
  for(var i=0;i<tabs.length;i++)tabs[i].className='cat-tab'+(cats[i]===c?' active':'');
  buildModeGrid();
}

// ========== SCREEN MANAGEMENT ==========
var curScreen='scrHome',wasGameScreen=false;
function showScreen(name){
  if(curScreen===name)return;
  ['scrHome','scrGame','scrRes','scrTurn','scrExtra','scrZones'].forEach(function(s){
    var el=E(s);if(el)el.classList.toggle('active',s===name)
  });
  curScreen=name;
  var inGame=name==='scrGame';
  E('bnav').classList.toggle('hidden',inGame);
}
function navTo(scr){
  showScreen(scr);
  var btns=E('bnav').children;
  var screens=['scrHome','scrZones','scrExtra'];
  for(var i=0;i<btns.length;i++)btns[i].className=screens[i]===scr?'active':'';
}
function goHome(){closeSettings();showScreen('scrHome');navTo('scrHome');wasGameScreen=false;E('rTitle').textContent='AFGELOPEN'}

function openSettings(modeId){
  curMode=modeId;cmd('setmode?m='+modeId);
  var m=MODES[modeId];
  E('setTitle').textContent=m.icon+' '+m.name;
  E('setBody').innerHTML=buildSettingsHTML(modeId);
  E('scrSet').classList.add('open');
  loadModeSettings(modeId);
  buildModeGrid();
}
function closeSettings(){E('scrSet').classList.remove('open')}
function quickStart(modeId){curMode=modeId;cmd('setmode?m='+modeId);doStart()}

// ========== SETTINGS BUILDER ==========
function buildSettingsHTML(m){
  var h='';
  if(m===0){
    h+='<div class="field"><label>Speeltijd (sec, 0=onbeperkt)</label><input type="number" id="fpTime" value="0" min="0" max="600" step="10"></div>';
  }else if(m===1){
    h+='<label class="chk"><input type="checkbox" id="darkMode"> Dark Mode (lichten uit)</label>';
    h+='<label class="chk"><input type="checkbox" id="offMode" checked> Off Mode (target uit bij hit)</label>';
    h+='<div class="field"><label>Speeltijd (sec)</label><input type="number" id="snsTime" value="60" min="10" max="300" step="5"></div>';
    h+='<div style="font-weight:700;color:var(--r);margin:8px 0 4px">Target Configuratie:</div>';
    h+='<div class="tog-row"><button class="tog-btn on" id="snsModeMan" onclick="setSnsMode(0)">Handmatig</button><button class="tog-btn" id="snsModeRnd" onclick="setSnsMode(1)">Random</button></div>';
    h+='<div id="snsManual"><div id="snsTargets"></div></div>';
    h+='<div id="snsRandom" style="display:none"><div class="field"><label>Aantal No-Shoot targets</label><input type="number" id="snsNoShootCount" value="1" min="0" max="20"></div><div style="font-size:.8em;color:#888">Rest wordt automatisch SHOOT.</div></div>';
  }else if(m===2){
    h+='<div class="field"><label>Aantal targets</label><input type="number" id="seqR" value="10" min="1" max="50"></div>';
    h+=sliderField('seqTPT','Target uit na (miss)','0','15000','3000','500','seqTPTVal','3.0',true);
    h+=sliderField('seqDN','Vertraging next target','0','10000','0','500','seqDNVal','0.0',true);
    h+=sliderField('seqHO','Hits voor uit','1','10','1','1','seqHOVal','1',false);
    h+=nsFieldBlock();
  }else if(m===3){
    h+='<div class="field"><label>Interval MIN (sec)</label><input type="number" id="randMin" value="1" min="1" max="30" step="0.5"></div>';
    h+='<div class="field"><label>Interval MAX (sec)</label><input type="number" id="randMax" value="5" min="1" max="30" step="0.5"></div>';
    h+='<div class="field"><label>Aantal targets</label><input type="number" id="randR" value="15" min="1" max="50"></div>';
    h+=nsFieldBlock();
  }else if(m===4){
    h+='<div style="color:#666;font-style:italic;padding:12px 0">Gebruik de kleurknoppen in Extra om targets handmatig te activeren.</div>';
  }else if(m===5){
    h+=sliderField('memLen','Aantal te onthouden','2','20','5','1','memLenVal','5',false);
    h+=sliderField('memDisp','Weergavetijd per doel','500','3000','1000','100','memDispVal','1.0',true);
    h+=sliderField('memWrong','Max fouten','1','10','3','1','memWrongVal','3',false);
  }else if(m===6){
    h+=playerPicker('rc',2,[2,3,4]);
    h+=playerNames('rc',4,['#00f','#f00','#80f','#fff']);
    h+=sliderField('rcRoundsIn','Doelen','5','60','20','1','rcRoundsVal','20',false);
    h+=sliderField('rcDispIn','Doeltijd','1000','8000','3000','500','rcDispVal','3.0',true);
    h+=sliderField('rcPauseIn','Pauze tussen targets','500','3000','1500','100','rcPauseVal','1.5',true);
  }else if(m===7){
    h+=playerPicker('rx',2,[1,2,3,4]);
    h+=playerNames('rx',4,['#00f','#f00','#80f','#fff']);
    h+=sliderField('rxRoundsIn','Rondes','1','10','5','1','rxRoundsVal','5',false);
    h+=sliderField('rxDelMin','Delay MIN','500','5000','1000','250','rxDelMinVal','1.0',true);
    h+=sliderField('rxDelMax','Delay MAX','1000','8000','3000','250','rxDelMaxVal','3.0',true);
    h+='<div class="tog-row"><button class="tog-btn on" id="rxModeRnd" onclick="setRxFixed(false)">Random</button><button class="tog-btn" id="rxModeFix" onclick="setRxFixed(true)">Vast</button></div>';
    h+='<div style="font-size:.8em;color:#888">Random = elke ronde nieuwe toewijzing.</div>';
    h+=nsFieldBlock();
  }else if(m===8){
    h+=sliderField('parDN','Vertraging next target','0','10000','0','500','parDNVal','0.0',true);
    h+=sliderField('parAO','Target uit na (auto-off, 0=nooit)','0','15000','0','500','parAOVal','0.0',true);
    h+=sliderField('parHO','Hits voor uit','1','10','1','1','parHOVal','1',false);
    h+='<div class="tog-row"><button class="tog-btn on" id="parModeSeq" onclick="setParMode(false)">Handmatig</button><button class="tog-btn" id="parModeRnd" onclick="setParMode(true)">Random</button></div>';
    h+='<div id="parManual"><div style="font-size:.8em;color:#888;margin-bottom:6px">Klik op targets om route te maken.</div><div class="route-avail" id="parAvail"></div><div style="font-size:.8em;font-weight:700;color:var(--r);margin:6px 0">Route (<span id="parRouteCount">0</span> targets):</div><div class="route-list" id="parRoute"></div><div class="route-actions"><button onclick="parClearRoute()">Wis</button><button onclick="parUndoRoute()">Undo</button><button onclick="parAllTargets()">Alle</button></div></div>';
    h+='<div id="parRandInfo" style="display:none;font-size:.8em;color:#888;padding:8px 0">Alle online targets in willekeurige volgorde.</div>';
    h+=nsFieldBlock();
  }else if(m===9){
    h+=playerPicker('ft',2,[2,3,4]);
    h+=playerNames('ft',4,['#00f','#f00','#80f','#fff']);
    h+='<div class="field"><label>Targets per speler: <span id="ftTppVal" style="color:var(--r);font-weight:700">10</span></label><div class="slider-f"><input type="range" id="ftTpp" min="3" max="30" value="10" oninput="E(\'ftTppVal\').textContent=this.value"></div></div>';
    h+='<div class="tog-row"><button class="tog-btn on" id="ftModeRnd" onclick="ftSetFixed(false)">Random</button><button class="tog-btn" id="ftModeFixed" onclick="ftSetFixed(true)">Vast</button></div>';
    h+=nsFieldBlock();
  }else if(m===10){
    h+='<div class="field"><label style="font-weight:700">Aantal zones:</label></div>';
    h+='<div class="tog-row"><button class="tog-btn on" id="tbN2" onclick="tbSetNum(2)">2</button><button class="tog-btn" id="tbN3" onclick="tbSetNum(3)">3</button><button class="tog-btn" id="tbN4" onclick="tbSetNum(4)">4</button></div>';
    h+='<div id="tbNames"></div>';
    h+='<div class="tog-row"><button class="tog-btn on" id="tbModeBattle" onclick="tbSetBattle(true)">&#x2694;&#xFE0F; Battle</button><button class="tog-btn" id="tbModeTijden" onclick="tbSetBattle(false)">&#x23F1; Tijden</button></div>';
    h+='<div id="tbModeDesc" style="font-size:.75em;color:#666;margin-bottom:8px">Battle: wie het eerst klaar is wint.</div>';
    h+='<div class="field"><label style="font-weight:700">Game Mode:</label></div>';
    h+='<div class="tog-row" style="flex-wrap:wrap" id="tbGlobalMode"><button class="tog-btn on" id="tbG0" onclick="tbSetGame(0)">Random</button><button class="tog-btn" id="tbG1" onclick="tbSetGame(1)">Sequence</button><button class="tog-btn" id="tbG2" onclick="tbSetGame(2)">SNS</button><button class="tog-btn" id="tbG3" onclick="tbSetGame(3)">Reaction</button><button class="tog-btn" id="tbG4" onclick="tbSetGame(4)">Parcours</button><button class="tog-btn" id="tbG5" onclick="tbSetGame(5)">Memory</button><button class="tog-btn" id="tbG6" onclick="tbSetGame(6)">Rand Clr</button><button class="tog-btn" id="tbG7" onclick="tbSetGame(7)">Fast Track</button></div>';
    h+='<div class="tog-row"><button class="tog-btn on" id="tbModeAll" onclick="tbSetPerTeam(false)">Alle zones</button><button class="tog-btn" id="tbModePer" onclick="tbSetPerTeam(true)">Per zone</button></div>';
    h+='<div id="tbPerTeamModes" style="display:none"></div>';
    h+='<div style="font-weight:700;font-size:.85em;margin:8px 0 4px">Targets toewijzen:</div><div id="tbAssign"></div><div id="tbLegend" style="display:flex;gap:10px;margin:8px 0;font-size:.75em;color:#666;flex-wrap:wrap"></div>';
    h+='<div id="tbGlobalRounds"><div class="field"><label>Rondes per zone: <span id="tbRoundsVal" style="color:var(--r);font-weight:700">10</span></label><div class="slider-f"><input type="range" id="tbRounds" min="3" max="30" value="10" oninput="E(\'tbRoundsVal\').textContent=this.value"></div></div></div>';
  }else if(m===11){
    h+='<div class="field"><label style="font-weight:700">Aantal teams:</label></div>';
    h+='<div class="tog-row"><button class="tog-btn on" id="cpN2" onclick="cpSetNum(2)">2</button><button class="tog-btn" id="cpN3" onclick="cpSetNum(3)">3</button><button class="tog-btn" id="cpN4" onclick="cpSetNum(4)">4</button></div>';
    h+='<div id="cpNames"></div>';
    h+=sliderField('cpHits','Hits om te veroveren','1','100','5','1','cpHitsVal','5',false);
    h+=sliderField('cpTime','Gametijd (min, 0=onbeperkt)','0','30','2','1','cpTimeVal','2',false);
    h+='<div id="cpPerWin"></div>';
    h+='<label class="chk" style="margin:8px 0"><input type="checkbox" id="cpRecap" checked onchange="E(\'cpLockRow\').style.display=this.checked?\'block\':\'none\'"> Heroveren toegestaan</label>';
    h+='<div id="cpLockRow"><div class="field"><label>Tijd tot definitief: <span id="cpLockVal" style="color:var(--r)">0 min</span> <span style="font-size:.75em;color:#888">(0=direct)</span></label><div class="slider-f"><input type="range" id="cpLockSlider" min="0" max="10" value="0" step="1" oninput="E(\'cpLockVal\').textContent=this.value+\' min\'"></div></div></div>';
    h+='<div style="font-weight:700;font-size:.85em;margin:8px 0 4px">Targets toewijzen:</div><div id="cpAssign"></div><div id="cpLegend" style="display:flex;gap:10px;margin:8px 0;font-size:.75em;color:#666;flex-wrap:wrap"></div>';
  }
  // TOERBEURT sectie — voor alle modes behalve tournament/capture
  if(m!==10&&m!==11){
    h+='<div style="border-top:2px solid #eee;margin-top:16px;padding-top:12px">';
    h+='<div style="font-weight:700;font-size:.95em;margin-bottom:8px">&#x1F504; Toerbeurt Modus</div>';
    h+='<label class="chk"><input type="checkbox" id="trnOn" onchange="trnToggle(this.checked)"> Spelers om de beurt</label>';
    h+='<div id="trnPanel" style="display:none;margin-top:8px">';
    h+='<div id="trnPlayers"></div>';
    h+='<div style="display:flex;gap:6px;margin:6px 0"><input type="text" id="trnNewName" placeholder="Naam toevoegen..." maxlength="19" style="flex:1;padding:8px;border:2px solid #ddd;border-radius:8px"><button onclick="trnAddPlayer()" style="padding:8px 14px;background:var(--r);color:var(--w);border:none;border-radius:8px;font-weight:700;cursor:pointer;min-height:40px">+</button></div>';
    h+='<div style="font-size:.75em;color:#888;margin-bottom:8px">Max 12 spelers. NFC tag scant automatisch naam.</div>';
    h+='<label class="chk"><input type="checkbox" id="trnAuto"> Auto-wissel tussen beurten</label>';
    h+='<div id="trnCdRow" style="display:none;margin:4px 0">'+sliderField('trnCd','Countdown','3','15','5','1','trnCdVal','5',false)+'</div>';
    h+='</div></div>';
  }
  return h;
}
function nsFieldBlock(){
  var h='<div style="border-top:1px solid #eee;margin-top:10px;padding-top:8px">';
  h+='<div style="font-weight:700;font-size:.85em;color:#0a0;margin-bottom:4px">&#x1F7E2; No-Shoot Targets</div>';
  h+='<div class="tog-row"><button class="tog-btn on" id="nsOff" onclick="nsSetMode(0)">Uit</button><button class="tog-btn" id="nsMPct" onclick="nsSetMode(1)">Kans %</button><button class="tog-btn" id="nsMCnt" onclick="nsSetMode(2)">Aantal</button></div>';
  h+='<div id="nsPctRow" style="display:none"><div class="field"><label>No-Shoot kans: <span id="nsPctVal" style="color:#0a0">0%</span></label><div class="slider-f"><input type="range" id="nsPct" min="0" max="50" value="0" step="5" oninput="E(\'nsPctVal\').textContent=this.value+\'%\'"></div></div></div>';
  h+='<div id="nsCntRow" style="display:none"><div class="field"><label>Aantal no-shoot targets: <span id="nsCntVal" style="color:#0a0;font-weight:700">0</span></label><div class="slider-f"><input type="range" id="nsCnt" min="0" max="10" value="0" step="1" oninput="E(\'nsCntVal\').textContent=this.value"></div></div></div>';
  h+='<div style="font-size:.75em;color:#888">Groen target = niet schieten! Straf: -3 punten.</div></div>';
  return h;
}
var nsMode=0;
function nsSetMode(m){nsMode=m;E('nsOff').classList.toggle('on',m===0);E('nsMPct').classList.toggle('on',m===1);E('nsMCnt').classList.toggle('on',m===2);E('nsPctRow').style.display=m===1?'block':'none';E('nsCntRow').style.display=m===2?'block':'none';if(m===0){var s=E('nsPct');if(s)s.value=0;E('nsPctVal').textContent='0%';var c=E('nsCnt');if(c)c.value=0;E('nsCntVal').textContent='0'}}
function sliderField(id,label,min,max,val,step,valId,valTxt,isMs){
  var oninp=isMs?"E('"+valId+"').textContent=(this.value/1000).toFixed(1)":"E('"+valId+"').textContent=this.value";
  return '<div class="field"><label>'+label+': <span id="'+valId+'" style="color:var(--r)">'+valTxt+'</span>'+(isMs?'s':'')+'</label><div class="slider-f"><input type="range" id="'+id+'" min="'+min+'" max="'+max+'" value="'+val+'" step="'+step+'" oninput="'+oninp+'"></div></div>';
}
function playerPicker(prefix,def,opts){
  var h='<div class="field"><label>Aantal spelers:</label></div><div class="tog-row">';
  opts.forEach(function(n){h+='<button class="tog-btn'+(n===def?' on':'')+'" id="'+prefix+'P'+n+'" onclick="'+prefix+'SetPlayers('+n+')">'+n+'</button>'});
  return h+'</div>';
}
function playerNames(prefix,max,colors){
  var h='<div id="'+prefix+'Names">';
  for(var i=1;i<=max;i++){
    var vis=i<=2?'flex':'none';
    h+='<div class="prow" id="'+prefix+'Row'+i+'" style="display:'+vis+'"><div class="pdot" style="background:'+colors[i-1]+'"></div><input type="text" id="'+prefix+'N'+i+'" value="Speler '+i+'" maxlength="15"></div>';
  }
  return h+'</div>';
}

// ========== STATE VARS ==========
var curZone=0,numZones=1,lastTargets=null,lastData=null;
var snsRandomMode=0,rxFixedMode=false,parRandomMode=false,parRouteIds=[];
var ftPlayerCount=2,ftFixedMode=false;
var rcPlayerCountUI=2,rxPlayerCountUI=2;
var tbGroups={},tbGameMode=0,tbBattleMode=true,tbNumTeams=2,tbPerTeam=false,tbTeamModes=[0,0,0,0],tbTeamRounds=[10,10,10,10];
var tbColors=['#00f','#f00','#80f','#fff'];
var tbGameNames=['Random','Sequence','SNS','Reaction','Parcours','Memory','Rand Colour','Fast Track'];

// ========== TOGGLE FUNCTIONS ==========
function setToggle(onId,offId,isOn){
  var on=E(onId),off=E(offId);
  if(on){on.classList.toggle('on',isOn)}if(off){off.classList.toggle('on',!isOn)}
}
function setSnsMode(m){snsRandomMode=m;setToggle('snsModeMan','snsModeRnd',!m);var mn=E('snsManual'),rn=E('snsRandom');if(mn)mn.style.display=m?'none':'block';if(rn)rn.style.display=m?'block':'none'}
function setRxFixed(f){rxFixedMode=f;setToggle('rxModeRnd','rxModeFix',!f)}
function setParMode(r){parRandomMode=r;setToggle('parModeSeq','parModeRnd',!r);var mn=E('parManual'),ri=E('parRandInfo');if(mn)mn.style.display=r?'none':'block';if(ri)ri.style.display=r?'block':'none'}
function ftSetFixed(f){ftFixedMode=f;setToggle('ftModeRnd','ftModeFixed',!f)}

function setPlayerCount(prefix,n,opts,showFrom){
  opts.forEach(function(v){var b=E(prefix+'P'+v);if(b)b.classList.toggle('on',v===n)});
  for(var i=1;i<=4;i++){var r=E(prefix+'Row'+i);if(r)r.style.display=i<=n?'flex':'none'}
}
function rcSetPlayers(n){rcPlayerCountUI=n;setPlayerCount('rc',n,[2,3,4])}
function rxSetPlayers(n){rxPlayerCountUI=n;setPlayerCount('rx',n,[1,2,3,4])}
function ftSetPlayers(n){ftPlayerCount=n;setPlayerCount('ft',n,[2,3,4])}

// ========== TOERBEURT SETTINGS ==========
var trnPlayerList=[];
function trnToggle(on){var p=E('trnPanel');if(p)p.style.display=on?'block':'none'}
function trnAddPlayer(){var inp=E('trnNewName');if(!inp)return;var n=inp.value.trim();if(!n||trnPlayerList.length>=12)return;trnPlayerList.push(n);inp.value='';trnRender()}
function trnRemove(i){trnPlayerList.splice(i,1);trnRender()}
function trnRender(){var el=E('trnPlayers');if(!el)return;var h='';trnPlayerList.forEach(function(n,i){h+='<div style="display:flex;align-items:center;gap:6px;margin:3px 0;padding:6px 8px;background:#f8f8f8;border-radius:8px"><span style="font-weight:700;color:var(--r);min-width:20px">'+(i+1)+'.</span><span style="flex:1">'+n+'</span><button onclick="trnRemove('+i+')" style="background:none;border:none;color:#999;font-size:1.1em;cursor:pointer;padding:4px">&#x2715;</button></div>'});el.innerHTML=h;
  var ae=E('trnAuto');if(ae)ae.onchange=function(){var cr=E('trnCdRow');if(cr)cr.style.display=this.checked?'block':'none'};
}
function trnAddFromNfc(name){if(trnPlayerList.length>=12)return;trnPlayerList.push(name);trnRender()}

// ========== PARCOURS ROUTE ==========
function parBuildAvail(tgts){var c=E('parAvail');if(!c||!tgts)return;c.innerHTML='';tgts.forEach(function(t){if(!t.online)return;var b=document.createElement('button');b.textContent='T'+t.id;b.onclick=function(){parAddTarget(t.id)};c.appendChild(b)})}
function parAddTarget(id){parRouteIds.push(id);parRenderRoute()}
function parClearRoute(){parRouteIds=[];parRenderRoute()}
function parUndoRoute(){parRouteIds.pop();parRenderRoute()}
function parAllTargets(){parRouteIds=[];if(lastTargets)lastTargets.forEach(function(t){if(t.online)parRouteIds.push(t.id)});parRenderRoute()}
function parRenderRoute(){var c=E('parRoute');if(!c)return;c.innerHTML='';var cnt=E('parRouteCount');if(cnt)cnt.textContent=parRouteIds.length;parRouteIds.forEach(function(id,i){var s=document.createElement('span');s.className='route-tag';s.innerHTML=(i+1)+'. T'+id+' <span class="rx" onclick="parRemoveAt('+i+')">&#x2715;</span>';c.appendChild(s);if(i<parRouteIds.length-1){var a=document.createElement('span');a.textContent='\u2192';a.style.cssText='color:#999;align-self:center;font-size:.85em';c.appendChild(a)}})}
function parRemoveAt(i){parRouteIds.splice(i,1);parRenderRoute()}

// ========== TOURNAMENT ==========
function tbSetNum(n){
  tbNumTeams=n;numZones=n;
  for(var i=2;i<=4;i++){var b=E('tbN'+i);if(b)b.classList.toggle('on',i===n)}
  tbBuildNames();
  if(lastTargets){var onl=[];lastTargets.forEach(function(t){if(t.online)onl.push(t.id)});onl.sort(function(a,b){return a-b});tbGroups={};for(var i=0;i<onl.length;i++)tbGroups[onl[i]]=Math.floor(i*n/onl.length)}
  tbRenderAssign();if(tbPerTeam)tbRenderPerTeamModes();
  fetch('/zones?auto=1&n='+n).then(function(r){return r.json()}).then(function(d){renderZoneTabs(d);if(d.zones){tbGroups={};for(var zi=0;zi<d.numZones;zi++)if(Array.isArray(d.zones[zi].tgts))d.zones[zi].tgts.forEach(function(tid){tbGroups[tid]=zi});tbRenderAssign()}}).catch(function(){});
}
function tbBuildNames(){var el=E('tbNames');if(!el)return;var defs=['Team A','Team B','Team C','Team D'];var h='';for(var i=0;i<tbNumTeams;i++){h+='<div class="prow"><div class="pdot" style="background:'+tbColors[i]+'"></div><input id="tbNm'+i+'" type="text" value="'+defs[i]+'" maxlength="15"></div>'}el.innerHTML=h}
function tbSetBattle(b){tbBattleMode=b;setToggle('tbModeBattle','tbModeTijden',b);var d=E('tbModeDesc');if(d)d.textContent=b?'Battle: wie het eerst klaar is wint.':'Tijden: alle zones spelen door.'}
function tbSetGame(m){tbGameMode=m;for(var i=0;i<=7;i++){var b=E('tbG'+i);if(b)b.classList.toggle('on',i===m)}if(!tbPerTeam)for(var t=0;t<4;t++)tbTeamModes[t]=m}
function tbSetPerTeam(en){tbPerTeam=en;setToggle('tbModeAll','tbModePer',!en);var gm=E('tbGlobalMode'),pt=E('tbPerTeamModes'),gr=E('tbGlobalRounds');if(gm)gm.style.display=en?'none':'flex';if(pt)pt.style.display=en?'block':'none';if(gr)gr.style.display=en?'none':'block';if(en)tbRenderPerTeamModes();else{for(var t=0;t<4;t++)tbTeamModes[t]=tbGameMode}}
function tbRenderPerTeamModes(){
  var el=E('tbPerTeamModes');if(!el)return;var h='';
  for(var t=0;t<tbNumTeams;t++){
    h+='<div style="margin:8px 0;padding:8px;border:2px solid '+tbColors[t]+';border-radius:10px"><div style="font-weight:700;color:'+tbColors[t]+';font-size:.85em;margin-bottom:4px">Zone '+'ABCD'[t]+':</div><div class="tog-row" style="flex-wrap:wrap">';
    for(var m=0;m<=7;m++){h+='<button class="tog-btn'+(tbTeamModes[t]===m?' on':'')+'" onclick="tbTeamModes['+t+']='+m+';tbRenderPerTeamModes()">'+tbGameNames[m]+'</button>'}
    h+='</div><div style="display:flex;align-items:center;gap:6px;margin-top:6px"><span style="font-size:.75em;color:#888">Rondes: <b style="color:'+tbColors[t]+'" id="tbRV'+t+'">'+tbTeamRounds[t]+'</b></span><input type="range" min="3" max="30" value="'+tbTeamRounds[t]+'" style="flex:1;height:24px" oninput="tbTeamRounds['+t+']=parseInt(this.value);E(\'tbRV'+t+'\').textContent=this.value"></div></div>';
  }
  el.innerHTML=h;
}
function tbCycleGroup(id){var c=tbGroups[id];if(c==undefined||c==-1)tbGroups[id]=0;else if(c<tbNumTeams-1)tbGroups[id]=c+1;else tbGroups[id]=-1;tbRenderAssign();var toZ=tbGroups[id]!=undefined?tbGroups[id]:-1;fetch('/zones?move='+id+'&to='+toZ).then(function(r){return r.json()}).then(function(d){renderZoneTabs(d)}).catch(function(){})}
function tbAutoSplit(){if(lastTargets){var onl=[];lastTargets.forEach(function(t){if(t.online)onl.push(t.id)});onl.sort(function(a,b){return a-b});tbGroups={};for(var i=0;i<onl.length;i++)tbGroups[onl[i]]=Math.floor(i*tbNumTeams/onl.length);tbRenderAssign()}fetch('/zones?auto=1&n='+tbNumTeams).then(function(r){return r.json()}).then(function(d){renderZoneTabs(d);if(d.zones){tbGroups={};for(var zi=0;zi<d.numZones;zi++)if(Array.isArray(d.zones[zi].tgts))d.zones[zi].tgts.forEach(function(tid){tbGroups[tid]=zi});tbRenderAssign()}}).catch(function(){})}
function tbAllTeam(t){if(!lastTargets)return;lastTargets.forEach(function(tr){if(tr.online)tbGroups[tr.id]=t});tbRenderAssign();lastTargets.forEach(function(tr){if(tr.online)fetch('/zones?move='+tr.id+'&to='+t).catch(function(){})})}
function tbRenderAssign(){
  var el=E('tbAssign');if(!el||!lastTargets)return;el.innerHTML='';
  if(lastTargets)lastTargets.forEach(function(t){if(t.zone>=0&&t.zone<tbNumTeams)tbGroups[t.id]=t.zone});
  lastTargets.forEach(function(t){if(!t.online)return;var g=tbGroups[t.id]!=undefined?tbGroups[t.id]:-1;var ci=g>=0&&g<tbNumTeams?g:tbNumTeams;var clrs=tbColors.concat(['#888']);var labels='ABCD\u2014';
    var r=document.createElement('div');r.style.cssText='display:flex;align-items:center;gap:8px;margin:3px 0';
    r.innerHTML='<span style="font-weight:700;font-size:.85em;min-width:28px">T'+t.id+'</span><button onclick="tbCycleGroup('+t.id+')" style="flex:1;padding:8px;border-radius:8px;border:2px solid '+clrs[ci]+';background:'+clrs[ci]+';color:#fff;font-weight:700;cursor:pointer;font-size:.85em;min-height:40px">Zone '+labels[ci]+'</button>';
    el.appendChild(r)});
  var qr=document.createElement('div');qr.style.cssText='display:flex;gap:4px;margin-top:6px;flex-wrap:wrap';
  var qh='<button onclick="tbAutoSplit()" style="flex:1;padding:6px;border-radius:6px;border:2px solid #ddd;background:var(--w);font-size:.75em;font-weight:600;cursor:pointer;min-height:36px">Auto</button>';
  for(var i=0;i<tbNumTeams;i++)qh+='<button onclick="tbAllTeam('+i+')" style="flex:1;padding:6px;border-radius:6px;border:2px solid '+tbColors[i]+';background:var(--w);color:'+tbColors[i]+';font-size:.75em;font-weight:600;cursor:pointer;min-height:36px">Alle\u2192'+'ABCD'[i]+'</button>';
  qr.innerHTML=qh;el.appendChild(qr);
  var lg=E('tbLegend');if(lg){var h='';for(var i=0;i<tbNumTeams;i++)h+='<span><span style="display:inline-block;width:10px;height:10px;background:'+tbColors[i]+';border-radius:2px;vertical-align:middle"></span> Zone '+'ABCD'[i]+'</span>';lg.innerHTML=h}
}

// ========== CAPTURE POINTS ==========
var cpNumTeams=2,cpGroups={};
function cpSetNum(n){cpNumTeams=n;for(var i=2;i<=4;i++){var b=E('cpN'+i);if(b)b.classList.toggle('on',i===n)}cpBuildNames();cpBuildWinSliders();if(lastTargets){var onl=[];lastTargets.forEach(function(t){if(t.online)onl.push(t.id)});onl.sort(function(a,b){return a-b});cpGroups={};for(var i=0;i<onl.length;i++)cpGroups[onl[i]]=Math.floor(i*n/onl.length)}cpRenderAssign()}
function cpBuildNames(){var el=E('cpNames');if(!el)return;var defs=['Team A','Team B','Team C','Team D'];var h='';for(var i=0;i<cpNumTeams;i++){h+='<div class="prow"><div class="pdot" style="background:'+tbColors[i]+'"></div><input id="cpNm'+i+'" type="text" value="'+defs[i]+'" maxlength="15"></div>'}el.innerHTML=h}
function cpBuildWinSliders(){var el=E('cpPerWin');if(!el)return;var h='';for(var i=0;i<cpNumTeams;i++){h+='<div class="field"><label style="color:'+tbColors[i]+';font-weight:700">'+'ABCD'[i]+' — Targets om te winnen: <span id="cpWV'+i+'" style="color:var(--r)">3</span></label><div class="slider-f"><input type="range" id="cpW'+i+'" min="1" max="15" value="3" oninput="E(\'cpWV'+i+'\').textContent=this.value"></div></div>'}el.innerHTML=h}
function cpCycleGroup(id){var c=cpGroups[id];if(c==undefined||c==-1)cpGroups[id]=0;else if(c<cpNumTeams-1)cpGroups[id]=c+1;else cpGroups[id]=-1;cpRenderAssign()}
function cpAutoSplit(){if(lastTargets){var onl=[];lastTargets.forEach(function(t){if(t.online)onl.push(t.id)});onl.sort(function(a,b){return a-b});cpGroups={};for(var i=0;i<onl.length;i++)cpGroups[onl[i]]=Math.floor(i*cpNumTeams/onl.length);cpRenderAssign()}}
function cpAllTeam(t){if(!lastTargets)return;lastTargets.forEach(function(tr){if(tr.online)cpGroups[tr.id]=t});cpRenderAssign()}
function cpRenderAssign(){
  var el=E('cpAssign');if(!el||!lastTargets)return;el.innerHTML='';
  lastTargets.forEach(function(t){if(!t.online)return;var g=cpGroups[t.id]!=undefined?cpGroups[t.id]:-1;var ci=g>=0&&g<cpNumTeams?g:cpNumTeams;var clrs=tbColors.concat(['#888']);var labels='ABCD\u2014';
    var r=document.createElement('div');r.style.cssText='display:flex;align-items:center;gap:8px;margin:3px 0';
    r.innerHTML='<span style="font-weight:700;font-size:.85em;min-width:28px">T'+t.id+'</span><button onclick="cpCycleGroup('+t.id+')" style="flex:1;padding:8px;border-radius:8px;border:2px solid '+clrs[ci]+';background:'+clrs[ci]+';color:#fff;font-weight:700;cursor:pointer;font-size:.85em;min-height:40px">Team '+labels[ci]+'</button>';
    el.appendChild(r)});
  var qr=document.createElement('div');qr.style.cssText='display:flex;gap:4px;margin-top:6px;flex-wrap:wrap';
  var qh='<button onclick="cpAutoSplit()" style="flex:1;padding:6px;border-radius:6px;border:2px solid #ddd;background:var(--w);font-size:.75em;font-weight:600;cursor:pointer;min-height:36px">Auto</button>';
  for(var i=0;i<cpNumTeams;i++)qh+='<button onclick="cpAllTeam('+i+')" style="flex:1;padding:6px;border-radius:6px;border:2px solid '+tbColors[i]+';background:var(--w);color:'+tbColors[i]+';font-size:.75em;font-weight:600;cursor:pointer;min-height:36px">Alle\u2192'+'ABCD'[i]+'</button>';
  qr.innerHTML=qh;el.appendChild(qr);
  var lg=E('cpLegend');if(lg){var h='';for(var i=0;i<cpNumTeams;i++)h+='<span><span style="display:inline-block;width:10px;height:10px;background:'+tbColors[i]+';border-radius:2px;vertical-align:middle"></span> Team '+'ABCD'[i]+'</span>';lg.innerHTML=h}
}

// ========== ZONES ==========
function switchZone(z){curZone=z;pollUI()}
function setNumZones(n){
  numZones=n;
  var btns=E('zoneNums').children;for(var i=0;i<btns.length;i++)btns[i].className=i+1===n?'on':'';
  fetch('/zones?auto=1&n='+n).then(function(r){return r.json()}).then(function(d){renderZoneTabs(d);renderZoneAssign(d);toast(n+' zone'+(n>1?'s':'')+' ingesteld')}).catch(function(){});
}
function autoSplitZones(){fetch('/zones?auto=1&n='+numZones).then(function(r){return r.json()}).then(function(d){renderZoneTabs(d);renderZoneAssign(d);toast('Targets automatisch verdeeld')}).catch(function(){})}
function moveTarget(tid,toZ){fetch('/zones?move='+tid+'&to='+toZ).then(function(r){return r.json()}).then(function(d){renderZoneTabs(d);renderZoneAssign(d)}).catch(function(){})}
function renderZoneTabs(d){
  if(!d)return;numZones=d.numZones||1;
  var bar=E('zoneBar');if(!bar)return;
  if(numZones<=1){bar.style.display='none';return}
  bar.style.display='flex';bar.innerHTML='';
  for(var i=0;i<numZones;i++){var z=d.zones[i];var st=['\u23F8','\u23F3','\u25B6','\u23F8','\u2713'][z.state]||'';var cnt=Array.isArray(z.tgts)?z.tgts.length:z.tgts;var b=document.createElement('button');b.className=i===curZone?'zact':'';b.textContent=z.name+' '+st+' ('+cnt+')';b.onclick=(function(zi){return function(){switchZone(zi)}})(i);bar.appendChild(b)}
}
function renderZoneAssign(d){
  if(!d||!d.zones)return;
  var el=E('zoneAssign');if(!el)return;el.innerHTML='';
  var zClrs=['#f80','#66f','#c6f','#6ff'];
  if(numZones<=1){el.innerHTML='<div style="color:#888;font-size:.85em;padding:8px">Kies 2+ zones om targets te verdelen.</div>';E('zoneSummary').innerHTML='';return}
  var zTgts={};for(var i=0;i<numZones;i++)zTgts[i]=[];
  var allAssigned={};
  for(var i=0;i<numZones;i++){var z=d.zones[i];if(Array.isArray(z.tgts))z.tgts.forEach(function(tid){zTgts[i].push(tid);allAssigned[tid]=i})}
  if(lastTargets){
    var sorted=lastTargets.slice().sort(function(a,b){return a.id-b.id});
    sorted.forEach(function(t){
      var curZ=allAssigned.hasOwnProperty(t.id)?allAssigned[t.id]:-1;
      var row=document.createElement('div');row.className='zt-row';
      var h='<span style="color:'+(t.online?'#000':'#999')+'">T'+t.id+'</span>';
      if(!t.online){h+='<span style="color:#999;font-size:.75em;flex:1">offline</span>'}
      else{for(var zi=0;zi<numZones;zi++){var act=curZ===zi;h+='<button class="zbtn" style="border-color:'+zClrs[zi]+';background:'+(act?zClrs[zi]:'var(--w)')+';color:'+(act?'#fff':zClrs[zi])+'" onclick="moveTarget('+t.id+','+zi+')">'+d.zones[zi].name+'</button>'}h+='<button class="zbtn" style="border-color:#999;background:'+(curZ<0?'#999':'var(--w)')+';color:'+(curZ<0?'#fff':'#999')+'" onclick="moveTarget('+t.id+',-1)">&#x2715;</button>'}
      row.innerHTML=h;el.appendChild(row);
    });
  }
  var sum=E('zoneSummary');sum.innerHTML='';
  for(var i=0;i<numZones;i++){var s=document.createElement('div');s.style.borderColor=zClrs[i];s.style.color=zClrs[i];s.textContent=d.zones[i].name+': '+zTgts[i].length;sum.appendChild(s)}
}

// ========== doStart ==========
function doStart(){
  initAudio();
  var n=(E('pname').value||'Speler').trim();
  var m=curMode;var cd=E('cdSlider').value;
  var q='start?m='+m+'&pn='+encodeURIComponent(n)+'&cd='+cd;
  if(m==0){q+='&ft='+(gv('fpTime')||0)}
  else if(m==1){
    q+='&dm='+(gc('darkMode')?1:0)+'&om='+(gc('offMode')?1:0)+'&st='+(gv('snsTime')||60);
    if(snsRandomMode){q+='&snsr=1&snsns='+(gv('snsNoShootCount')||1)}
    else{for(var ti=1;ti<=20;ti++){var el=E('t'+ti+'a');if(el)q+='&t'+ti+'='+el.value}}
  }
  else if(m==2){q+='&tpt='+gv('seqTPT')+'&sr='+gv('seqR')+'&sdn='+gv('seqDN')+'&sho='+gv('seqHO');var ns=gv('nsPct');if(ns>0)q+='&nsp='+ns;var nc=gv('nsCnt');if(nc>0)q+='&nsc='+nc}
  else if(m==3){q+='&rmin='+Math.round(gv('randMin')*1000)+'&rmax='+Math.round(gv('randMax')*1000)+'&rr='+gv('randR');var ns=gv('nsPct');if(ns>0)q+='&nsp='+ns;var nc=gv('nsCnt');if(nc>0)q+='&nsc='+nc}
  else if(m==5){q+='&ml='+gv('memLen')+'&md='+gv('memDisp')+'&mw='+gv('memWrong')}
  else if(m==6){
    var c=rcPlayerCountUI;q+='&rcc='+c;
    for(var i=1;i<=c;i++)q+='&rcn'+i+'='+encodeURIComponent(gv('rcN'+i)||'Speler '+i);
    q+='&rcr='+gv('rcRoundsIn')+'&rcd='+gv('rcDispIn')+'&rcp='+gv('rcPauseIn');
  }
  else if(m==7){
    var c=rxPlayerCountUI;q+='&rxc='+c;
    for(var i=1;i<=c;i++)q+='&rxn'+i+'='+encodeURIComponent(gv('rxN'+i)||'Speler '+i);
    q+='&rxr='+gv('rxRoundsIn')+'&rxdmin='+gv('rxDelMin')+'&rxdmax='+gv('rxDelMax')+'&rxf='+(rxFixedMode?1:0);
    var ns=gv('nsPct');if(ns>0)q+='&nsp='+ns;var nc=gv('nsCnt');if(nc>0)q+='&nsc='+nc;
  }
  else if(m==8){
    q+='&prf='+(parRandomMode?1:0)+'&pdn='+gv('parDN')+'&pao='+gv('parAO')+'&pho='+gv('parHO');
    if(!parRandomMode&&parRouteIds.length>0)q+='&pro='+parRouteIds.join(',');
    var ns=gv('nsPct');if(ns>0)q+='&nsp='+ns;var nc=gv('nsCnt');if(nc>0)q+='&nsc='+nc;
  }
  else if(m==9){
    q+='&ftc='+ftPlayerCount;
    for(var i=1;i<=ftPlayerCount;i++)q+='&ftn'+i+'='+encodeURIComponent(gv('ftn'+i)||gv('ftN'+i)||'Speler '+i);
    q+='&ftt='+gv('ftTpp')+'&ftf='+(ftFixedMode?1:0);
    var ns=gv('nsPct');if(ns>0)q+='&nsp='+ns;var nc=gv('nsCnt');if(nc>0)q+='&nsc='+nc;
  }
  else if(m==10){
    q+='&tbr='+gv('tbRounds')+'&tbgm='+tbGameMode+'&tbb='+(tbBattleMode?1:0)+'&tbn='+tbNumTeams;
    for(var ti=0;ti<tbNumTeams;ti++){var ne=E('tbNm'+ti);if(ne)q+='&tbn'+ti+'='+encodeURIComponent(ne.value);if(tbPerTeam)q+='&tbgm'+ti+'='+tbTeamModes[ti]+'&tbr'+ti+'='+tbTeamRounds[ti]}
    var gArr=[];if(lastTargets){for(var i=0;i<lastTargets.length;i++){var g=tbGroups[lastTargets[i].id];gArr.push(g!=undefined?g:-1)}}
    q+='&tbg='+gArr.join(',');
  }
  else if(m==11){
    q+='&cph='+gv('cpHits')+'&cpgt='+gv('cpTime')+'&cpr='+(gc('cpRecap')?1:0)+'&cplt='+(gc('cpRecap')?gv('cpLockSlider'):'0')+'&tbn='+cpNumTeams;
    for(var ti=0;ti<cpNumTeams;ti++){var ne=E('cpNm'+ti);if(ne)q+='&tbn'+ti+'='+encodeURIComponent(ne.value);q+='&cpw'+ti+'='+(gv('cpW'+ti)||3)}
    var gArr=[];if(lastTargets){for(var i=0;i<lastTargets.length;i++){var g=cpGroups[lastTargets[i].id];gArr.push(g!=undefined?g:-1)}}
    q+='&tbg='+gArr.join(',');
  }
  // Toerbeurt parameters
  if(gc('trnOn')&&trnPlayerList.length>=2){
    q+='&trn=1&trnc='+trnPlayerList.length;
    for(var ti=0;ti<trnPlayerList.length;ti++)q+='&trnp'+(ti+1)+'='+encodeURIComponent(trnPlayerList[ti]);
    if(gc('trnAuto')){q+='&trna=1&trncd='+(gv('trnCd')||5)}
  }
  saveSettings();closeSettings();cmd(q);
}
function gv(id){var el=E(id);return el?el.value:''}
function gc(id){var el=E(id);return el?el.checked:false}

// ========== SAVE/LOAD SETTINGS ==========
function saveSettings(){try{var s={mode:curMode,cd:E('cdSlider').value};localStorage.setItem('rtt_cfg',JSON.stringify(s))}catch(e){}}
function loadSettings(){try{var s=JSON.parse(localStorage.getItem('rtt_cfg'));if(s){if(s.cd)E('cdSlider').value=s.cd;E('cdVal').textContent=E('cdSlider').value+'s';if(s.mode!=null)curMode=s.mode}}catch(e){}}
function loadModeSettings(m){
  // Populate SNS targets when opening mode 1
  if(m===1&&lastTargets){
    var sc=E('snsTargets');if(sc&&sc.children.length!==lastTargets.length){sc.innerHTML='';lastTargets.forEach(function(t){sc.innerHTML+='<div class="prow"><span style="min-width:60px;font-weight:700;font-size:.85em">Target '+t.id+':</span><select id="t'+t.id+'a" style="flex:1;padding:8px;border:2px solid #ddd;border-radius:8px"><option value="1"'+(t.shoot?' selected':'')+'>SHOOT (+10)</option><option value="0"'+(!t.shoot?' selected':'')+'>NO-SHOOT (-11)</option></select></div>'})}
  }
  if(m===8)parBuildAvail(lastTargets);
  if(m===10){tbBuildNames();tbRenderAssign()}
  if(m===11){cpBuildNames();cpBuildWinSliders();if(!Object.keys(cpGroups).length)cpAutoSplit();cpRenderAssign()}
}

// ========== CONFIRM DIALOG ==========
function confirmStop(){E('confirmDlg').classList.add('open')}
function closeConfirm(){E('confirmDlg').classList.remove('open')}
function doStop(){closeConfirm();cmd('stop')}
function saveScore(){var n=E('rName').value||E('pname').value||'Speler';cmd('savescore?name='+encodeURIComponent(n));toast('Score opgeslagen!')}

// ========== UPDATE / POLL ==========
function pollUI(){fetch('/status?z='+curZone).then(function(r){return r.json()}).then(function(d){update(d);if(d.zones)renderZoneTabs(d)}).catch(function(){})}
setInterval(pollUI,200);
setTimeout(pollUI,100);

function update(d){
  checkAudio(d);lastData=d;
  // Auto screen transitions
  if(d.state===0&&!wasGameScreen){showScreen('scrHome')}
  else if(d.state===1||d.state===2||d.state===3){showScreen('scrGame');wasGameScreen=true}
  else if(d.state===5){showScreen('scrTurn');wasGameScreen=true;updateTurnWait(d)}
  else if(d.state===4){
    if(d.turnMode&&d.turnCount>0&&d.turnCur>=d.turnCount)showScreen('scrRes');
    else showScreen('scrRes');
    wasGameScreen=true;
  }
  else if(d.state===0&&wasGameScreen){showScreen('scrRes')}

  // Online badge
  E('onlineBadge').textContent=(d.numOnline||0)+'/'+d.numTargets+' targets';

  // Update targets & manual btns
  if(!lastTargets||lastTargets.length!==d.targets.length){parBuildAvail(d.targets)}
  lastTargets=d.targets;

  // FW info
  if(d.fw){var fi=E('fwInfo');if(fi)fi.textContent='RTT '+d.fw+(d.chipId?' | '+d.chipId:'')}

  // Sort targets
  var sorted=d.targets.slice().sort(function(a,b){return a.id-b.id});
  var zClrs=['#f80','#66f','#c6f','#6ff'];

  if(d.state>=1&&d.state<=3) updateGame(d,sorted,zClrs);
  if(d.state===4||wasGameScreen) updateResults(d,sorted);
  updateManual(sorted);
  updateLeaderboard(d);
  updateOTA(d);
}

function updateGame(d,sorted,zClrs){
  // Timer
  var ms=d.elapsed;
  if(d.state==1){E('gTimer').textContent='0.000';E('gRemain').textContent='Start in '+d.countdown+'...'}
  else{E('gTimer').textContent=fmtMs(ms);E('gRemain').textContent=getRemaining(d)}
  // State badge
  var badges=[null,['COUNTDOWN','sb-cd'],['ACTIEF','sb-run'],['GEPAUZEERD','sb-pause']];
  var b=badges[d.state];if(b){E('gStateBadge').textContent=b[0];E('gStateBadge').className='state-badge '+b[1]}
  // Player name
  var gpn=E('gPlayerName');if(d.pn&&d.pn!=='Speler'){gpn.textContent=d.pn;gpn.style.display=''}else{gpn.style.display='none'}
  // No-shoot badge
  var nsb=E('gNsBadge');if(d.nsp>0||d.nsCount>0){nsb.style.display='';E('gNsPct').textContent=d.nsp>0?d.nsp+'%':d.nsCount+' st'}else{nsb.style.display='none'}
  // Turn-mode badge
  var tb=E('gTurnBadge');if(d.turnMode&&d.turnCount>0){tb.style.display='';E('gTurnName').textContent=d.turnPlayers&&d.turnPlayers[d.turnCur]?d.turnPlayers[d.turnCur].name:'Speler';E('gTurnNum').textContent=(d.turnCur+1)+'/'+d.turnCount}else{tb.style.display='none'}
  // Score
  E('gScore').textContent=d.score;E('gHits').textContent=d.hits;E('gMisses').textContent=d.misses||0;
  // Target grid
  var g=E('gTgrid');g.innerHTML='';
  sorted.forEach(function(t){
    var div=document.createElement('div');
    div.className='gt-box'+(t.active?' on':'')+(!t.online?' off':'');
    if(t.active){var bg='rgb('+t.r+','+t.g+','+t.b+')';div.style.background=bg;div.style.borderColor=bg}
    if(numZones>1&&t.zone>=0&&t.zone<numZones){div.style.borderBottomColor=zClrs[t.zone];div.style.borderBottomWidth='3px'}
    div.innerHTML='T'+t.id+'<div style="font-size:.6em;color:'+(t.active?'rgba(255,255,255,.8)':'#888')+'">'+t.hits+'</div>';
    if(t.online)div.onclick=(function(id){return function(){cmd('locate?t='+id)}})(t.id);
    g.appendChild(div);
  });
  // Multiplayer cards
  var pc=E('gPcards');pc.innerHTML='';
  if(d.mode==6&&d.rcPlayers&&d.rcPlayers.length>0)renderRCCards(pc,d);
  else if(d.mode==7&&d.rcPlayers&&d.rcPlayers.length>0)renderRXCards(pc,d);
  else if(d.mode==8&&d.parTotal>0)renderParCards(pc,d);
  else if(d.mode==9&&d.ftPlayers&&d.ftPlayers.length>0)renderFTCards(pc,d);
  else if(d.mode==10&&d.tbTeams&&d.tbTeams.length>=2)renderTBCards(pc,d);
  else if(d.mode==11&&d.cpTeams&&d.cpTeams.length>=2)renderCPCards(pc,d);
}

function getRemaining(d){
  if(d.state==1)return'';
  if(d.mode==2&&(d.state==2||d.state==4))return'Target '+(d.seqRound+1)+'/'+d.seqRounds+(d.seqHitsToOff>1?' ('+d.seqCurHits+'/'+d.seqHitsToOff+')':'')+(d.seqWaiting?' ...':'');
  if(d.mode==3&&(d.state==2||d.state==4))return'Target '+(d.randRound+1)+'/'+d.randRounds;
  if(d.mode==5&&d.state==2){if(d.memPhase<3)return'Onthoud de volgorde...';if(d.memPhase<5)return'Maak je klaar!';if(d.memPhase==5)return'Schiet! '+d.memShot+'/'+d.memTotal+(d.memWrong>0?' \u2014 Fout: '+d.memWrong:'')}
  if(d.mode==6&&(d.state==2||d.state==4))return'Target '+d.rcRound+'/'+d.rcRounds;
  if(d.mode==7&&(d.state==2||d.state==4)){var t='Ronde '+(d.rxCycle+1)+'/'+d.rxRounds;return d.rxWaiting?t+' \u2014 Wacht...':t+' \u2014 SCHIET!'}
  if(d.mode==8&&(d.state==2||d.state==4))return'Target '+(d.parCurrent<d.parTotal?d.parCurrent+1:d.parTotal)+'/'+d.parTotal+(d.parHitsToOff>1?' ('+d.parCurHits+'/'+d.parHitsToOff+')':'')+(d.parWaiting?' ...':'');
  if(d.mode==9&&(d.state==2||d.state==4)){if(d.ftWinner>=0&&d.ftPlayers)return'WINNAAR: '+d.ftPlayers[d.ftWinner].name+'!';return'Fast Track \u2014 '+d.ftTPP+' targets/speler'}
  if(d.mode==10&&(d.state==2||d.state==4)){
    if(d.tbWinner>=0&&d.tbTeams)return(d.tbBattle?'WINNAAR: ':'Snelste: ')+d.tbTeams[d.tbWinner].name+'!';
    if(d.tbTeams){var p=[];for(var i=0;i<d.tbTeams.length;i++){var tm=d.tbTeams[i];var rn=tm.rnd||d.tbRounds;p.push(tm.name+': '+(tm.done?fmtMs(tm.finishMs):tm.hits+'/'+rn))}return p.join(' \u2014 ')}
  }
  if(d.mode==11&&(d.state==2||d.state==4)){
    if(d.cpWinner>=0&&d.cpTeams)return'\u{1F3C6} WINNAAR: '+d.cpTeams[d.cpWinner].name+'!';
    if(d.cpTeams){var p=[];for(var i=0;i<d.cpTeams.length;i++){p.push(d.cpTeams[i].name+': '+(d.cpCap[i]||0)+'/'+(d.cpWin[i]||'?'))}return p.join(' \u2014 ')}
  }
  if(d.gt>0&&(d.state==2||d.state==3)){var rem=Math.max(0,d.gt-d.elapsed);return'Resterend: '+fmtSec(rem)}
  return'';
}

// ========== MULTIPLAYER CARD RENDERERS ==========
function makeCard(clr,name,score,pct,stats,extra){
  return '<div class="pcard" style="border-color:'+clr+'"><div class="pcard-hdr"><div class="pcard-dot" style="background:'+clr+'"></div><span class="pcard-name" style="color:'+clr+'">'+name+'</span><span class="pcard-score">'+score+'</span></div><div class="pcard-bar"><div class="pcard-fill" style="background:'+clr+';width:'+Math.min(100,pct)+'%"></div></div><div class="pcard-stats">'+stats+'</div>'+(extra||'')+'</div>';
}
function renderRCCards(el,d){
  var s=d.rcPlayers.slice().sort(function(a,b){if(a.done&&!b.done)return -1;if(!a.done&&b.done)return 1;if(a.done&&b.done)return a.finishMs-b.finishMs;return b.score-a.score});
  s.forEach(function(p,i){var clr='rgb('+p.r+','+p.g+','+p.b+')';var pct=d.rcRounds>0?Math.round(p.cycles/d.rcRounds*100):0;var ft=p.done?fmtMs(p.finishMs):'...';el.innerHTML+=makeCard(clr,(d.state==4&&p.done&&i==0?'\u{1F3C6} ':'')+p.name,p.score,pct,'<span>Raak: '+p.correct+'</span><span>Gem: '+p.avgMs+'ms</span><span>'+p.cycles+'/'+d.rcRounds+'</span><span>'+ft+'</span>')});
}
function renderRXCards(el,d){
  d.rcPlayers.forEach(function(p,pi){var clr='rgb('+p.r+','+p.g+','+p.b+')';var avg=p.correct>0?p.avgMs+' ms':'-';var best='-';if(d.rxTimes&&d.rxTimes[pi]){var b=9999;d.rxTimes[pi].forEach(function(t){if(t>0&&t<b)b=t});if(b<9999)best=b+'ms'}var pct=d.rxRounds>0?Math.round(p.correct/d.rxRounds*100):0;
    var times='';if(d.rxTimes&&d.rxTimes[pi])times='<div style="font-size:.75em;margin-top:4px;color:#666">'+d.rxTimes[pi].map(function(t){return t>=9999?'<span style="color:var(--r)">X</span>':'<span style="color:'+(t<500?'#4CAF50':t<1000?'#FFD600':'#FF9800')+'">'+t+'</span>'}).join(', ')+'</div>';
    el.innerHTML+=makeCard(clr,p.name,avg,pct,'<span>Beste: '+best+'</span><span>Hits: '+p.correct+'/'+d.rxCycle+'</span>',times)});
}
function renderParCards(el,d){
  var h='<div class="extra-card"><h3>Parcours Splits</h3><table class="lb-table"><thead><tr><th>#</th><th>Target</th><th>Split</th><th>Tussen</th><th></th></tr></thead><tbody>';
  for(var i=0;i<d.parTotal;i++){var tid=d.parOrder?d.parOrder[i]:'?';var sp=d.parSplits&&d.parSplits[i]?d.parSplits[i]:0;var lap=sp;if(i>0&&d.parSplits&&d.parSplits[i-1])lap=sp-d.parSplits[i-1];var done=sp>0;var act=i==d.parCurrent&&d.state==2;
    h+='<tr style="'+(act?'background:rgba(220,0,0,.08)':'')+'"><td>'+(i+1)+'</td><td>T'+tid+'</td><td style="font-weight:700;color:'+(done?'#4CAF50':'#888')+'">'+(done?fmtMs(sp):'-')+'</td><td style="color:'+(done?(lap<1000?'#4CAF50':lap<2000?'#FFD600':'#FF9800'):'#888')+'">'+(done?fmtMs(lap):'-')+'</td><td>'+(done?'&#x2713;':act?'&#x25C4; NU':'\u2014')+'</td></tr>'}
  h+='</tbody></table></div>';el.innerHTML=h;
}
function renderFTCards(el,d){
  var s=d.ftPlayers.map(function(p,i){p._i=i;return p}).sort(function(a,b){if(b.hits!==a.hits)return b.hits-a.hits;if(a.finishMs&&b.finishMs)return a.finishMs-b.finishMs;return 0});
  s.forEach(function(p,rank){var clr='rgb('+p.r+','+p.g+','+p.b+')';var pct=d.ftTPP>0?Math.round(p.hits/d.ftTPP*100):0;var st=p.done?fmtMs(p.finishMs):p.hits+'/'+d.ftTPP;var tgt=p.target>0?'T'+p.target:'-';
    el.innerHTML+=makeCard(clr,(rank==0&&d.ftWinner>=0?'\u{1F3C6} ':'')+p.name,p.score,pct,'<span>Hits: '+p.hits+'/'+d.ftTPP+'</span><span>Target: '+tgt+'</span><span>'+st+'</span>')});
}
function renderTBCards(el,d){
  var gmNames=['Random','Sequence','SNS','Reaction','Parcours','Memory','Rand Clr','Fast Track'];
  for(var t=0;t<d.tbTeams.length;t++){var tm=d.tbTeams[t];if(tm.count==0)continue;var clr=tbColors[t];var gm=tm.gm!=undefined?tm.gm:d.tbGame;var rnd=tm.rnd||d.tbRounds;var pct=rnd>0?Math.round(tm.hits/rnd*100):0;var winner=d.tbWinner==t;
    var st=tm.done?fmtMs(tm.finishMs):tm.hits+'/'+rnd;var badge=winner?(d.tbBattle?'\u{1F3C6} ':'\u26A1 '):'';
    var extra='<div style="display:flex;gap:4px;margin-top:6px">'+(tm.done?'<button onclick="cmd(\'tbcontrol?t='+t+'&a=restart\')" style="flex:1;padding:6px;background:var(--w);border:2px solid '+clr+';border-radius:8px;color:'+clr+';font-weight:700;font-size:.75em;cursor:pointer;min-height:36px">Herstart</button>':'<button onclick="cmd(\'tbcontrol?t='+t+'&a=stop\')" style="flex:1;padding:6px;background:var(--w);border:2px solid var(--r);border-radius:8px;color:var(--r);font-weight:700;font-size:.75em;cursor:pointer;min-height:36px">Stop</button>')+'<button onclick="if(confirm(\'Reset '+tm.name+'?\'))cmd(\'tbcontrol?t='+t+'&a=reset\')" style="flex:1;padding:6px;background:var(--w);border:2px solid #999;border-radius:8px;color:#999;font-weight:700;font-size:.75em;cursor:pointer;min-height:36px">Reset</button></div>';
    el.innerHTML+=makeCard(clr,badge+tm.name+(gmNames[gm]?' <small style="color:#888">'+gmNames[gm]+'</small>':''),tm.score,pct,'<span>'+st+'</span><span>Target: '+(tm.target>0?'T'+tm.target:'\u2014')+'</span>',extra)}
}
function renderCPCards(el,d){
  for(var t=0;t<d.cpTeams.length;t++){var tm=d.cpTeams[t];if(tm.count==0)continue;
    var clr=tbColors[t];var cap=d.cpCap[t]||0;var win=d.cpWin[t]||3;
    var pct=win>0?Math.round(cap/win*100):0;var winner=d.cpWinner==t;
    var badge=winner?'\u{1F3C6} ':'';
    el.innerHTML+=makeCard(clr,badge+tm.name,cap+'/'+win,pct,'<span>Veroverd: '+cap+'/'+win+'</span><span>Targets: '+tm.count+'</span>')}
  // Per-target status
  if(d.cpTargets&&d.cpTargets.length>0){
    var h='<div style="margin:12px 0;padding:10px;background:#f8f8f8;border-radius:12px"><div style="font-weight:700;font-size:.85em;margin-bottom:8px">Target Status</div><div style="display:flex;flex-wrap:wrap;gap:6px">';
    for(var i=0;i<d.cpTargets.length;i++){var ct=d.cpTargets[i];if(ct.o<0)continue;
      var oclr=tbColors[ct.o]||'#888';var cclr=ct.c&&ct.cb>=0?tbColors[ct.cb]:oclr;
      var bg=ct.c?cclr+'33':'transparent';
      var brd=ct.c?'3px solid '+cclr:'2px solid '+oclr;
      var anim=ct.c&&!ct.lk?' animate-pulse':'';
      var lockPct=0;
      if(ct.c&&!ct.lk&&d.cpLock>0&&ct.ca>=0){lockPct=Math.min(100,Math.round(ct.ca/(d.cpLock*60)*100))}
      h+='<div class="'+anim+'" style="width:48px;height:56px;display:flex;flex-direction:column;align-items:center;justify-content:center;border:'+brd+';border-radius:10px;background:'+bg+';font-size:.7em;font-weight:700;position:relative;overflow:hidden">';
      if(ct.c&&!ct.lk&&d.cpLock>0){h+='<div style="position:absolute;bottom:0;left:0;width:100%;height:'+lockPct+'%;background:'+cclr+'44;transition:height .5s"></div>'}
      h+='<span style="color:'+cclr+';position:relative">T'+(i+1)+'</span>';
      if(!ct.c)h+='<span style="color:#888;font-size:.85em;position:relative">'+ct.h+'/'+d.cpHits+'</span>';
      else if(ct.lk)h+='<span style="font-size:.85em;position:relative">\uD83D\uDD12</span>';
      else h+='<span style="font-size:.85em;position:relative">\u2713</span>';
      h+='</div>'}
    h+='</div></div>';el.innerHTML+=h}
}

// ========== RESULTS ==========
function updateResults(d){
  E('rScore').textContent=d.score;E('rHits').textContent=d.hits;E('rMisses').textContent=d.misses||0;E('rTime').textContent=fmtSec(d.elapsed);
  E('rName').value=E('pname').value||'';
  var pc=E('rPcards');pc.innerHTML='';
  // Turn-mode ranglijst
  if(d.turnMode&&d.turnPlayers&&d.turnPlayers.length>0){
    E('rTitle').textContent='TOERBEURT AFGELOPEN';
    pc.innerHTML=renderTurnRankings(d);
  }
  else if(d.mode==6&&d.rcPlayers&&d.rcPlayers.length>0)renderRCCards(pc,d);
  else if(d.mode==7&&d.rcPlayers&&d.rcPlayers.length>0)renderRXCards(pc,d);
  else if(d.mode==8&&d.parTotal>0)renderParCards(pc,d);
  else if(d.mode==9&&d.ftPlayers&&d.ftPlayers.length>0)renderFTCards(pc,d);
  else if(d.mode==10&&d.tbTeams&&d.tbTeams.length>=2)renderTBCards(pc,d);
  else if(d.mode==11&&d.cpTeams&&d.cpTeams.length>=2)renderCPCards(pc,d);
}
function updateLeaderboard(d){
  var fill=function(tbId){
    var lb=E(tbId);if(!lb)return;lb.innerHTML='';
    if(d.top)d.top.forEach(function(s,i){if(s.name==='---')return;var tm=fmtMs(s.time);lb.innerHTML+='<tr><td>'+(i+1)+'</td><td>'+s.name+'</td><td>'+s.mode+'</td><td>'+s.hits+'</td><td>'+tm+'</td></tr>'});
  };
  fill('lb');fill('lbExtra');
}
function updateManual(sorted){
  var mb=E('manualBtns');if(!mb)return;mb.innerHTML='';
  sorted.forEach(function(t){var row=document.createElement('div');row.className='act-row';
    if(!t.online){row.innerHTML='<span style="opacity:.4">T'+t.id+'</span><span style="color:#999;font-size:.75em;flex:1">offline</span>'}
    else{row.innerHTML='<span>T'+t.id+'</span><button class="cbtn cbtn-r" onclick="cmd(\'activate?t='+t.id+'&c=r\')">R</button><button class="cbtn cbtn-g" onclick="cmd(\'activate?t='+t.id+'&c=g\')">G</button><button class="cbtn cbtn-b" onclick="cmd(\'activate?t='+t.id+'&c=b\')">B</button><button class="cbtn cbtn-off" onclick="cmd(\'activate?t='+t.id+'&c=off\')">Uit</button>'}
    mb.appendChild(row);
  });
}
function updateOTA(d){
  var ob=E('otaBtns');if(!ob||!d.targets)return;
  if(ob.children.length!==d.targets.length){ob.innerHTML='';d.targets.forEach(function(t){var col=t.online?'var(--r)':'#999';ob.innerHTML+='<button onclick="if(confirm(\'Target '+t.id+' updaten?\')){location.href=\'/otaTarget?t='+t.id+'\'}" style="background:'+col+';color:var(--w);border:none;padding:8px 12px;border-radius:8px;margin:2px;font-size:.8em;cursor:pointer;font-weight:600;min-height:40px"'+(t.online?'':' disabled')+'>T'+t.id+(t.online?' &#x2705;':' &#x274C;')+'</button>'})}
}

// ========== TURN WAIT SCREEN ==========
function updateTurnWait(d){
  if(!d.turnMode||!d.turnPlayers)return;
  // Vorige speler resultaat
  var prev=d.turnCur>0?d.turnPlayers[d.turnCur-1]:null;
  var pe=E('twPrevResult');
  if(prev&&prev.done){pe.innerHTML='<div style="font-size:1.1em;font-weight:700;color:#4CAF50">&#x2705; '+prev.name+' &#x2014; '+prev.score+' punten</div><div style="font-size:.85em;color:#666">Raak: '+prev.hits+' | Mis: '+prev.misses+' | Tijd: '+fmtMs(prev.time)+'</div>'}
  else{pe.innerHTML=''}
  // Volgende speler
  var cur=d.turnPlayers[d.turnCur];
  E('twNextInfo').textContent='Volgende: '+(cur?cur.name:'?')+' ('+((d.turnCur||0)+1)+' van '+d.turnCount+')';
  // Countdown
  var ce=E('twCountdown');
  if(d.turnAuto){
    var elapsed=d.turnWaitMs||0;
    var total=(d.turnCd||5)*1000;
    var rem=Math.max(0,total-elapsed);
    ce.textContent=Math.ceil(rem/1000)+'...';
  }else{ce.textContent=''}
  // Mini-ranking
  var re=E('twRanking');
  var done=d.turnPlayers.filter(function(p){return p.done});
  if(done.length>0){
    done.sort(function(a,b){if(b.hits!==a.hits)return b.hits-a.hits;return a.time-b.time});
    var h='<div style="font-size:.85em;font-weight:700;margin-bottom:6px">Tussenstand:</div><table class="lb-table"><thead><tr><th>#</th><th>Naam</th><th>Raak</th><th>Score</th><th>Tijd</th></tr></thead><tbody>';
    done.forEach(function(p,i){h+='<tr><td>'+(i+1)+'</td><td>'+p.name+'</td><td style="font-weight:700;color:var(--r)">'+p.hits+'</td><td>'+p.score+'</td><td>'+fmtMs(p.time)+'</td></tr>'});
    h+='</tbody></table>';re.innerHTML=h;
  }else{re.innerHTML=''}
}

// ========== TURN RANKINGS (final results) ==========
function renderTurnRankings(d){
  if(!d.turnMode||!d.turnPlayers)return'';
  var ps=d.turnPlayers.filter(function(p){return p.done});
  if(ps.length===0)return'';
  ps.sort(function(a,b){if(b.hits!==a.hits)return b.hits-a.hits;return a.time-b.time});
  var h='<div class="extra-card"><h3>&#x1F3C6; Toerbeurt Ranglijst</h3><table class="lb-table"><thead><tr><th>#</th><th>Naam</th><th>Raak</th><th>Score</th><th>Mis</th><th>Tijd</th></tr></thead><tbody>';
  ps.forEach(function(p,i){
    var medal=i===0?'&#x1F947; ':i===1?'&#x1F948; ':i===2?'&#x1F949; ':'';
    h+='<tr style="'+(i===0?'background:rgba(255,215,0,.15)':'')+'"><td>'+medal+(i+1)+'</td><td style="font-weight:700">'+p.name+'</td><td style="font-weight:700;color:var(--r)">'+p.hits+'</td><td>'+p.score+'</td><td>'+p.misses+'</td><td>'+fmtMs(p.time)+'</td></tr>'});
  h+='</tbody></table></div>';
  return h;
}

// ========== EMAIL ==========
function emailScores(){
  var rows=document.querySelectorAll('#lbExtra tr, #lb tr');
  var lines=['RAF RTT TRAINING SYSTEM - TOP SCORES','','#  | Naam | Modus | Raak | Tijd','---|------|-------|------|-----'];
  var seen={};
  for(var i=0;i<rows.length;i++){var cells=rows[i].querySelectorAll('td');if(cells.length>=5){var key=cells[1].textContent+cells[3].textContent;if(!seen[key]){lines.push(cells[0].textContent+' | '+cells[1].textContent+' | '+cells[2].textContent+' | '+cells[3].textContent+' | '+cells[4].textContent);seen[key]=1}}}
  if(lines.length<=4)lines.push('Nog geen scores.');
  lines.push('','Datum: '+new Date().toLocaleDateString('nl-NL')+' '+new Date().toLocaleTimeString('nl-NL'));
  var txt=lines.join('\n');
  if(navigator.share){navigator.share({title:'RTT Top Scores',text:txt}).catch(function(){})}
  else{window.location.href='mailto:?subject='+encodeURIComponent('RTT Top Scores')+'\x26body='+encodeURIComponent(txt)}
}
</script>
</body></html>
)rawliteral";
  
  server.send_P(200, "text/html", page);
}

// ============================================================
// WEB SERVER - API ENDPOINTS
// ============================================================

void handleStatus() {
  uint8_t z = server.hasArg("z") ? constrain(server.arg("z").toInt(), 0, MAX_ZONES-1) : 0;
  // Tournament always runs in zone 0 — show same state on all phones
  if ((zones[0].mode == MODE_TOURNAMENT || zones[0].mode == MODE_CAPTURE) && zones[0].state >= STATE_COUNTDOWN) z = 0;
  uint8_t prevZone = activeZone;  // Remember which zone was active before
  loadZone(z);
  
  unsigned long elapsed = getElapsed();
  uint32_t gt = getGameTimeMs();
  
  // Countdown remaining
  int countdown = 0;
  if (gameState == STATE_COUNTDOWN) {
    int cdElapsed = (millis() - countdownStart) / 1000;
    countdown = countdownTotal - cdElapsed;
    if (countdown < 0) countdown = 0;
  }
  
  String json;
  json.reserve(16384);  // Pre-allocate — worst case ~12KB (reaction 4p×60cycles + tournament + leaderboard)
  json = "{";
  json += "\"state\":" + String((int)gameState);
  json += ",\"mode\":" + String((int)currentMode);
  json += ",\"score\":" + String(currentScore);
  json += ",\"fw\":\"V3Sec\"";
  json += ",\"chipId\":\"" + String((uint32_t)(ESP.getEfuseMac() >> 16), HEX) + "\"";
  json += ",\"numTargets\":" + String(numTargets);
  json += ",\"numOnline\":" + String(onlineCount);
  json += ",\"hits\":" + String(totalHits);
  json += ",\"misses\":" + String(totalMisses);
  json += ",\"elapsed\":" + String(elapsed);
  json += ",\"gt\":" + String(gt);
  json += ",\"countdown\":" + String(countdown);
  json += ",\"threshold\":" + String(settings.piezoThreshold);
  json += ",\"nsp\":" + String(settings.noShootPct);
  json += ",\"nsCount\":" + String(settings.noShootCount);
  json += ",\"pn\":\"" + String(playerName) + "\"";
  json += ",\"seqRound\":" + String(seqRound);
  json += ",\"seqRounds\":" + String(settings.seqRounds);
  json += ",\"seqCurHits\":" + String(seqCurHits);
  json += ",\"seqHitsToOff\":" + String(settings.seqHitsToOff);
  json += ",\"seqWaiting\":" + String(seqWaiting ? "true" : "false");
  json += ",\"randRound\":" + String(randRound);
  json += ",\"randRounds\":" + String(settings.randRounds);
  json += ",\"darkMode\":" + String(settings.snsDarkMode ? "true" : "false");
  json += ",\"memPhase\":" + String(memPhase);
  json += ",\"memShot\":" + String(memShootIdx);
  json += ",\"memTotal\":" + String(settings.memLength);
  json += ",\"memWrong\":" + String(memWrongCount);
  
  // Random Multi player data
  json += ",\"rcRound\":" + String(rcCycle);
  json += ",\"rcRounds\":" + String(settings.rcRounds);
  json += ",\"rcPlayers\":[";
  for (int i = 0; i < rcPlayerCount; i++) {
    if (i > 0) json += ",";
    json += "{\"name\":\"" + String(rcPlayers[i].name) + "\"";
    json += ",\"r\":" + String(rcPlayers[i].r);
    json += ",\"g\":" + String(rcPlayers[i].g);
    json += ",\"b\":" + String(rcPlayers[i].b);
    json += ",\"score\":" + String(rcPlayers[i].score);
    json += ",\"correct\":" + String(rcPlayers[i].correctHits);
    json += ",\"wrong\":" + String(rcPlayers[i].wrongHits);
    json += ",\"avgMs\":" + String(rcPlayers[i].correctHits > 0 ? rcPlayers[i].totalReactionMs / rcPlayers[i].correctHits : 0);
    json += ",\"cycles\":" + String(rcPlayers[i].cyclesCompleted);
    json += ",\"done\":" + String(rcPlayers[i].allCyclesDone ? "true" : "false");
    json += ",\"finishMs\":" + String(rcPlayers[i].finishTimeMs);
    json += ",\"hitCycle\":" + String(rcPlayers[i].targetsHitThisCycle);
    json += ",\"totalCycle\":" + String(rcPlayers[i].targetsThisCycle);
    json += ",\"totalAssigned\":" + String(rcPlayers[i].totalAssigned);
    json += "}";
  }
  json += "]";
  
  // Reaction mode data
  json += ",\"rxCycle\":" + String(rxCycle);
  json += ",\"rxRounds\":" + String(settings.rxRounds);
  json += ",\"rxWaiting\":" + String(rxWaiting ? "true" : "false");
  if (currentMode == MODE_REACTION) {
    json += ",\"rxTimes\":[";
    for (int p = 0; p < rcPlayerCount; p++) {
      if (p > 0) json += ",";
      json += "[";
      for (int c = 0; c < rxCycle && c < RX_MAX_CYCLES; c++) {
        if (c > 0) json += ",";
        json += String(rxTimes[p][c]);
      }
      json += "]";
    }
    json += "]";
  }
  
  // Parcours data
  json += ",\"parCurrent\":" + String(parCurrent);
  json += ",\"parTotal\":" + String(parTotal);
  json += ",\"parCurHits\":" + String(parCurHits);
  json += ",\"parHitsToOff\":" + String(settings.parHitsToOff);
  json += ",\"parWaiting\":" + String(parWaiting ? "true" : "false");
  if (currentMode == MODE_PARCOURS) {
    json += ",\"parSplits\":[";
    for (int i = 0; i < parTotal; i++) {
      if (i > 0) json += ",";
      json += String(parSplits[i]);
    }
    json += "]";
    json += ",\"parOrder\":[";
    for (int i = 0; i < parTotal; i++) {
      if (i > 0) json += ",";
      json += String(targets[parOrder[i]].id);
    }
    json += "]";
  }
  
  // Fast Track data
  if (currentMode == MODE_FASTTRACK) {
    json += ",\"ftWinner\":" + String(ftWinner);
    json += ",\"ftTPP\":" + String(settings.ftTargetsPerPlayer);
    json += ",\"ftFixed\":" + String(settings.ftFixed ? "true" : "false");
    json += ",\"ftPlayers\":[";
    for (int i = 0; i < rcPlayerCount; i++) {
      if (i > 0) json += ",";
      json += "{\"name\":\"" + String(rcPlayers[i].name) + "\"";
      json += ",\"r\":" + String(rcPlayers[i].r);
      json += ",\"g\":" + String(rcPlayers[i].g);
      json += ",\"b\":" + String(rcPlayers[i].b);
      json += ",\"hits\":" + String(ftPlayerHits[i]);
      json += ",\"score\":" + String(rcPlayers[i].score);
      json += ",\"target\":" + String(ftPlayerTarget[i] >= 0 ? targets[ftPlayerTarget[i]].id : -1);
      json += ",\"done\":" + String(ftPlayerHits[i] >= settings.ftTargetsPerPlayer ? "true" : "false");
      json += ",\"finishMs\":" + String(rcPlayers[i].finishTimeMs);
      json += "}";
    }
    json += "]";
  }
  
  // Tournament data
  if (currentMode == MODE_TOURNAMENT) {
    json += ",\"tbWinner\":" + String(tbWinner);
    json += ",\"tbRounds\":" + String(settings.tbRounds);
    json += ",\"tbGame\":" + String(tbGameMode);
    json += ",\"tbBattle\":" + String(tbBattleMode ? "true" : "false");
    json += ",\"tbNum\":" + String(tbNumTeams);
    json += ",\"tbTeams\":[";
    for (int t = 0; t < (int)tbNumTeams; t++) {
      if (t > 0) json += ",";
      json += "{\"name\":\"" + String(tbTeam[t].name) + "\"";
      json += ",\"gm\":" + String(tbTeam[t].gameMode);
      json += ",\"rnd\":" + String(tbRoundsForTeam(t));
      json += ",\"count\":" + String(tbTeam[t].count);
      json += ",\"hits\":" + String(tbTeam[t].hits);
      json += ",\"misses\":" + String(tbTeam[t].misses);
      json += ",\"score\":" + String(tbTeam[t].score);
      json += ",\"target\":" + String(tbTeam[t].curTarget >= 0 ? targets[tbTeam[t].curTarget].id : -1);
      json += ",\"done\":" + String(tbTeam[t].done ? "true" : "false");
      json += ",\"finishMs\":" + String(tbTeam[t].finishMs);
      json += ",\"rxWait\":" + String(tbTeam[t].rxWaiting ? "true" : "false");
      json += ",\"memPhase\":" + String(tbTeam[t].memPhase);
      json += ",\"memLen\":" + String(tbTeam[t].memLen);
      json += "}";
    }
    json += "]";
  }

  // Capture Points data
  if (currentMode == MODE_CAPTURE) {
    json += ",\"cpWinner\":" + String(cpWinner);
    json += ",\"cpHits\":" + String(cpHitsToCapture);
    json += ",\"cpTime\":" + String(settings.cpGameTime);
    json += ",\"cpRecap\":" + String(cpRecapture ? "true" : "false");
    json += ",\"cpLock\":" + String(cpLockTime);
    json += ",\"cpNum\":" + String(tbNumTeams);
    json += ",\"cpCap\":[";
    for (int t = 0; t < (int)tbNumTeams; t++) { if (t > 0) json += ","; json += String(cpCaptures[t]); }
    json += "],\"cpWin\":[";
    for (int t = 0; t < (int)tbNumTeams; t++) { if (t > 0) json += ","; json += String(cpWinCount[t]); }
    json += "],\"cpTeams\":[";
    for (int t = 0; t < (int)tbNumTeams; t++) {
      if (t > 0) json += ",";
      json += "{\"name\":\"" + String(tbTeam[t].name) + "\",\"count\":" + String(tbTeam[t].count) + "}";
    }
    json += "],\"cpTargets\":[";
    for (int i = 0; i < numTargets; i++) {
      if (i > 0) json += ",";
      json += "{\"o\":" + String(cpTargets[i].owner);
      json += ",\"cb\":" + String(cpTargets[i].capturedBy);
      json += ",\"h\":" + String(cpTargets[i].hitCount);
      json += ",\"c\":" + String(cpTargets[i].captured ? 1 : 0);
      json += ",\"lk\":" + String(cpTargets[i].locked ? 1 : 0);
      json += ",\"ca\":" + String(cpTargets[i].capturedAt > 0 ? (millis() - cpTargets[i].capturedAt) / 1000 : 0) + "}";
    }
    json += "]";
  }

  json += ",\"targets\":[";
  for (int i = 0; i < numTargets; i++) {
    if (i > 0) json += ",";
    json += "{\"id\":" + String(targets[i].id);
    json += ",\"active\":" + String(targets[i].active ? "true" : "false");
    json += ",\"hits\":" + String(targets[i].hits);
    json += ",\"shoot\":" + String(targets[i].isShoot ? "true" : "false");
    json += ",\"r\":" + String(targets[i].colorR);
    json += ",\"g\":" + String(targets[i].colorG);
    json += ",\"b\":" + String(targets[i].colorB);
    json += ",\"online\":" + String(targets[i].online ? "true" : "false");
    json += ",\"zone\":" + String(targetZone[i]);
    json += "}";
  }
  json += "]";
  
  json += ",\"top\":[";
  for (int i = 0; i < MAX_SCORES; i++) {
    if (i > 0) json += ",";
    // Ensure null-terminated strings for JSON safety
    topScores[i].name[15] = '\0';
    topScores[i].mode[15] = '\0';
    json += "{\"name\":\"" + String(topScores[i].name) + "\"";
    json += ",\"mode\":\"" + String(topScores[i].mode) + "\"";
    json += ",\"score\":" + String(topScores[i].score);
    json += ",\"hits\":" + String(topScores[i].hits);
    json += ",\"time\":" + String(topScores[i].timeMs);
    json += "}";
  }
  json += "]";
  
  // Zone info
  json += ",\"zone\":" + String(z);
  json += ",\"numZones\":" + String(numZones);
  json += ",\"zones\":[";
  for (int zi = 0; zi < numZones; zi++) {
    if (zi > 0) json += ",";
    json += "{\"name\":\"" + String(zones[zi].name) + "\"";
    json += ",\"active\":" + String(zones[zi].active ? "true" : "false");
    json += ",\"state\":" + String((int)zones[zi].state);
    json += ",\"mode\":" + String((int)zones[zi].mode);
    json += ",\"tgts\":" + String(zones[zi].tgtCount);
    json += "}";
  }
  json += "]";

  // Turn-based (toerbeurt) data
  json += ",\"turnMode\":" + String(turnMode ? "true" : "false");
  if (turnMode) {
    json += ",\"turnCur\":" + String(turnCurrentPlayer);
    json += ",\"turnCount\":" + String(turnPlayerCount);
    json += ",\"turnAuto\":" + String(turnAutoNext ? "true" : "false");
    json += ",\"turnCd\":" + String(turnCountdownSec);
    json += ",\"turnWaitMs\":" + String(gameState == STATE_TURN_WAIT ? (millis() - turnWaitStart) : 0);
    json += ",\"turnPlayers\":[";
    for (int i = 0; i < turnPlayerCount; i++) {
      if (i > 0) json += ",";
      json += "{\"name\":\"" + String(turnPlayers[i].name) + "\"";
      json += ",\"score\":" + String(turnPlayers[i].score);
      json += ",\"hits\":" + String(turnPlayers[i].hits);
      json += ",\"misses\":" + String(turnPlayers[i].misses);
      json += ",\"time\":" + String(turnPlayers[i].timeMs);
      json += ",\"done\":" + String(turnPlayers[i].done ? "true" : "false");
      json += "}";
    }
    json += "]";
  }

  json += "}";

  // Restore previous zone's globals so pending actions aren't polluted
  if (numZones > 1 && prevZone != z) {
    loadZone(prevZone);
  }

  server.send(200, "application/json", json);
}

void handleStart() {
  uint8_t z = server.hasArg("z") ? constrain(server.arg("z").toInt(), 0, MAX_ZONES-1) : 0;
  loadZone(z);
  
  // Parse zone target assignments if provided
  if (server.hasArg("zt")) {
    String zt = server.arg("zt");
    zones[z].tgtCount = 0;
    int start = 0;
    while (start < (int)zt.length() && zones[z].tgtCount < MAX_TARGETS) {
      int comma = zt.indexOf(',', start);
      if (comma < 0) comma = zt.length();
      int tid = zt.substring(start, comma).toInt();
      int idx = findTargetByID(tid);
      if (idx >= 0) {
        zones[z].tgtIdx[zones[z].tgtCount++] = idx;
        targetZone[idx] = z;
      }
      start = comma + 1;
    }
  }
  
  // Parse player name
  if (server.hasArg("pn")) {
    String pn = server.arg("pn");
    if (pn.length() == 0) pn = "Speler";
    strncpy(playerName, pn.c_str(), 15);
    playerName[15] = '\0';
  }
  
  // Parse mode from request
  if (server.hasArg("m")) {
    currentMode = (GameMode)server.arg("m").toInt();
  }
  
  // Parse no-shoot (generiek, voor seq/rand/par/rx/ft)
  // nsp = percentage (0-50%), nsc = vast aantal (0-20)
  if (server.hasArg("nsp")) settings.noShootPct = constrain(server.arg("nsp").toInt(), 0, 50);
  else settings.noShootPct = 0;
  if (server.hasArg("nsc")) settings.noShootCount = constrain(server.arg("nsc").toInt(), 0, 20);
  else settings.noShootCount = 0;

  // Parse mode-specific settings
  switch (currentMode) {
    case MODE_FREEPLAY:
      if (server.hasArg("ft")) settings.fpGameTime = server.arg("ft").toInt();
      break;
      
    case MODE_SHOOTNOSHOOT:
      if (server.hasArg("dm")) settings.snsDarkMode = server.arg("dm").toInt() == 1;
      if (server.hasArg("om")) settings.snsOffMode = server.arg("om").toInt() == 1;
      if (server.hasArg("st")) settings.snsGameTime = server.arg("st").toInt();
      
      if (server.hasArg("snsr") && server.arg("snsr").toInt() == 1) {
        // RANDOM MODE: assign no-shoots randomly
        int noShootCount = 0;
        if (server.hasArg("snsns")) noShootCount = server.arg("snsns").toInt();
        
        // Eerst alles shoot (alleen zone targets)
        for (int i = 0; i < numTargets; i++) {
          if (!isMyTarget(i)) continue;
          targets[i].isShoot = true;
        }
        
        // Bouw lijst van online targets (alleen zone)
        int onlineList[MAX_TARGETS];
        int onlineN = 0;
        for (int i = 0; i < numTargets; i++) {
          if (!isMyTarget(i)) continue;
          if (targets[i].online) onlineList[onlineN++] = i;
        }
        
        // Clamp no-shoot count
        if (noShootCount > onlineN) noShootCount = onlineN;
        if (noShootCount >= onlineN) noShootCount = onlineN - 1;  // minstens 1 shoot
        if (noShootCount < 0) noShootCount = 0;
        
        // Fisher-Yates shuffle, pak eerste N als no-shoot
        for (int i = onlineN - 1; i > 0; i--) {
          int j = random(0, i + 1);
          int tmp = onlineList[i];
          onlineList[i] = onlineList[j];
          onlineList[j] = tmp;
        }
        for (int i = 0; i < noShootCount; i++) {
          targets[onlineList[i]].isShoot = false;
        }
        
        Serial.printf("SNS RANDOM: %d no-shoot van %d online\n", noShootCount, onlineN);
      } else {
        // HANDMATIG MODE: parse per target ID
        for (int i = 0; i < numTargets; i++) {
          if (!isMyTarget(i)) continue;
          String key = "t" + String(targets[i].id);
          if (server.hasArg(key)) {
            targets[i].isShoot = server.arg(key).toInt() == 1;
          }
        }
      }
      break;
      
    case MODE_SEQUENCE:
      if (server.hasArg("tpt")) settings.seqTimePerTarget = server.arg("tpt").toInt();
      if (server.hasArg("sr"))  settings.seqRounds = server.arg("sr").toInt();
      if (server.hasArg("sdn")) settings.seqDelayNext = server.arg("sdn").toInt();
      if (server.hasArg("sho")) settings.seqHitsToOff = constrain(server.arg("sho").toInt(), 1, 10);
      break;
      
    case MODE_RANDOM:
      if (server.hasArg("rmin")) settings.randMinTime = server.arg("rmin").toInt();
      if (server.hasArg("rmax")) settings.randMaxTime = server.arg("rmax").toInt();
      if (server.hasArg("rr"))   settings.randRounds = server.arg("rr").toInt();
      // Sanity check: min <= max
      if (settings.randMinTime > settings.randMaxTime) {
        uint16_t tmp = settings.randMinTime;
        settings.randMinTime = settings.randMaxTime;
        settings.randMaxTime = tmp;
      }
      break;
      
    case MODE_MEMORY:
      if (server.hasArg("ml")) settings.memLength = constrain(server.arg("ml").toInt(), 2, MEM_MAX_LENGTH);
      if (server.hasArg("md")) settings.memDisplayTime = constrain(server.arg("md").toInt(), 300, 5000);
      if (server.hasArg("mw")) settings.memMaxWrong = constrain(server.arg("mw").toInt(), 1, 10);
      break;
      
    case MODE_RANDOMCOLOR:
      if (server.hasArg("rcc")) rcPlayerCount = constrain(server.arg("rcc").toInt(), 2, RC_MAX_PLAYERS);
      if (server.hasArg("rcn1")) server.arg("rcn1").toCharArray(rcPlayers[0].name, 16);
      if (server.hasArg("rcn2")) server.arg("rcn2").toCharArray(rcPlayers[1].name, 16);
      if (server.hasArg("rcn3")) server.arg("rcn3").toCharArray(rcPlayers[2].name, 16);
      if (server.hasArg("rcn4")) server.arg("rcn4").toCharArray(rcPlayers[3].name, 16);
      if (server.hasArg("rcr")) settings.rcRounds = constrain(server.arg("rcr").toInt(), 5, 60);
      if (server.hasArg("rcd")) settings.rcDisplayTime = constrain(server.arg("rcd").toInt(), 500, 10000);
      if (server.hasArg("rcp")) settings.rcPauseTime = constrain(server.arg("rcp").toInt(), 300, 5000);
      break;
      
    case MODE_REACTION:
      if (server.hasArg("rxc")) rcPlayerCount = constrain(server.arg("rxc").toInt(), 1, RC_MAX_PLAYERS);
      if (server.hasArg("rxn1")) server.arg("rxn1").toCharArray(rcPlayers[0].name, 16);
      if (server.hasArg("rxn2")) server.arg("rxn2").toCharArray(rcPlayers[1].name, 16);
      if (server.hasArg("rxn3")) server.arg("rxn3").toCharArray(rcPlayers[2].name, 16);
      if (server.hasArg("rxn4")) server.arg("rxn4").toCharArray(rcPlayers[3].name, 16);
      if (server.hasArg("rxr")) settings.rxRounds = constrain(server.arg("rxr").toInt(), 1, 10);
      if (server.hasArg("rxdmin")) settings.rxDelayMin = constrain(server.arg("rxdmin").toInt(), 500, 5000);
      if (server.hasArg("rxdmax")) settings.rxDelayMax = constrain(server.arg("rxdmax").toInt(), 1000, 8000);
      if (server.hasArg("rxf")) settings.rxFixed = server.arg("rxf").toInt() == 1;
      if (settings.rxDelayMin > settings.rxDelayMax) {
        uint16_t tmp = settings.rxDelayMin;
        settings.rxDelayMin = settings.rxDelayMax;
        settings.rxDelayMax = tmp;
      }
      break;
      
    case MODE_PARCOURS:
      if (server.hasArg("prf")) settings.parRandom = server.arg("prf").toInt() == 1;
      if (server.hasArg("pdn")) settings.parDelayNext = server.arg("pdn").toInt();
      if (server.hasArg("pao")) settings.parAutoOff = server.arg("pao").toInt();
      if (server.hasArg("pho")) settings.parHitsToOff = constrain(server.arg("pho").toInt(), 1, 10);
      // Custom route: "pro=3,1,5,2" = target IDs in order
      parCustomCount = 0;
      if (server.hasArg("pro")) {
        String pro = server.arg("pro");
        int start = 0;
        while (start < (int)pro.length() && parCustomCount < PAR_MAX_TARGETS) {
          int comma = pro.indexOf(',', start);
          if (comma < 0) comma = pro.length();
          int tid = pro.substring(start, comma).toInt();
          if (tid > 0) parCustomIds[parCustomCount++] = tid;
          start = comma + 1;
        }
        Serial.printf("PARCOURS: Custom route %d targets: ", parCustomCount);
        for (int i = 0; i < parCustomCount; i++) Serial.printf("T%d ", parCustomIds[i]);
        Serial.println();
      }
      break;
      
    case MODE_FASTTRACK:
      if (server.hasArg("ftc")) rcPlayerCount = constrain(server.arg("ftc").toInt(), 2, RC_MAX_PLAYERS);
      if (server.hasArg("ftn1")) server.arg("ftn1").toCharArray(rcPlayers[0].name, 16);
      if (server.hasArg("ftn2")) server.arg("ftn2").toCharArray(rcPlayers[1].name, 16);
      if (server.hasArg("ftn3")) server.arg("ftn3").toCharArray(rcPlayers[2].name, 16);
      if (server.hasArg("ftn4")) server.arg("ftn4").toCharArray(rcPlayers[3].name, 16);
      if (server.hasArg("ftt")) settings.ftTargetsPerPlayer = constrain(server.arg("ftt").toInt(), 3, 50);
      if (server.hasArg("ftf")) settings.ftFixed = server.arg("ftf").toInt() == 1;
      for (int i = 0; i < RC_MAX_PLAYERS; i++) rcPlayers[i].active = (i < rcPlayerCount);
      break;
      
    case MODE_TOURNAMENT:
      if (server.hasArg("tbr")) settings.tbRounds = constrain(server.arg("tbr").toInt(), 3, 50);
      if (server.hasArg("tbgm")) tbGameMode = constrain(server.arg("tbgm").toInt(), 0, 7);
      if (server.hasArg("tbb")) tbBattleMode = server.arg("tbb").toInt() == 1;
      if (server.hasArg("tbn")) tbNumTeams = constrain(server.arg("tbn").toInt(), 2, 4);
      // Per-team game modes + rounds: tbgm0..3, tbr0..3 (fallback to global)
      for (int t = 0; t < 4; t++) {
        String gmKey = "tbgm" + String(t);
        if (server.hasArg(gmKey)) {
          tbTeam[t].gameMode = constrain(server.arg(gmKey).toInt(), 0, 7);
        } else {
          tbTeam[t].gameMode = tbGameMode;  // fallback naar globale mode
        }
        String rrKey = "tbr" + String(t);
        if (server.hasArg(rrKey)) {
          tbTeam[t].rounds = constrain(server.arg(rrKey).toInt(), 3, 50);
        } else {
          tbTeam[t].rounds = settings.tbRounds;  // fallback naar globale rondes
        }
      }
      // Team names: tbn0, tbn1, tbn2, tbn3
      for (int t = 0; t < 4; t++) {
        String key = "tbn" + String(t);
        if (server.hasArg(key)) server.arg(key).toCharArray(tbTeam[t].name, 16);
      }
      // Parse group assignments: "tbg=0,-1,0,1,2,3" (per target: -1=none, 0-3=team)
      for (int i = 0; i < 20; i++) tbTargetTeam[i] = -1;
      if (server.hasArg("tbg")) {
        String g = server.arg("tbg");
        int start = 0; int ti = 0;
        while (start < (int)g.length() && ti < 20) {
          int comma = g.indexOf(',', start);
          if (comma < 0) comma = g.length();
          tbTargetTeam[ti++] = g.substring(start, comma).toInt();
          start = comma + 1;
        }
      }
      // Fallback: auto-populate from zone assignments if tbg was missing/empty
      {
        bool anyAssigned = false;
        for (int i = 0; i < numTargets; i++) { if (tbTargetTeam[i] >= 0) { anyAssigned = true; break; } }
        if (!anyAssigned && numZones > 1) {
          for (int i = 0; i < numTargets; i++) {
            tbTargetTeam[i] = targetZone[i];  // zone 0 = team 0, zone 1 = team 1, etc.
          }
          Serial.println("TOURNAMENT: auto-populated teams from zone assignments");
        }
      }
      // Tournament always runs globally in zone 0
      // Reset ALL other zones to freeplay (prevent ghost tournament state)
      for (int oz = 1; oz < MAX_ZONES; oz++) {
        zones[oz].state = STATE_IDLE;
        zones[oz].mode = MODE_FREEPLAY;
      }
      z = 0;
      activeZone = 0;
      // Reload zone 0's own settings so tournament doesn't inherit another zone's config
      settings = zones[0].cfg;
      // Re-apply tournament-specific settings parsed above
      currentMode = MODE_TOURNAMENT;
      break;

    case MODE_CAPTURE:
      if (server.hasArg("cph"))  settings.cpHitsToCapture = constrain(server.arg("cph").toInt(), 1, 100);
      if (server.hasArg("cpgt")) settings.cpGameTime = constrain(server.arg("cpgt").toInt(), 0, 30);
      if (server.hasArg("cpr"))  settings.cpRecapture = server.arg("cpr").toInt() == 1;
      if (server.hasArg("cplt")) settings.cpLockTime = constrain(server.arg("cplt").toInt(), 0, 10);
      if (server.hasArg("tbn"))  tbNumTeams = constrain(server.arg("tbn").toInt(), 2, 4);
      // Per-team win counts
      for (int t = 0; t < 4; t++) {
        String key = "cpw" + String(t);
        if (server.hasArg(key)) cpWinCount[t] = constrain(server.arg(key).toInt(), 1, 20);
        else cpWinCount[t] = 3;
      }
      // Team names
      for (int t = 0; t < 4; t++) {
        String key = "tbn" + String(t);
        if (server.hasArg(key)) server.arg(key).toCharArray(tbTeam[t].name, 16);
      }
      // Target assignments (reuse tbg format)
      for (int i = 0; i < 20; i++) tbTargetTeam[i] = -1;
      if (server.hasArg("tbg")) {
        String g = server.arg("tbg");
        int start = 0; int ti = 0;
        while (start < (int)g.length() && ti < 20) {
          int comma = g.indexOf(',', start);
          if (comma < 0) comma = g.length();
          tbTargetTeam[ti++] = g.substring(start, comma).toInt();
          start = comma + 1;
        }
      }
      // Auto-populate from zones if needed
      {
        bool anyAssigned = false;
        for (int i = 0; i < numTargets; i++) { if (tbTargetTeam[i] >= 0) { anyAssigned = true; break; } }
        if (!anyAssigned && numZones > 1) {
          for (int i = 0; i < numTargets; i++) tbTargetTeam[i] = targetZone[i];
          Serial.println("CAPTURE: auto-populated teams from zone assignments");
        }
      }
      // Capture runs globally in zone 0
      for (int oz = 1; oz < MAX_ZONES; oz++) { zones[oz].state = STATE_IDLE; zones[oz].mode = MODE_FREEPLAY; }
      z = 0; activeZone = 0;
      settings = zones[0].cfg;
      currentMode = MODE_CAPTURE;
      break;

    default: break;
  }

  // Parse turn-based (toerbeurt) parameters
  if (server.hasArg("trn") && server.arg("trn").toInt() == 1) {
    turnMode = true;
    turnPlayerCount = constrain(server.hasArg("trnc") ? server.arg("trnc").toInt() : 0, 1, TURN_MAX_PLAYERS);
    turnCurrentPlayer = 0;
    turnAutoNext = server.hasArg("trna") && server.arg("trna").toInt() == 1;
    turnCountdownSec = server.hasArg("trncd") ? constrain(server.arg("trncd").toInt(), 3, 15) : 5;
    turnGlobal = server.hasArg("trng") && server.arg("trng").toInt() == 1;
    for (int i = 0; i < TURN_MAX_PLAYERS; i++) {
      turnPlayers[i].done = false;
      turnPlayers[i].score = 0;
      turnPlayers[i].hits = 0;
      turnPlayers[i].misses = 0;
      turnPlayers[i].timeMs = 0;
      String key = "trnp" + String(i + 1);
      if (server.hasArg(key)) {
        server.arg(key).toCharArray(turnPlayers[i].name, 20);
      } else {
        snprintf(turnPlayers[i].name, 20, "Speler %d", i + 1);
      }
    }
    // Zet spelernaam op huidige turn-speler
    strncpy(playerName, turnPlayers[0].name, 15);
    playerName[15] = '\0';
    turnSavedMode = currentMode;
    Serial.printf("TURN MODE: %d spelers, auto=%d, cd=%ds\n", turnPlayerCount, turnAutoNext, turnCountdownSec);
  } else if (!server.hasArg("_nextturn")) {
    // Alleen reset als het geen nextturn herstart is
    turnMode = false;
  }

  startGame();
  saveZone(z);
  server.send(200, "text/plain", "OK");
}

void handleStop() {
  uint8_t z = server.hasArg("z") ? constrain(server.arg("z").toInt(), 0, MAX_ZONES-1) : 0;
  if ((zones[0].mode == MODE_TOURNAMENT || zones[0].mode == MODE_CAPTURE) && zones[0].state >= STATE_COUNTDOWN) z = 0;
  loadZone(z);
  stopGame();
  saveZone(z);
  server.send(200, "text/plain", "OK");
}

void handlePause() {
  uint8_t z = server.hasArg("z") ? constrain(server.arg("z").toInt(), 0, MAX_ZONES-1) : 0;
  if ((zones[0].mode == MODE_TOURNAMENT || zones[0].mode == MODE_CAPTURE) && zones[0].state >= STATE_COUNTDOWN) z = 0;
  loadZone(z);
  pauseGame();
  saveZone(z);
  server.send(200, "text/plain", "OK");
}

void handleReset() {
  uint8_t z = server.hasArg("z") ? constrain(server.arg("z").toInt(), 0, MAX_ZONES-1) : 0;
  if ((zones[0].mode == MODE_TOURNAMENT || zones[0].mode == MODE_CAPTURE) && zones[0].state >= STATE_COUNTDOWN) z = 0;
  loadZone(z);
  resetGame();
  saveZone(z);
  server.send(200, "text/plain", "OK");
}

void handleNextTurn() {
  if (!turnMode || turnCurrentPlayer >= turnPlayerCount) {
    server.send(400, "text/plain", "No turn mode active");
    return;
  }
  if (gameState != STATE_TURN_WAIT) {
    server.send(400, "text/plain", "Not in turn-wait state");
    return;
  }

  uint8_t z = server.hasArg("z") ? constrain(server.arg("z").toInt(), 0, MAX_ZONES-1) : 0;
  loadZone(z);

  // Zet spelernaam op huidige turn-speler
  strncpy(playerName, turnPlayers[turnCurrentPlayer].name, 15);
  playerName[15] = '\0';

  // Reset game state voor nieuwe beurt, behoud settings
  currentScore = 0;
  totalHits = 0;
  totalMisses = 0;
  totalPausedMs = 0;
  finalTimeMs = 0;
  currentMode = turnSavedMode;

  startGame();
  saveZone(z);
  Serial.printf("NEXTTURN: %s (%d/%d) start\n",
    turnPlayers[turnCurrentPlayer].name, turnCurrentPlayer + 1, turnPlayerCount);
  server.send(200, "text/plain", "OK");
}

void handleSetMode() {
  uint8_t z = server.hasArg("z") ? constrain(server.arg("z").toInt(), 0, MAX_ZONES-1) : 0;
  loadZone(z);
  if (server.hasArg("m")) {
    currentMode = (GameMode)server.arg("m").toInt();
    Serial.printf("ZONE %c MODE=%d\n", 'A'+z, currentMode);
  }
  saveZone(z);
  server.send(200, "text/plain", "OK");
}

void handleThreshold() {
  if (server.hasArg("v")) {
    uint16_t thr = server.arg("v").toInt();
    if (thr < 10) thr = 10;
    if (thr > 1000) thr = 1000;
    // Apply threshold to ALL zones so it stays consistent
    for (int z = 0; z < MAX_ZONES; z++) {
      zones[z].cfg.piezoThreshold = thr;
    }
    settings.piezoThreshold = thr;
    broadcastThreshold(thr);
    Serial.printf("THRESHOLD=%d (all zones)\n", thr);
  }
  server.send(200, "text/plain", "OK");
}

// ---- Quick Start/Stop for physical button ----
// Uses current mode + settings (set via UI beforehand)
// Optional: ?cd=3 for countdown, ?z=0 for zone
void handleQuickStart() {
  uint8_t z = server.hasArg("z") ? constrain(server.arg("z").toInt(), 0, MAX_ZONES-1) : 0;
  loadZone(z);
  
  // If game already running, ignore
  if (gameState == STATE_RUNNING || gameState == STATE_COUNTDOWN) {
    saveZone(z);
    server.send(200, "text/plain", "ALREADY_RUNNING");
    return;
  }
  
  // Reset to idle if needed
  if (gameState == STATE_ENDED) {
    resetGame();
  }
  
  // For multiplayer modes: restore default colors (zone load may have zeroed them)
  if (currentMode == MODE_RANDOMCOLOR || currentMode == MODE_REACTION || currentMode == MODE_FASTTRACK) {
    for (int i = 0; i < RC_MAX_PLAYERS; i++) {
      rcPlayers[i].r = playerColorR[i];
      rcPlayers[i].g = playerColorG[i];
      rcPlayers[i].b = playerColorB[i];
    }
  }
  
  startGame();
  saveZone(z);
  Serial.println("QUICKSTART: Game started via physical button");
  server.send(200, "text/plain", "OK");
}

void handleQuickStop() {
  uint8_t z = server.hasArg("z") ? constrain(server.arg("z").toInt(), 0, MAX_ZONES-1) : 0;
  if ((zones[0].mode == MODE_TOURNAMENT || zones[0].mode == MODE_CAPTURE) && zones[0].state >= STATE_COUNTDOWN) z = 0;
  loadZone(z);
  stopGame();
  saveZone(z);
  Serial.println("QUICKSTOP: Game stopped via physical button");
  server.send(200, "text/plain", "OK");
}

void handleLocate() {
  int tid = server.arg("t").toInt();
  int idx = findTargetByID(tid);
  if (idx >= 0 && targets[idx].online) {
    sendFlash(idx, 255, 255, 255, 5);
    schedulePending(300, ACT_FLASH, idx, 255, 255, 255, 5);
    schedulePending(600, ACT_FLASH, idx, 255, 255, 255, 5);
    sendBuzz(idx, 3);
    Serial.printf("LOCATE: T%d flash+buzz\n", tid);
  }
  server.send(200, "text/plain", "OK");
}

void handleActivate() {
  if (server.hasArg("t")) {
    int t = server.arg("t").toInt();
    if (t == 0) {
      sendAllOff();
    } else {
      int idx = findTargetByID(t);
      if (idx >= 0) {
        // Check color parameter
        String c = server.hasArg("c") ? server.arg("c") : "r";
        if (c == "off") {
          sendLightOff(idx);
        } else if (c == "g") {
          sendLightOn(idx, 0, 255, 0);
        } else if (c == "b") {
          sendLightOn(idx, 0, 0, 255);
        } else {
          sendLightOn(idx, 255, 0, 0);  // default rood
        }
      }
    }
  }
  server.send(200, "text/plain", "OK");
}

// Per-team tournament control
void handleTbControl() {
  loadZone(0);  // Tournament always runs in zone 0
  if (currentMode != MODE_TOURNAMENT || gameState != STATE_RUNNING) {
    server.send(400, "text/plain", "NOT_RUNNING");
    return;
  }
  int t = server.hasArg("t") ? server.arg("t").toInt() : -1;
  String a = server.hasArg("a") ? server.arg("a") : "";
  if (t < 0 || t >= (int)tbNumTeams) {
    server.send(400, "text/plain", "INVALID_TEAM");
    return;
  }
  
  if (a == "stop") {
    // Stop this team — mark as done
    if (!tbTeam[t].done) {
      tbTeam[t].done = true;
      tbTeam[t].finishMs = millis() - gameStartTime - totalPausedMs;
      // Turn off this team's targets
      for (int i = 0; i < tbTeam[t].count; i++) {
        sendLightOff(tbTeam[t].tgts[i]);
      }
      tbTeam[t].curTarget = -1;
      Serial.printf("TB CONTROL: Team %d STOPPED at %dms\n", t, tbTeam[t].finishMs);
      
      // Check if ALL teams are done → end game
      bool allDone = true;
      for (int i = 0; i < (int)tbNumTeams; i++) {
        if (tbTeam[i].count > 0 && !tbTeam[i].done) { allDone = false; break; }
      }
      if (allDone) endGame();
    }
  }
  else if (a == "reset") {
    // Reset this team's scores but keep playing
    tbTeam[t].hits = 0;
    tbTeam[t].misses = 0;
    tbTeam[t].score = 0;
    tbTeam[t].finishMs = 0;
    tbTeam[t].done = false;
    // Pick a new target for them
    if (tbTeam[t].count > 0) {
      int next = tbPickRandom(t);
      tbTeam[t].curTarget = next;
      if (next >= 0) tbLightTarget(t, next);
    }
    Serial.printf("TB CONTROL: Team %d RESET\n", t);
  }
  else if (a == "restart") {
    // Restart a stopped team
    if (tbTeam[t].done && tbTeam[t].count > 0) {
      tbTeam[t].done = false;
      tbTeam[t].hits = 0;
      tbTeam[t].misses = 0;
      tbTeam[t].score = 0;
      tbTeam[t].finishMs = 0;
      int next = tbPickRandom(t);
      tbTeam[t].curTarget = next;
      if (next >= 0) tbLightTarget(t, next);
      Serial.printf("TB CONTROL: Team %d RESTARTED\n", t);
    }
  }
  
  saveZone(0);  // Tournament always in zone 0
  server.send(200, "text/plain", "OK");
}

void handleSaveScore() {
  uint8_t z = server.hasArg("z") ? constrain(server.arg("z").toInt(), 0, MAX_ZONES-1) : 0;
  if ((zones[0].mode == MODE_TOURNAMENT || zones[0].mode == MODE_CAPTURE) && zones[0].state >= STATE_COUNTDOWN) z = 0;
  loadZone(z);
  if (server.hasArg("name")) {
    String name = server.arg("name");
    if (name.length() == 0) name = "Anon";
    uint32_t elapsed = getElapsed();
    tryInsertScore(name.c_str(), currentScore, totalHits, elapsed, getModeName(currentMode));
    Serial.printf("SAVED: %s %d %dms (zone %d)\n", name.c_str(), currentScore, elapsed, z);
  }
  server.send(200, "text/plain", "OK");
}

void handleResetScores() {
  for (int i = 0; i < MAX_SCORES; i++) {
    strcpy(topScores[i].name, "---");
    strcpy(topScores[i].mode, "");
    topScores[i].score = 0;
    topScores[i].hits = 0;
    topScores[i].timeMs = 0;
  }
  saveScores();
  server.send(200, "text/plain", "OK");
}

// ============================================================
// TARGET OTA - Send target into update mode
// ============================================================

void handleNFCPage() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>"
    "<title>NFC Tag Setup</title>"
    "<style>"
    "body{background:#111;color:#eee;font-family:Arial;max-width:500px;margin:0 auto;padding:16px}"
    "h1{color:#DC0000;text-align:center;font-size:1.2em}"
    "h2{color:#DC0000;font-size:.95em;margin-top:20px}"
    ".card{background:#1a1a1a;border-radius:12px;padding:14px;margin:10px 0}"
    "input{width:100%;padding:10px;background:#222;border:1px solid #444;color:#fff;border-radius:8px;font-size:1em;box-sizing:border-box;margin:6px 0}"
    ".url{background:#0a0a0a;border:1px solid #333;border-radius:8px;padding:10px;font-family:monospace;font-size:.85em;word-break:break-all;margin:8px 0;color:#4CAF50;cursor:pointer}"
    ".url:active{background:#1a3a1a}"
    "button{background:#DC0000;color:#fff;border:none;padding:10px 20px;border-radius:8px;font-size:.95em;cursor:pointer;width:100%;margin:6px 0}"
    "button:active{background:#a00}"
    ".step{display:flex;gap:10px;align-items:flex-start;margin:8px 0}"
    ".step-n{background:#DC0000;color:#fff;border-radius:50%;min-width:24px;height:24px;display:flex;align-items:center;justify-content:center;font-weight:bold;font-size:.8em}"
    ".step-t{font-size:.9em;line-height:1.4}"
    ".back{display:block;text-align:center;color:#888;margin-top:16px;text-decoration:none;font-size:.9em}"
    "</style></head><body>"
    "<h1>&#x1F4F1; NFC Tag Setup</h1>"
    "<div class='card'>"
    "<h2>&#x1F3AF; Naam op tag</h2>"
    "<input type='text' id='nfcName' placeholder='Speler naam...' maxlength='15' oninput='genURL()'>"
    "<div style='margin-top:8px;font-size:.8em;color:#888'>URL voor op de tag:</div>"
    "<div class='url' id='nfcURL' onclick='copyURL()'>http://192.168.4.1/?player=</div>"
    "<button onclick='copyURL()'>&#x1F4CB; Kopieer URL</button>"
    "<div id='copied' style='text-align:center;color:#4CAF50;font-size:.8em;height:18px'></div>"
    "</div>"
    
    "<div class='card'>"
    "<h2>&#x1F4DD; Hoe schrijf je de tag?</h2>"
    
    "<div style='font-weight:bold;color:#DC0000;margin:10px 0 6px'>NFC Tools App (iPhone/Android):</div>"
    "<div class='step'><div class='step-n'>1</div><div class='step-t'>Download <b>NFC Tools</b> (gratis)</div></div>"
    "<div class='step'><div class='step-n'>2</div><div class='step-t'>Tap <b>Write</b> → <b>Add a record</b> → <b>URL</b></div></div>"
    "<div class='step'><div class='step-n'>3</div><div class='step-t'>Plak de URL van hierboven</div></div>"
    "<div class='step'><div class='step-n'>4</div><div class='step-t'>Tap <b>Write</b> → Houd NTAG215 tegen telefoon</div></div>"
    
    "<div style='font-weight:bold;color:#DC0000;margin:14px 0 6px'>Flipper Zero:</div>"
    "<div class='step'><div class='step-n'>1</div><div class='step-t'><b>NFC</b> → <b>Saved</b> → maak een .nfc bestand</div></div>"
    "<div class='step'><div class='step-n'>2</div><div class='step-t'>Of gebruik de Flipper Lab web app om NDEF URL te schrijven</div></div>"
    "</div>"
    
    "<div class='card'>"
    "<h2>&#x2705; Testen</h2>"
    "<div class='step'><div class='step-n'>1</div><div class='step-t'>Verbind met WiFi: <b>" AP_SSID "</b></div></div>"
    "<div class='step'><div class='step-n'>2</div><div class='step-t'>Houd tag tegen telefoon</div></div>"
    "<div class='step'><div class='step-n'>3</div><div class='step-t'>Browser opent automatisch met naam ingevuld</div></div>"
    "<div class='step'><div class='step-n'>4</div><div class='step-t'>Groene melding: <b>\"Naam ingeladen via NFC\"</b></div></div>"
    "</div>"
    
    "<script>"
    "function genURL(){"
    "  var n=document.getElementById('nfcName').value;"
    "  var u='http://192.168.4.1/?player='+encodeURIComponent(n);"
    "  document.getElementById('nfcURL').textContent=u;"
    "}"
    "function copyURL(){"
    "  var u=document.getElementById('nfcURL').textContent;"
    "  if(navigator.clipboard){"
    "    navigator.clipboard.writeText(u).then(function(){show()});"
    "  }else{"
    "    var ta=document.createElement('textarea');ta.value=u;document.body.appendChild(ta);ta.select();document.execCommand('copy');ta.remove();show();"
    "  }"
    "  function show(){document.getElementById('copied').textContent='\\u2705 Gekopieerd!';setTimeout(function(){document.getElementById('copied').textContent='';},2000);}"
    "}"
    "</script>"
    "<a class='back' href='/'>&#x2190; Terug naar control panel</a>"
    "</body></html>";
  server.send(200, "text/html", html);
}

void handleTargetOTA() {
  if (!server.hasArg("t")) {
    server.send(400, "text/plain", "Missing target");
    return;
  }
  int t = server.arg("t").toInt();
  int idx = findTargetByID(t);
  if (idx < 0) {
    server.send(400, "text/plain", "Invalid target");
    return;
  }
  if (!targets[idx].online) {
    server.send(400, "text/plain", "Target offline");
    return;
  }
  sendToTarget(idx, MSG_OTA_MODE, 0, 0, 0, 0, 0);
  Serial.printf("OTA MODE sent to Target %d\n", t);
  
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "RTT-TARGET-%d", t);
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>"
    "<title>Target " + String(t) + " Update</title>"
    "<style>"
    "body{font-family:'Segoe UI',sans-serif;background:#000;color:#fff;padding:20px;text-align:center}"
    "h1{color:#DC0000}"
    ".box{background:#1a1a1a;border:2px solid #DC0000;border-radius:10px;padding:20px;max-width:400px;margin:20px auto}"
    ".step{text-align:left;margin:10px 0;padding:10px;background:#222;border-radius:6px}"
    ".step b{color:#DC0000}"
    "a.back{background:#333;color:#fff;text-decoration:none;padding:10px 20px;border-radius:8px;display:inline-block;margin-top:15px}"
    "</style></head><body>"
    "<h1>&#x1F3AF; Target " + String(t) + " → Update Mode</h1>"
    "<div class='box'>"
    "<p style='color:#0f0;font-size:1.2em'>&#x2705; Commando verstuurd!</p>"
    "<p>Target " + String(t) + " knippert nu <b style='color:#b400ff'>paars</b></p>"
    "<div class='step'><b>Stap 1:</b> Verbind je telefoon/laptop met WiFi:<br>"
    "<span style='font-size:1.3em;color:#DC0000'>" + String(ssid) + "</span><br>"
    "Wachtwoord: <b>12345678</b></div>"
    "<div class='step'><b>Stap 2:</b> Open in je browser:<br>"
    "<span style='font-size:1.3em;color:#DC0000'>192.168.4.1</span></div>"
    "<div class='step'><b>Stap 3:</b> Upload het .bin bestand</div>"
    "<div class='step'><b>Stap 4:</b> Na de update herstart de target automatisch.<br>"
    "Verbind terug met <b>RAF RTT TRAINING SYSTEM</b></div>"
    "<p style='color:#888;font-size:.8em'>Timeout: 5 minuten, daarna herstart target automatisch</p>"
    "<a class='back' href='/'>&#x2190; Terug</a>"
    "</div></body></html>";
  server.send(200, "text/html", html);
}

// ============================================================
// OTA FIRMWARE UPDATE (MASTER)
// ============================================================

// ===== ZONE MANAGEMENT =====
void handleZones() {
  // GET: return zone config
  // POST with params: configure zones
  
  if (server.hasArg("n")) {
    // Set number of zones
    numZones = constrain(server.arg("n").toInt(), 1, MAX_ZONES);
    for (int i = 0; i < MAX_ZONES; i++) {
      zones[i].active = (i < numZones);
    }
    Serial.printf("ZONES: %d active\n", numZones);
  }
  
  // Zone names: zn0=ZoneA&zn1=ZoneB
  for (int z = 0; z < MAX_ZONES; z++) {
    String key = "zn" + String(z);
    if (server.hasArg(key)) {
      strncpy(zones[z].name, server.arg(key).c_str(), 15);
      zones[z].name[15] = '\0';
    }
  }
  
  // Block zone changes while any zone has an active game
  bool anyGameActive = false;
  for (int z = 0; z < numZones; z++) {
    if (zones[z].state == STATE_RUNNING || zones[z].state == STATE_COUNTDOWN || zones[z].state == STATE_PAUSED) {
      anyGameActive = true;
      break;
    }
  }
  // Also check single-zone mode
  if (numZones <= 1 && (gameState == STATE_RUNNING || gameState == STATE_COUNTDOWN || gameState == STATE_PAUSED)) {
    anyGameActive = true;
  }

  // Target assignments: zt0=1,2,3&zt1=4,5,6
  bool hasZtArg = false;
  for (int z = 0; z < MAX_ZONES; z++) {
    if (server.hasArg("zt" + String(z))) { hasZtArg = true; break; }
  }
  if (hasZtArg && anyGameActive) {
    server.send(409, "text/plain", "GAME_ACTIVE");
    return;
  }
  if (hasZtArg) {
    // Clear ALL targetZone entries first to prevent stale assignments
    for (int i = 0; i < MAX_TARGETS; i++) targetZone[i] = -1;
    for (int z = 0; z < MAX_ZONES; z++) zones[z].tgtCount = 0;

    for (int z = 0; z < MAX_ZONES; z++) {
      String key = "zt" + String(z);
      if (server.hasArg(key)) {
        String val = server.arg(key);
        if (val.length() == 0) continue;
        int start = 0;
        while (start < (int)val.length() && zones[z].tgtCount < MAX_TARGETS) {
          int comma = val.indexOf(',', start);
          if (comma < 0) comma = val.length();
          int tid = val.substring(start, comma).toInt();
          int idx = findTargetByID(tid);
          if (idx >= 0 && targetZone[idx] < 0) {  // prevent duplicate assignment
            zones[z].tgtIdx[zones[z].tgtCount++] = idx;
            targetZone[idx] = z;
          }
          start = comma + 1;
        }
      }
    }
  }

  // Manual move: move single target to zone (/zones?move=targetId&to=zoneIdx)
  if (server.hasArg("move") && server.hasArg("to") && !anyGameActive) {
    int tid = server.arg("move").toInt();
    int toZ = constrain(server.arg("to").toInt(), -1, numZones - 1);
    int idx = findTargetByID(tid);
    if (idx >= 0) {
      // Remove from current zone
      int oldZ = targetZone[idx];
      if (oldZ >= 0 && oldZ < MAX_ZONES) {
        for (int i = 0; i < zones[oldZ].tgtCount; i++) {
          if (zones[oldZ].tgtIdx[i] == idx) {
            for (int j = i; j < zones[oldZ].tgtCount - 1; j++)
              zones[oldZ].tgtIdx[j] = zones[oldZ].tgtIdx[j + 1];
            zones[oldZ].tgtCount--;
            break;
          }
        }
      }
      // Add to new zone
      if (toZ >= 0 && toZ < numZones && zones[toZ].tgtCount < MAX_TARGETS) {
        zones[toZ].tgtIdx[zones[toZ].tgtCount++] = idx;
        targetZone[idx] = toZ;
      } else {
        targetZone[idx] = -1;  // unassigned
      }
      Serial.printf("ZONES: T%d moved to zone %d\n", tid, toZ);
    }
  }
  
  // Auto-split: evenly distribute unassigned targets
  if (server.hasArg("auto") && !anyGameActive) {
    // Clear all assignments
    for (int i = 0; i < MAX_TARGETS; i++) targetZone[i] = -1;
    for (int z = 0; z < MAX_ZONES; z++) zones[z].tgtCount = 0;
    
    // Collect online targets, sorted by ID (numerical order)
    uint8_t onl[MAX_TARGETS];
    uint8_t cnt = 0;
    for (int i = 0; i < numTargets; i++) {
      if (targets[i].online) onl[cnt++] = i;
    }
    // Sort by target ID (insertion sort)
    for (int i = 1; i < cnt; i++) {
      uint8_t key = onl[i];
      int j = i - 1;
      while (j >= 0 && targets[onl[j]].id > targets[key].id) {
        onl[j + 1] = onl[j]; j--;
      }
      onl[j + 1] = key;
    }
    
    // Distribute in sequential blocks (T1,T2,T3→A  T4,T5,T6→B)
    for (int i = 0; i < cnt; i++) {
      int z = (i * numZones) / cnt;  // sequential blocks
      zones[z].tgtIdx[zones[z].tgtCount++] = onl[i];
      targetZone[onl[i]] = z;
    }
    Serial.printf("ZONES: auto-split %d targets across %d zones\n", cnt, numZones);
  }
  
  // Return zone config as JSON
  String json;
  json.reserve(1024);
  json = "{\"numZones\":" + String(numZones) + ",\"zones\":[";
  for (int z = 0; z < MAX_ZONES; z++) {
    if (z > 0) json += ",";
    json += "{\"name\":\"" + String(zones[z].name) + "\"";
    json += ",\"active\":" + String(zones[z].active ? "true" : "false");
    json += ",\"state\":" + String((int)zones[z].state);
    json += ",\"mode\":" + String((int)zones[z].mode);
    json += ",\"tgts\":[";
    for (int i = 0; i < zones[z].tgtCount; i++) {
      if (i > 0) json += ",";
      json += String(targets[zones[z].tgtIdx[i]].id);
    }
    json += "]}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// OTA update password — verander dit!
#define OTA_PASSWORD "RTT2026"

void handleOTAPage() {
  // Password check via URL param: /update?pw=RTT2026
  if (!server.hasArg("pw") || server.arg("pw") != OTA_PASSWORD) {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Firmware Update</title>"
      "<style>"
      "body{font-family:'Segoe UI',sans-serif;background:#000;color:#fff;padding:20px;text-align:center}"
      "h1{color:#DC0000}"
      ".box{background:#1a1a1a;border:2px solid #DC0000;border-radius:10px;padding:20px;max-width:400px;margin:20px auto}"
      "input{width:80%;padding:10px;font-size:1em;border:2px solid #DC0000;border-radius:8px;background:#222;color:#fff;margin:10px 0}"
      "button{background:#DC0000;color:#fff;border:none;padding:12px 30px;font-size:1em;font-weight:bold;border-radius:8px;cursor:pointer}"
      ".back{background:#333;color:#fff;text-decoration:none;padding:10px 20px;border-radius:8px;display:inline-block;margin-top:15px}"
      "</style></head><body>"
      "<h1>&#x1F512; Firmware Update</h1>"
      "<div class='box'>"
      "<p>Voer het OTA wachtwoord in:</p>"
      "<input type='password' id='pw' placeholder='Wachtwoord'><br>"
      "<button onclick=\"location.href='/update?pw='+document.getElementById('pw').value\">&#x1F513; Inloggen</button>"
      "</div>"
      "<a class='back' href='/'>&#x2190; Terug</a>"
      "</body></html>";
    server.send(200, "text/html", html);
    return;
  }
  
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>"
    "<title>Firmware Update</title>"
    "<style>"
    "body{font-family:'Segoe UI',sans-serif;background:#000;color:#fff;padding:20px;text-align:center}"
    "h1{color:#DC0000}h2{color:#fff}"
    ".box{background:#1a1a1a;border:2px solid #DC0000;border-radius:10px;padding:20px;max-width:400px;margin:20px auto}"
    "input[type=file]{margin:15px 0;color:#fff}"
    "button{background:#DC0000;color:#fff;border:none;padding:12px 30px;font-size:1em;font-weight:bold;border-radius:8px;cursor:pointer;margin:10px}"
    "button:hover{background:#ff2020}"
    ".back{background:#333;color:#fff;text-decoration:none;padding:10px 20px;border-radius:8px;display:inline-block;margin-top:15px}"
    "#prog{width:100%;height:25px;margin:10px 0;display:none}"
    "#msg{margin:10px;font-weight:bold;font-size:1.1em}"
    "</style></head><body>"
    "<h1>&#x1F527; RAF RTT TRAINING SYSTEM</h1>"
    "<div class='box'><h2>Firmware Update</h2>"
    "<p style='font-size:.85em;color:#888'>Chip: " + String((uint32_t)(ESP.getEfuseMac() >> 16), HEX) + 
    " | FW: V3Sec</p>"
    "<p>Upload een <b>.bin</b> bestand<br><span style='font-size:.8em;color:#aaa'>(Arduino IDE &rarr; Sketch &rarr; Export Compiled Binary)</span></p>"
    "<form id='uf' method='POST' action='/doUpdate' enctype='multipart/form-data'>"
    "<input type='file' name='update' id='file' accept='.bin'><br>"
    "<button type='submit' onclick='startUpload()'>&#x1F680; Upload Firmware</button>"
    "</form>"
    "<progress id='prog' value='0' max='100'></progress>"
    "<div id='msg'></div>"
    "<a class='back' href='/'>&#x2190; Terug</a>"
    "</div>"
    "<script>"
    "function startUpload(){"
    "  document.getElementById('prog').style.display='block';"
    "  document.getElementById('msg').textContent='Uploading...';"
    "}"
    "var f=document.getElementById('uf');"
    "f.addEventListener('submit',function(e){"
    "  e.preventDefault();"
    "  var fd=new FormData(f);"
    "  var xhr=new XMLHttpRequest();"
    "  xhr.open('POST','/doUpdate',true);"
    "  xhr.upload.onprogress=function(ev){"
    "    if(ev.lengthComputable){"
    "      var pct=Math.round(ev.loaded/ev.total*100);"
    "      document.getElementById('prog').value=pct;"
    "      document.getElementById('msg').textContent=pct+'%';"
    "    }"
    "  };"
    "  xhr.onload=function(){"
    "    if(xhr.status==200){"
    "      document.getElementById('msg').innerHTML='<span style=\"color:#0f0\">&#x2705; Update gelukt! Herstart...</span>';"
    "      setTimeout(function(){location.href='/';},5000);"
    "    }else{"
    "      document.getElementById('msg').innerHTML='<span style=\"color:#f00\">&#x274C; '+xhr.responseText+'</span>';"
    "    }"
    "  };"
    "  xhr.onerror=function(){document.getElementById('msg').innerHTML='<span style=\"color:#f00\">Upload mislukt</span>';};"
    "  xhr.send(fd);"
    "  document.getElementById('prog').style.display='block';"
    "});"
    "</script></body></html>";
  server.send(200, "text/html", html);
}

void handleDoUpdate() {
  server.sendHeader("Connection", "close");
  if (Update.hasError()) {
    server.send(500, "text/plain", "Update MISLUKT: " + String(Update.errorString()));
  } else {
    server.send(200, "text/plain", "OK");
    delay(1000);
    ESP.restart();
  }
}

void handleDoUpdateUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("OTA Update: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("OTA Success: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

// ============================================================
// SETUP
// ============================================================


void setup() {
  Serial.begin(115200);
  
  // Master hit LED
  pinMode(HIT_LED_PIN, OUTPUT);
  digitalWrite(HIT_LED_PIN, LOW);
  
  // Physical START button (pullup: HIGH = released, LOW = pressed)
  pinMode(START_BTN_PIN, INPUT_PULLUP);
  
  // ---- CHIP-ID SECURITY CHECK ----
  uint64_t chipId = ESP.getEfuseMac();
  Serial.println("\n==========================================");
  Serial.println("  BB TARGET SYSTEM - MASTER V3 SECURE");
  Serial.println("==========================================");
  Serial.printf("  Chip ID: 0x%012llX\n", chipId);
  Serial.println("==========================================\n");
  
  if (!chipIsAuthorized()) {
    securityHalt(chipId);  // Never returns
  }
  Serial.println(">> Chip authorized — starting system...\n");
  
  // Clear pending actions
  clearAllPending();
  delay(100);  // Short settle time
  
  // Init targets — array is leeg, targets melden zich automatisch aan
  Serial.println("Wacht op targets (auto-discovery via announce)...");
  
  // Init zones
  initZones();
  snapshotBaseSettings();
  
  // WiFi AP mode
  WiFi.mode(WIFI_AP_STA);
  // Zet master MAC vast zodat targets altijd dezelfde master herkennen
  // Bij vervanging van master-ESP32: geen target herprogrammering nodig
  const uint8_t masterMAC[] = {0x28, 0x05, 0xA5, 0x07, 0x41, 0xFD};
  esp_wifi_set_mac(WIFI_IF_AP, masterMAC);
  WiFi.softAP(AP_SSID, AP_PASS, 1);  // Channel 1!
  Serial.printf("AP SSID: %s\n", AP_SSID);
  Serial.printf("AP IP:   %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("AP MAC:  %s\n", WiFi.softAPmacAddress().c_str());
  
  // ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW FAIL!");
    while (1) delay(1000);
  }
  
  esp_now_register_recv_cb(OnDataRecv);
  
  // Peers worden automatisch geregistreerd bij ontvangst van announce
  // (geen hardcoded MACs meer nodig)
  
  // Load leaderboard
  loadScores();
  
  // Web server routes
  server.on("/",            handleRoot);
  server.on("/status",      handleStatus);
  server.on("/start",       handleStart);
  server.on("/quickstart",  handleQuickStart);
  server.on("/quickstop",   handleQuickStop);
  server.on("/stop",        handleStop);
  server.on("/pause",       handlePause);
  server.on("/reset",       handleReset);
  server.on("/nextturn",    handleNextTurn);
  server.on("/setmode",     handleSetMode);
  server.on("/threshold",   handleThreshold);
  server.on("/activate",    handleActivate);
  server.on("/locate",      handleLocate);
  server.on("/savescore",   handleSaveScore);
  server.on("/tbcontrol",   handleTbControl);
  server.on("/resetscores", handleResetScores);
  server.on("/zones",       handleZones);
  server.on("/update",      handleOTAPage);
  server.on("/nfc",         handleNFCPage);
  server.on("/doUpdate",    HTTP_POST, handleDoUpdate, handleDoUpdateUpload);
  server.on("/otaTarget",   handleTargetOTA);
  
  server.begin();
  Serial.println("Web server started on port 80");
  
  // Initial ping
  pingTargets();
  lastPingTime = millis();
  
  // Seed random
  randomSeed(analogRead(0));
  
  Serial.println("\n--- SYSTEM READY ---\n");
  
  // Hardware watchdog: auto-restart als main loop >30s vastloopt
  const esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 30000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  Serial.println("Watchdog timer: 30s");
}

// ============================================================
// PROCESS PENDING ACTIONS (non-blocking)
// ============================================================

void processPendingActions() {
  uint32_t now = millis();
  
  // Master LED off
  if (masterLedOffTime && now >= masterLedOffTime) {
    digitalWrite(HIT_LED_PIN, LOW);
    masterLedOffTime = 0;
  }
  
  for (int i = 0; i < MAX_PENDING; i++) {
    if (pendingActions[i].type == ACT_NONE) continue;
    if (now < pendingActions[i].triggerTime) continue;
    
    PendingAction &a = pendingActions[i];
    // Load zone context for game-logic actions
    bool needsZone = (numZones > 1 && a.type != ACT_FLASH && a.type != ACT_LIGHT_OFF && a.type != ACT_LED_OFF);
    if (needsZone && a.zone >= 0 && a.zone < MAX_ZONES) loadZone(a.zone);
    switch (a.type) {
      case ACT_FLASH:
        if (a.targetIdx >= 0) sendFlash(a.targetIdx, a.r, a.g, a.b, a.dur);
        break;
      case ACT_LIGHT_OFF:
        if (a.targetIdx >= 0) sendLightOff(a.targetIdx);
        break;
      case ACT_END_GAME:
        endGame();
        break;
      case ACT_ADVANCE_SEQ:
        advanceSequence();
        break;
      case ACT_ADVANCE_RAND:
        advanceRandom();
        break;
      case ACT_FINISH_RC:
        finishRCCycle();
        break;
      case ACT_FINISH_RX:
        finishReactionCycle();
        break;
      case ACT_PAR_NEXT:
        // Turn off previous ONLY als shoot target (no-shoot blijft branden)
        if (parCurrent > 0 && targets[parOrder[parCurrent - 1]].isShoot) sendLightOff(parOrder[parCurrent - 1]);
        if (parCurrent < parTotal) {
          bool nsPN = decideNoShoot();
          targets[parOrder[parCurrent]].isShoot = !nsPN;
          if (nsPN) {
            sendLightOn(parOrder[parCurrent], 0, 255, 0);
            parCurrent++;
            if (parCurrent >= parTotal) { endGame(); }
            else { schedulePending(400, ACT_PAR_NEXT, 0); }
          } else {
            sendLightOn(parOrder[parCurrent], 255, 0, 0);
            Serial.printf("PARCOURS: Target T%d aan (%d/%d)\n", targets[parOrder[parCurrent]].id, parCurrent + 1, parTotal);
          }
        }
        break;
      case ACT_FT_LIGHT: {
        int p = a.targetIdx;
        if (p >= 0 && p < rcPlayerCount && ftPlayerTarget[p] >= 0 && ftWinner < 0) {
          int t = ftPlayerTarget[p];
          bool nsFTL = decideNoShoot();
          targets[t].isShoot = !nsFTL;
          if (nsFTL) {
            sendLightOn(t, 0, 255, 0);
            schedulePending(1000, ACT_NS_OFF, t);
          } else {
            sendLightOn(t, rcPlayers[p].r, rcPlayers[p].g, rcPlayers[p].b);
          }
          Serial.printf("FASTTRACK: %s → T%d%s\n", rcPlayers[p].name, targets[t].id, nsFTL ? " (NS)" : "");
        }
        break;
      }
      case ACT_TB_LIGHT: {
        int tm = a.targetIdx;
        if (tm >= 0 && tm < (int)tbNumTeams && !tbTeam[tm].done && tbWinner < 0) {
          tbActivateNext(tm);
        }
        break;
      }
      case ACT_TB_RX_LIGHT: {
        // Reaction mode: delay expired, light the target now
        int tm = a.targetIdx;
        if (tm >= 0 && tm < (int)tbNumTeams && !tbTeam[tm].done && tbWinner < 0) {
          int8_t tgt = tbTeam[tm].memSeq[0];  // stored target index
          tbTeam[tm].rxWaiting = false;
          tbTeam[tm].rxLightTime = millis();
          tbLightTarget(tm, tgt);
          Serial.printf("TOURN RX: %s → T%d (SCHIET!)\n", tbTeam[tm].name, targets[tgt].id);
        }
        break;
      }
      case ACT_TB_MEM_SHOW: {
        // Memory mode: show next target in sequence
        int tm = a.targetIdx;
        if (tm >= 0 && tm < (int)tbNumTeams && !tbTeam[tm].done && tbWinner < 0) {
          TeamState &ts = tbTeam[tm];
          // Turn off previous
          if (ts.curTarget >= 0) sendLightOff(ts.curTarget);
          
          if (ts.memShowIdx < ts.memLen) {
            int8_t tgt = ts.tgts[ts.memSeq[ts.memShowIdx]];
            sendLightOn(tgt, tbColor[tm][0], tbColor[tm][1], tbColor[tm][2]);
            ts.curTarget = tgt;
            ts.memShowIdx++;
            // Schedule next show or switch to play
            if (ts.memShowIdx < ts.memLen) {
              schedulePending(settings.memDisplayTime, ACT_TB_MEM_SHOW, tm);
            } else {
              schedulePending(settings.memDisplayTime, ACT_TB_MEM_PLAY, tm);
            }
          }
        }
        break;
      }
      case ACT_TB_MEM_PLAY: {
        // Memory mode: done showing, start replay
        int tm = a.targetIdx;
        if (tm >= 0 && tm < (int)tbNumTeams && !tbTeam[tm].done && tbWinner < 0) {
          TeamState &ts = tbTeam[tm];
          if (ts.curTarget >= 0) sendLightOff(ts.curTarget);
          ts.memPhase = 1;  // replay mode
          ts.memPos = 0;
          // Light first expected target
          int8_t tgt = ts.tgts[ts.memSeq[0]];
          sendLightOn(tgt, tbColor[tm][0], tbColor[tm][1], tbColor[tm][2]);
          ts.curTarget = tgt;
          Serial.printf("TOURN MEM: %s → replay level %d\n", ts.name, ts.memLen);
        }
        break;
      }
      case ACT_NS_OFF: {
        // No-shoot target auto-off na 1s (variant B: seq/rand/rx/ft)
        int t = a.targetIdx;
        if (t >= 0 && t < numTargets && !targets[t].isShoot) {
          sendLightOff(t);
          // For seq/rand: advance to next target
          if (currentMode == MODE_SEQUENCE) {
            seqLightNext();
          } else if (currentMode == MODE_RANDOM) {
            advanceRandom();
          } else if (currentMode == MODE_FASTTRACK) {
            // Find which player owned this target and give them a new one
            for (int p = 0; p < rcPlayerCount; p++) {
              if (ftPlayerTarget[p] == t) {
                int next = ftPickTarget(p);
                ftPlayerTarget[p] = next;
                if (next >= 0) schedulePending(200, ACT_FT_LIGHT, p);
                break;
              }
            }
          }
          // Reaction: cycle timeout handles advancement, no extra action needed
        }
        break;
      }
      default:
        break;
    }
    if (needsZone && a.zone >= 0 && a.zone < MAX_ZONES) saveZone(a.zone);
    a.type = ACT_NONE;  // Mark as done
  }
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {
  // Feed watchdog — als dit niet elke 30s wordt aangeroepen, herstart de ESP32 automatisch
  esp_task_wdt_reset();
  
  // Handle web requests
  server.handleClient();
  
  // Process non-blocking pending actions
  processPendingActions();
  
  // Process auto-discovery queue
  processDiscovery();
  
  // Process queued hits — route to correct zone
  // Tournament mode always runs globally in zone 0
  bool isTournament = ((zones[0].mode == MODE_TOURNAMENT || zones[0].mode == MODE_CAPTURE) && zones[0].state >= STATE_COUNTDOWN);
  HitEvent hit;
  while (dequeueHit(hit)) {
    int idx = findTargetByID(hit.targetId);
    if (idx < 0) continue;
    int8_t z;
    if (isTournament) {
      z = 0;  // Tournament: all hits go to zone 0
    } else {
      z = (numZones > 1) ? findZoneForTarget(idx) : 0;
    }
    if (z >= 0) {
      loadZone(z);
      processHit(hit);
      saveZone(z);
    }
  }
  
  // Per-zone game logic (single-zone: loadZone/saveZone are no-ops)
  for (uint8_t z = 0; z < numZones; z++) {
    if (!zones[z].active) continue;
    loadZone(z);
    if (gameState == STATE_IDLE || gameState == STATE_ENDED) { saveZone(z); continue; }

    // TOERBEURT: auto-countdown tussen beurten
    if (gameState == STATE_TURN_WAIT && turnMode && turnAutoNext) {
      unsigned long elapsed = millis() - turnWaitStart;
      if (elapsed >= (unsigned long)turnCountdownSec * 1000) {
        // Auto-start volgende beurt
        strncpy(playerName, turnPlayers[turnCurrentPlayer].name, 15);
        playerName[15] = '\0';
        currentScore = 0;
        totalHits = 0;
        totalMisses = 0;
        totalPausedMs = 0;
        finalTimeMs = 0;
        currentMode = turnSavedMode;
        startGame();
        Serial.printf("TURN AUTO: %s (%d/%d) start\n",
          turnPlayers[turnCurrentPlayer].name, turnCurrentPlayer + 1, turnPlayerCount);
      }
      saveZone(z);
      continue;
    }
    if (gameState == STATE_TURN_WAIT) { saveZone(z); continue; }

    if (gameState == STATE_COUNTDOWN) {
      if (millis() - countdownStart >= (uint32_t)countdownTotal * 1000) {
        beginPlaying();
      }
    }
    if (gameState == STATE_RUNNING) {
      uint32_t gt = getGameTimeMs();
      if (gt > 0 && getElapsed() >= gt) {
        endGame();
      }
      if (currentMode == MODE_SEQUENCE)    handleSequenceLogic();
      if (currentMode == MODE_RANDOM)      handleRandomLogic();
      if (currentMode == MODE_MEMORY)      handleMemoryLogic();
      if (currentMode == MODE_RANDOMCOLOR) handleRCLogic();
      if (currentMode == MODE_REACTION)    handleReactionLogic();
      if (currentMode == MODE_PARCOURS)    handleParcoursLogic();
      if (currentMode == MODE_CAPTURE)     handleCaptureLogic();
    }
    saveZone(z);
  }
  
  // Periodic ping (every 5 seconds)
  if (millis() - lastPingTime > 5000) {
    pingTargets();
    lastPingTime = millis();
    
    // Mark targets offline if no announce/pong in 10s
    bool changed = false;
    for (int i = 0; i < numTargets; i++) {
      if (targets[i].online && millis() - targets[i].lastPong > 10000) {
        targets[i].online = false;
        changed = true;
        Serial.printf("TARGET OFFLINE: T%d\n", targets[i].id);
      }
    }
    
    // Mid-game: reageer op offline targets per zone
    if (changed) {
      for (uint8_t z = 0; z < numZones; z++) {
        if (!zones[z].active) continue;
        loadZone(z);
        if (gameState != STATE_RUNNING) { saveZone(z); continue; }
        
        refreshOnline();
        if (onlineCount == 0) {
          Serial.printf("ZONE %c: ALLE TARGETS OFFLINE - game gestopt\n", 'A'+z);
          endGame();
        } else {
          switch (currentMode) {
            case MODE_SEQUENCE:
              if (!targets[seqCurrentIdx].online) advanceSequence();
              break;
            case MODE_RANDOM:
              if (!targets[randCurrentIdx].online) advanceRandom();
              break;
            case MODE_MEMORY:
              if (memPhase == 5 && !targets[memSequence[memShootIdx]].online) {
                memShootIdx++;
                if (memShootIdx >= settings.memLength) endGame();
              }
              break;
            case MODE_RANDOMCOLOR:
              for (int i = 0; i < numTargets; i++) {
                if (!isMyTarget(i)) continue;
                if (!targets[i].online && rcTargetPlayer[i] >= 0) {
                  rcTargetPlayer[i] = -1; if (rcActiveTargets > 0) rcActiveTargets--;
                }
              }
              if (rcActiveTargets == 0) startRCCycle();
              break;
            case MODE_SHOOTNOSHOOT: {
              bool allDone = true;
              for (int i = 0; i < numTargets; i++) {
                if (!isMyTarget(i)) continue;
                if (!targets[i].online) continue;
                if (targets[i].isShoot && !targets[i].shootHit) { allDone = false; break; }
              }
              if (allDone) { finalTimeMs = millis() - gameStartTime - totalPausedMs; schedulePending(300, ACT_END_GAME); }
              break;
            }
            case MODE_PARCOURS:
              if (parCurrent < parTotal && !targets[parOrder[parCurrent]].online) {
                parSplits[parCurrent] = getElapsed(); parCurrent++;
                if (parCurrent >= parTotal) endGame();
                else sendLightOn(parOrder[parCurrent], 255, 0, 0);
              }
              break;
            case MODE_FASTTRACK:
              for (int p = 0; p < rcPlayerCount; p++) {
                if (ftPlayerTarget[p] >= 0 && !targets[ftPlayerTarget[p]].online) {
                  int next = ftPickTarget(p); ftPlayerTarget[p] = next;
                  if (next >= 0 && targets[next].online) sendLightOn(next, rcPlayers[p].r, rcPlayers[p].g, rcPlayers[p].b);
                }
              }
              break;
            case MODE_TOURNAMENT:
              for (int t = 0; t < (int)tbNumTeams; t++) {
                if (tbTeam[t].curTarget >= 0 && !targets[tbTeam[t].curTarget].online) {
                  int next = tbPickRandom(t); tbTeam[t].curTarget = next;
                  if (next >= 0 && targets[next].online) tbLightTarget(t, next);
                }
              }
              break;
            default: break;
          }
        }
        saveZone(z);
      }
    }
  }
  
  // ---- WiFi health check (elke 30s) ----
  static unsigned long lastWifiCheck = 0;
  static uint8_t wifiFailCount = 0;
  if (millis() - lastWifiCheck > 30000) {
    lastWifiCheck = millis();
    
    // Check of AP nog actief is
    IPAddress apIP = WiFi.softAPIP();
    if (apIP[0] == 0 && apIP[1] == 0 && apIP[2] == 0 && apIP[3] == 0) {
      wifiFailCount++;
      Serial.printf("WIFI: AP down! Poging %d/3 — herstart WiFi...\n", wifiFailCount);
      
      // Herstart WiFi stack
      WiFi.mode(WIFI_OFF);
      delay(500);
      WiFi.mode(WIFI_AP_STA);
      const uint8_t masterMAC[] = {0x28, 0x05, 0xA5, 0x07, 0x41, 0xFD};
      esp_wifi_set_mac(WIFI_IF_AP, masterMAC);
      WiFi.softAP(AP_SSID, AP_PASS, 1);
      
      // Re-init ESP-NOW
      esp_now_deinit();
      esp_now_init();
      esp_now_register_recv_cb(OnDataRecv);
      
      // Re-register known target peers
      for (int i = 0; i < numTargets; i++) {
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, targets[i].mac, 6);
        peer.channel = 1;
        peer.encrypt = false;
        peer.ifidx = WIFI_IF_AP;
        esp_now_add_peer(&peer);
      }
      
      Serial.printf("WIFI: Herstart klaar. AP IP: %s\n", WiFi.softAPIP().toString().c_str());
      
      if (wifiFailCount >= 3) {
        Serial.println("WIFI: 3x mislukt — volledige reboot!");
        delay(100);
        ESP.restart();
      }
    } else {
      wifiFailCount = 0;  // Reset counter als alles OK is
    }
  }
  
  // ---- Auto-reboot na 24 uur (preventief, alleen als IDLE) ----
  static const unsigned long REBOOT_INTERVAL = 86400000UL;  // 24 uur in ms
  if (millis() > REBOOT_INTERVAL && gameState == STATE_IDLE) {
    Serial.println("AUTO-REBOOT: 24 uur bereikt, preventieve herstart...");
    delay(100);
    ESP.restart();
  }
  
  delay(1);
}
