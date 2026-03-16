/**
 * RTT Fitness Test Timer - Webserver & WebSocket Logic
 *
 * Route handlers, WebSocket events, sessie- en resultatenbeheer.
 */

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"

// ============================================================================
// GLOBALS
// ============================================================================

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct Participant {
    String name;
    int buddyId;
};

struct Result {
    unsigned long timeMs;
    int score;
    bool completed;
};

const char* exerciseNames[] = {"Sprint", "Hardlopen", "Schietoefening"};

// Sessie state
Participant participants[MAX_PARTICIPANTS];
int participantCount = 0;
int currentExercise = -1;
bool exerciseActive = false;
unsigned long exerciseStartMs = 0;
Result results[MAX_PARTICIPANTS][3];

// ============================================================================
// HELPERS
// ============================================================================

String formatTime(int exercise, unsigned long ms) {
    if (exercise == 1) {
        unsigned long totalSec = ms / 1000;
        unsigned long min = totalSec / 60;
        unsigned long sec = totalSec % 60;
        return String(min) + ":" + (sec < 10 ? "0" : "") + String(sec);
    }
    return String(ms / 1000.0, 2);
}

String buildStateJson() {
    JsonDocument doc;
    doc["event"] = "state";
    JsonObject data = doc["data"].to<JsonObject>();
    data["participantCount"] = participantCount;
    data["currentExercise"] = currentExercise;
    data["exerciseActive"] = exerciseActive;
    data["elapsedMs"] = exerciseActive ? (millis() - exerciseStartMs) : 0;

    JsonArray parts = data["participants"].to<JsonArray>();
    for (int i = 0; i < participantCount; i++) {
        JsonObject p = parts.add<JsonObject>();
        p["name"] = participants[i].name;
        p["buddyId"] = participants[i].buddyId;
    }

    JsonArray res = data["results"].to<JsonArray>();
    for (int i = 0; i < participantCount; i++) {
        JsonArray pRes = res.add<JsonArray>();
        for (int j = 0; j < 3; j++) {
            JsonObject r = pRes.add<JsonObject>();
            r["timeMs"] = results[i][j].timeMs;
            r["score"] = results[i][j].score;
            r["completed"] = results[i][j].completed;
        }
    }

    String output;
    serializeJson(doc, output);
    return output;
}

void broadcastState() {
    ws.textAll(buildStateJson());
}

void saveResults() {
    JsonDocument doc;
    doc["participantCount"] = participantCount;

    JsonArray parts = doc["participants"].to<JsonArray>();
    for (int i = 0; i < participantCount; i++) {
        JsonObject p = parts.add<JsonObject>();
        p["name"] = participants[i].name;
        p["buddyId"] = participants[i].buddyId;
    }

    JsonArray res = doc["results"].to<JsonArray>();
    for (int i = 0; i < participantCount; i++) {
        JsonArray pRes = res.add<JsonArray>();
        for (int j = 0; j < 3; j++) {
            JsonObject r = pRes.add<JsonObject>();
            r["timeMs"] = results[i][j].timeMs;
            r["score"] = results[i][j].score;
            r["completed"] = results[i][j].completed;
        }
    }

    File f = LittleFS.open("/results.json", "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
        Serial.println("Resultaten opgeslagen");
    }
}

void loadResults() {
    File f = LittleFS.open("/results.json", "r");
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return;

    participantCount = doc["participantCount"] | 0;
    if (participantCount > MAX_PARTICIPANTS) participantCount = MAX_PARTICIPANTS;

    JsonArray parts = doc["participants"];
    for (int i = 0; i < participantCount; i++) {
        participants[i].name = parts[i]["name"].as<String>();
        participants[i].buddyId = parts[i]["buddyId"] | 0;
    }

    JsonArray res = doc["results"];
    for (int i = 0; i < participantCount; i++) {
        JsonArray pRes = res[i];
        for (int j = 0; j < 3; j++) {
            results[i][j].timeMs = pRes[j]["timeMs"] | (unsigned long)0;
            results[i][j].score = pRes[j]["score"] | 0;
            results[i][j].completed = pRes[j]["completed"] | false;
        }
    }

    currentExercise = 0;
    exerciseActive = false;
    Serial.printf("Sessie geladen: %d deelnemers\n", participantCount);
}

// ============================================================================
// WEBSOCKET HANDLER
// ============================================================================

void handleWebSocketMessage(AsyncWebSocketClient *client, uint8_t *data, size_t len) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) return;

    const char* cmd = doc["cmd"];
    if (!cmd) return;

    if (strcmp(cmd, "createSession") == 0) {
        JsonArray names = doc["participants"];
        participantCount = min((int)names.size(), MAX_PARTICIPANTS);
        for (int i = 0; i < participantCount; i++) {
            participants[i].name = names[i].as<String>();
            participants[i].buddyId = 0;
            for (int j = 0; j < 3; j++) {
                results[i][j] = {0, 0, false};
            }
        }
        currentExercise = 0;
        exerciseActive = false;
        saveResults();
        broadcastState();
        Serial.printf("Sessie aangemaakt: %d deelnemers\n", participantCount);
    }
    else if (strcmp(cmd, "assignBuddy") == 0) {
        int pIdx = doc["participant"];
        int bId = doc["buddy"];
        if (pIdx >= 0 && pIdx < participantCount) {
            participants[pIdx].buddyId = bId;
            saveResults();
            broadcastState();
        }
    }
    else if (strcmp(cmd, "startExercise") == 0) {
        int ex = doc["exercise"];
        if (ex >= 0 && ex < 3) {
            currentExercise = ex;
            exerciseActive = true;
            exerciseStartMs = millis();

            // Reset incomplete resultaten voor deze oefening
            for (int i = 0; i < participantCount; i++) {
                if (!results[i][ex].completed) {
                    results[i][ex] = {0, 0, false};
                }
            }

            JsonDocument evt;
            evt["event"] = "exerciseStart";
            evt["data"]["exercise"] = currentExercise;
            evt["data"]["name"] = exerciseNames[currentExercise];
            String msg;
            serializeJson(evt, msg);
            ws.textAll(msg);

            playBuzzer(200);
            Serial.printf("Oefening gestart: %s\n", exerciseNames[currentExercise]);
        }
    }
    else if (strcmp(cmd, "stopExercise") == 0) {
        exerciseActive = false;

        JsonDocument evt;
        evt["event"] = "exerciseStop";
        evt["data"]["exercise"] = currentExercise;
        String msg;
        serializeJson(evt, msg);
        ws.textAll(msg);

        Serial.println("Oefening gestopt door admin");
    }
    else if (strcmp(cmd, "stopTimer") == 0) {
        int pIdx = doc["participant"];
        int ex = doc["exercise"];
        unsigned long time = doc["time"];
        if (pIdx >= 0 && pIdx < participantCount && ex >= 0 && ex < 3) {
            results[pIdx][ex].timeMs = time;
            results[pIdx][ex].completed = true;
            saveResults();
            broadcastState();
            Serial.printf("Tijd geregistreerd: %s - %s - %s\n",
                participants[pIdx].name.c_str(),
                exerciseNames[ex],
                formatTime(ex, time).c_str());
        }
    }
    else if (strcmp(cmd, "setScore") == 0) {
        int pIdx = doc["participant"];
        int ex = doc["exercise"];
        int score = doc["score"];
        if (pIdx >= 0 && pIdx < participantCount && ex >= 0 && ex < 3) {
            results[pIdx][ex].score = score;
            saveResults();
            broadcastState();
        }
    }
    else if (strcmp(cmd, "resetExercise") == 0) {
        int ex = doc["exercise"];
        if (ex >= 0 && ex < 3) {
            for (int i = 0; i < participantCount; i++) {
                results[i][ex] = {0, 0, false};
            }
            exerciseActive = false;
            saveResults();
            broadcastState();
            Serial.printf("Oefening gereset: %s\n", exerciseNames[ex]);
        }
    }
    else if (strcmp(cmd, "nextExercise") == 0) {
        exerciseActive = false;
        if (currentExercise < 2) {
            currentExercise++;
        }
        broadcastState();
    }
    else if (strcmp(cmd, "clearSession") == 0) {
        participantCount = 0;
        currentExercise = -1;
        exerciseActive = false;
        LittleFS.remove("/results.json");
        broadcastState();
        Serial.println("Sessie gewist");
    }
    else if (strcmp(cmd, "getState") == 0) {
        client->text(buildStateJson());
    }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("Client #%u verbonden\n", client->id());
            client->text(buildStateJson());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("Client #%u losgekoppeld\n", client->id());
            break;
        case WS_EVT_DATA: {
            AwsFrameInfo *info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                handleWebSocketMessage(client, data, len);
            }
            break;
        }
        default:
            break;
    }
}

// ============================================================================
// ROUTE SETUP
// ============================================================================

void setupWebServer() {
    // Vorige sessie laden
    loadResults();

    // WebSocket
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // Pagina's
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->redirect("/admin");
    });

    server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(LittleFS, "/admin.html", "text/html");
    });

    server.on("/buddy", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(LittleFS, "/buddy.html", "text/html");
    });

    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(LittleFS, "/style.css", "text/css");
    });

    // API: Resultaten als JSON
    server.on("/api/results", HTTP_GET, [](AsyncWebServerRequest *req) {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < participantCount; i++) {
            JsonObject p = arr.add<JsonObject>();
            p["name"] = participants[i].name;
            p["buddy"] = participants[i].buddyId;
            JsonObject res = p["results"].to<JsonObject>();
            for (int j = 0; j < 3; j++) {
                JsonObject r = res[exerciseNames[j]].to<JsonObject>();
                r["timeMs"] = results[i][j].timeMs;
                r["time"] = formatTime(j, results[i][j].timeMs);
                r["score"] = results[i][j].score;
                r["completed"] = results[i][j].completed;
            }
        }
        String output;
        serializeJson(doc, output);
        req->send(200, "application/json", output);
    });

    // API: CSV export
    server.on("/api/export", HTTP_GET, [](AsyncWebServerRequest *req) {
        String csv = "Naam,Buddy,Sprint (s),Hardlopen,Schietoefening (s),Schiet Score\n";
        for (int i = 0; i < participantCount; i++) {
            csv += participants[i].name + ",";
            csv += String(participants[i].buddyId) + ",";
            // Sprint
            if (results[i][0].completed) csv += String(results[i][0].timeMs / 1000.0, 2);
            csv += ",";
            // Hardlopen
            if (results[i][1].completed) csv += formatTime(1, results[i][1].timeMs);
            csv += ",";
            // Schietoefening
            if (results[i][2].completed) csv += String(results[i][2].timeMs / 1000.0, 2);
            csv += ",";
            if (results[i][2].completed) csv += String(results[i][2].score);
            csv += "\n";
        }
        AsyncWebServerResponse *response = req->beginResponse(200, "text/csv", csv);
        response->addHeader("Content-Disposition", "attachment; filename=resultaten.csv");
        req->send(response);
    });

    server.begin();
}

#endif // WEBSERVER_H
