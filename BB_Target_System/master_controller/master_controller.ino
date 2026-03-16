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
<!DOCTYPE html>
<html lang="nl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <meta name="mobile-web-app-capable" content="yes">
    <title>Raf RTT Training System</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link href="https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700;900&family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --primary: #00ff88;
            --primary-dark: #00cc6a;
            --secondary: #00d4ff;
            --danger: #ff4444;
            --warning: #ff8800;
            --success: #00ff88;
            --bg-dark: #0a0a0f;
            --bg-card: #12121a;
            --bg-card-hover: #1a1a25;
            --border: #2a2a3a;
            --text: #ffffff;
            --text-muted: #888899;
            --gold: #ffd700;
            --silver: #c0c0c0;
            --bronze: #cd7f32;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            -webkit-tap-highlight-color: transparent;
        }

        body {
            font-family: 'Inter', system-ui, -apple-system, sans-serif;
            background: var(--bg-dark);
            color: var(--text);
            min-height: 100vh;
            overflow-x: hidden;
        }

        /* TV Mode Styles */
        body.tv-mode {
            font-size: 1.3em;
        }

        body.tv-mode .container {
            max-width: 100%;
            padding: 40px;
        }

        body.tv-mode .timer-value {
            font-size: 8rem;
        }

        body.tv-mode .score-value {
            font-size: 6rem;
        }

        /* Container */
        .container {
            max-width: 1600px;
            margin: 0 auto;
            padding: 15px;
        }

        /* Header */
        header {
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            padding: 20px 25px;
            border-radius: 20px;
            margin-bottom: 20px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            flex-wrap: wrap;
            gap: 15px;
            border: 1px solid var(--border);
            box-shadow: 0 10px 40px rgba(0, 0, 0, 0.3);
        }

        .logo {
            display: flex;
            align-items: center;
            gap: 15px;
        }

        .logo-icon {
            width: 50px;
            height: 50px;
            background: linear-gradient(135deg, var(--primary), var(--secondary));
            border-radius: 12px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 24px;
        }

        h1 {
            font-family: 'Orbitron', monospace;
            font-size: 1.8rem;
            font-weight: 900;
            background: linear-gradient(90deg, var(--primary), var(--secondary));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
            letter-spacing: 2px;
        }

        .header-controls {
            display: flex;
            align-items: center;
            gap: 15px;
        }

        .status-indicator {
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 8px 16px;
            background: rgba(0, 0, 0, 0.3);
            border-radius: 50px;
        }

        .status-dot {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background: var(--danger);
            transition: all 0.3s;
        }

        .status-dot.online {
            background: var(--success);
            box-shadow: 0 0 15px var(--success);
            animation: pulse-glow 2s infinite;
        }

        @keyframes pulse-glow {
            0%, 100% { box-shadow: 0 0 15px var(--success); }
            50% { box-shadow: 0 0 25px var(--success); }
        }

        /* Main Grid */
        .main-grid {
            display: grid;
            grid-template-columns: 1fr 2fr 1fr;
            gap: 20px;
        }

        @media (max-width: 1200px) {
            .main-grid {
                grid-template-columns: 1fr 1fr;
            }
        }

        @media (max-width: 768px) {
            .main-grid {
                grid-template-columns: 1fr;
            }
        }

        /* Cards */
        .card {
            background: var(--bg-card);
            border-radius: 20px;
            padding: 25px;
            border: 1px solid var(--border);
            transition: all 0.3s;
        }

        .card:hover {
            border-color: var(--primary);
            box-shadow: 0 0 30px rgba(0, 255, 136, 0.1);
        }

        .card-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 20px;
        }

        .card h2 {
            font-family: 'Orbitron', monospace;
            font-size: 1rem;
            color: var(--secondary);
            letter-spacing: 1px;
            text-transform: uppercase;
        }

        /* Timer & Score Display */
        .timer-score-card {
            background: linear-gradient(145deg, #12121a, #0a0a0f);
            text-align: center;
        }

        .timer-container {
            margin-bottom: 30px;
        }

        .timer-label {
            font-size: 0.9rem;
            color: var(--text-muted);
            margin-bottom: 10px;
            text-transform: uppercase;
            letter-spacing: 2px;
        }

        .timer-value {
            font-family: 'Orbitron', monospace;
            font-size: 4.5rem;
            font-weight: 900;
            background: linear-gradient(90deg, var(--primary), var(--secondary));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
            text-shadow: 0 0 60px rgba(0, 255, 136, 0.5);
            line-height: 1;
        }

        .timer-value.warning {
            background: linear-gradient(90deg, var(--warning), var(--danger));
            -webkit-background-clip: text;
            background-clip: text;
        }

        .score-container {
            margin-bottom: 30px;
        }

        .score-label {
            font-size: 0.9rem;
            color: var(--text-muted);
            margin-bottom: 10px;
            text-transform: uppercase;
            letter-spacing: 2px;
        }

        .score-value {
            font-family: 'Orbitron', monospace;
            font-size: 4rem;
            font-weight: 900;
            color: var(--gold);
            text-shadow: 0 0 40px rgba(255, 215, 0, 0.5);
        }

        .stats-row {
            display: flex;
            justify-content: center;
            gap: 40px;
            margin-bottom: 25px;
        }

        .stat-item {
            text-align: center;
        }

        .stat-value {
            font-family: 'Orbitron', monospace;
            font-size: 2rem;
            font-weight: 700;
        }

        .stat-value.hits { color: var(--success); }
        .stat-value.misses { color: var(--danger); }

        .stat-label {
            font-size: 0.75rem;
            color: var(--text-muted);
            text-transform: uppercase;
        }

        /* Controls */
        .controls {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
            justify-content: center;
        }

        .btn {
            padding: 15px 30px;
            border: none;
            border-radius: 12px;
            font-family: 'Inter', sans-serif;
            font-size: 1rem;
            font-weight: 700;
            cursor: pointer;
            transition: all 0.2s;
            text-transform: uppercase;
            letter-spacing: 1px;
            position: relative;
            overflow: hidden;
        }

        .btn::before {
            content: '';
            position: absolute;
            top: 0;
            left: -100%;
            width: 100%;
            height: 100%;
            background: linear-gradient(90deg, transparent, rgba(255,255,255,0.2), transparent);
            transition: 0.5s;
        }

        .btn:hover::before {
            left: 100%;
        }

        .btn-start {
            background: linear-gradient(135deg, var(--primary), var(--primary-dark));
            color: #000;
            min-width: 140px;
        }

        .btn-start:hover {
            transform: translateY(-3px);
            box-shadow: 0 10px 30px rgba(0, 255, 136, 0.4);
        }

        .btn-pause {
            background: linear-gradient(135deg, var(--warning), #cc6600);
            color: #000;
        }

        .btn-stop {
            background: linear-gradient(135deg, var(--danger), #cc0000);
            color: #fff;
        }

        .btn-reset {
            background: var(--border);
            color: var(--text);
        }

        .btn:disabled {
            opacity: 0.4;
            cursor: not-allowed;
            transform: none !important;
        }

        /* Targets Grid */
        .targets-grid {
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 15px;
        }

        @media (max-width: 500px) {
            .targets-grid {
                grid-template-columns: repeat(2, 1fr);
            }
        }

        .target {
            aspect-ratio: 1;
            border-radius: 16px;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            cursor: pointer;
            transition: all 0.2s;
            border: 3px solid transparent;
            background: linear-gradient(145deg, #1a1a2e, #12121a);
            position: relative;
            overflow: hidden;
        }

        .target::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: radial-gradient(circle at center, transparent 30%, rgba(0,0,0,0.3));
            pointer-events: none;
        }

        .target-num {
            font-family: 'Orbitron', monospace;
            font-size: 2.5rem;
            font-weight: 900;
            position: relative;
            z-index: 1;
        }

        .target-status {
            font-size: 0.7rem;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: var(--text-muted);
            margin-top: 5px;
        }

        .target.offline {
            opacity: 0.3;
            cursor: not-allowed;
        }

        .target.offline .target-status {
            color: var(--danger);
        }

        .target.inactive {
            border-color: var(--border);
        }

        .target.inactive .target-num {
            color: var(--text-muted);
        }

        .target.active {
            border-color: var(--success);
            background: linear-gradient(145deg, rgba(0, 255, 136, 0.15), rgba(0, 255, 136, 0.05));
            box-shadow: 0 0 30px rgba(0, 255, 136, 0.3), inset 0 0 30px rgba(0, 255, 136, 0.1);
            animation: target-pulse 1s infinite;
        }

        .target.active .target-num {
            color: var(--success);
            text-shadow: 0 0 20px var(--success);
        }

        @keyframes target-pulse {
            0%, 100% { box-shadow: 0 0 30px rgba(0, 255, 136, 0.3), inset 0 0 30px rgba(0, 255, 136, 0.1); }
            50% { box-shadow: 0 0 50px rgba(0, 255, 136, 0.5), inset 0 0 40px rgba(0, 255, 136, 0.2); }
        }

        .target.hit {
            border-color: var(--warning);
            background: linear-gradient(145deg, rgba(255, 136, 0, 0.3), rgba(255, 136, 0, 0.1));
            animation: hit-flash 0.3s ease-out;
        }

        @keyframes hit-flash {
            0% { transform: scale(1); }
            50% { transform: scale(1.1); background: rgba(255, 200, 0, 0.4); }
            100% { transform: scale(1); }
        }

        .target.noshoot {
            border-color: var(--danger);
            background: linear-gradient(145deg, rgba(255, 68, 68, 0.15), rgba(255, 68, 68, 0.05));
            box-shadow: 0 0 30px rgba(255, 68, 68, 0.3);
        }

        .target.noshoot .target-num {
            color: var(--danger);
        }

        .target-count {
            text-align: center;
            margin-top: 15px;
            color: var(--text-muted);
            font-size: 0.9rem;
        }

        /* Game Modes */
        .mode-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 10px;
        }

        .mode-btn {
            padding: 20px 15px;
            border: 2px solid var(--border);
            border-radius: 12px;
            background: transparent;
            color: var(--text);
            cursor: pointer;
            transition: all 0.2s;
            text-align: center;
        }

        .mode-btn:hover {
            border-color: var(--secondary);
            background: rgba(0, 212, 255, 0.05);
        }

        .mode-btn.active {
            border-color: var(--primary);
            background: rgba(0, 255, 136, 0.1);
            box-shadow: 0 0 20px rgba(0, 255, 136, 0.2);
        }

        .mode-btn strong {
            display: block;
            font-size: 1rem;
            margin-bottom: 5px;
        }

        .mode-btn small {
            color: var(--text-muted);
            font-size: 0.75rem;
        }

        /* Numpad */
        .numpad {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
        }

        .numpad-btn {
            aspect-ratio: 1;
            border: none;
            border-radius: 12px;
            background: var(--border);
            color: var(--text);
            font-family: 'Orbitron', monospace;
            font-size: 1.8rem;
            font-weight: 700;
            cursor: pointer;
            transition: all 0.2s;
        }

        .numpad-btn:hover {
            background: var(--primary);
            color: #000;
            transform: scale(1.05);
        }

        .numpad-btn:active {
            transform: scale(0.95);
        }

        .numpad-btn.all {
            font-size: 1rem;
            background: linear-gradient(135deg, var(--secondary), #0099cc);
        }

        /* Highscores */
        .highscores-list {
            list-style: none;
        }

        .highscore-item {
            display: flex;
            align-items: center;
            padding: 15px;
            border-bottom: 1px solid var(--border);
            gap: 15px;
        }

        .highscore-item:last-child {
            border-bottom: none;
        }

        .rank {
            font-family: 'Orbitron', monospace;
            font-size: 1.5rem;
            font-weight: 900;
            width: 50px;
        }

        .rank-1 { color: var(--gold); text-shadow: 0 0 10px var(--gold); }
        .rank-2 { color: var(--silver); }
        .rank-3 { color: var(--bronze); }

        .highscore-name {
            flex: 1;
            font-weight: 600;
        }

        .highscore-score {
            font-family: 'Orbitron', monospace;
            font-size: 1.2rem;
            font-weight: 700;
            color: var(--gold);
        }

        /* Settings */
        .settings-group {
            margin-bottom: 20px;
        }

        .settings-group label {
            display: block;
            font-size: 0.85rem;
            color: var(--text-muted);
            margin-bottom: 8px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }

        .settings-group input,
        .settings-group select {
            width: 100%;
            padding: 12px 15px;
            border: 2px solid var(--border);
            border-radius: 10px;
            background: var(--bg-dark);
            color: var(--text);
            font-family: 'Inter', sans-serif;
            font-size: 1rem;
            transition: all 0.2s;
        }

        .settings-group input:focus,
        .settings-group select:focus {
            outline: none;
            border-color: var(--primary);
            box-shadow: 0 0 15px rgba(0, 255, 136, 0.2);
        }

        /* Event Log */
        .log-container {
            height: 250px;
            overflow-y: auto;
            background: var(--bg-dark);
            border-radius: 10px;
            padding: 15px;
            font-family: 'JetBrains Mono', monospace;
            font-size: 0.8rem;
        }

        .log-entry {
            padding: 5px 0;
            border-bottom: 1px solid rgba(255,255,255,0.05);
            display: flex;
            gap: 10px;
        }

        .log-time {
            color: var(--text-muted);
            flex-shrink: 0;
        }

        .log-entry.hit { color: var(--success); }
        .log-entry.miss { color: var(--danger); }
        .log-entry.info { color: var(--secondary); }

        /* Connection Status Toast */
        .connection-toast {
            position: fixed;
            bottom: 20px;
            right: 20px;
            padding: 15px 25px;
            border-radius: 12px;
            font-weight: 600;
            z-index: 1000;
            transition: all 0.3s;
            display: flex;
            align-items: center;
            gap: 10px;
        }

        .connection-toast.connected {
            background: rgba(0, 255, 136, 0.15);
            border: 1px solid var(--success);
            color: var(--success);
        }

        .connection-toast.disconnected {
            background: rgba(255, 68, 68, 0.15);
            border: 1px solid var(--danger);
            color: var(--danger);
        }

        /* TV Mode Button */
        .tv-mode-btn {
            background: var(--border);
            border: none;
            padding: 10px 15px;
            border-radius: 8px;
            color: var(--text);
            cursor: pointer;
            font-size: 0.9rem;
        }

        .tv-mode-btn:hover {
            background: var(--primary);
            color: #000;
        }

        /* Game Status Overlay */
        .game-status {
            position: fixed;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            background: rgba(0, 0, 0, 0.95);
            padding: 40px 80px;
            border-radius: 20px;
            text-align: center;
            z-index: 1000;
            border: 2px solid var(--primary);
            display: none;
        }

        .game-status.show {
            display: block;
            animation: fadeIn 0.3s;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translate(-50%, -50%) scale(0.9); }
            to { opacity: 1; transform: translate(-50%, -50%) scale(1); }
        }

        .game-status h2 {
            font-family: 'Orbitron', monospace;
            font-size: 3rem;
            margin-bottom: 20px;
        }

        .game-status .final-score {
            font-family: 'Orbitron', monospace;
            font-size: 4rem;
            color: var(--gold);
            margin-bottom: 30px;
        }

        /* Scrollbar */
        ::-webkit-scrollbar {
            width: 8px;
        }

        ::-webkit-scrollbar-track {
            background: var(--bg-dark);
        }

        ::-webkit-scrollbar-thumb {
            background: var(--border);
            border-radius: 4px;
        }

        ::-webkit-scrollbar-thumb:hover {
            background: var(--primary);
        }

        /* ============================================================
           TAB NAVIGATION
           ============================================================ */
        .tab-nav {
            display: flex;
            gap: 0;
            margin-bottom: 20px;
            background: var(--bg-card);
            border-radius: 16px;
            padding: 5px;
            border: 1px solid var(--border);
        }

        .tab-btn {
            flex: 1;
            padding: 16px 20px;
            border: none;
            border-radius: 12px;
            background: transparent;
            color: var(--text-muted);
            font-family: 'Orbitron', monospace;
            font-size: 0.9rem;
            font-weight: 700;
            cursor: pointer;
            transition: all 0.3s;
            text-transform: uppercase;
            letter-spacing: 1px;
        }

        .tab-btn:hover {
            color: var(--text);
            background: rgba(255, 255, 255, 0.05);
        }

        .tab-btn.active {
            background: linear-gradient(135deg, var(--primary), var(--primary-dark));
            color: #000;
            box-shadow: 0 4px 20px rgba(0, 255, 136, 0.3);
        }

        .tab-content {
            display: none;
        }

        .tab-content.active {
            display: block;
        }

        /* ============================================================
           FITNESS COUNTER STYLES
           ============================================================ */
        .fitness-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
        }

        @media (max-width: 768px) {
            .fitness-grid {
                grid-template-columns: 1fr;
            }
        }

        .counter-display {
            text-align: center;
            padding: 20px 0;
        }

        .count-value {
            font-family: 'Orbitron', monospace;
            font-size: 5rem;
            font-weight: 900;
            background: linear-gradient(135deg, var(--primary), var(--secondary));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
            line-height: 1;
        }

        .count-label {
            font-size: 0.9rem;
            color: var(--text-muted);
            margin-top: 8px;
        }

        .round-display {
            text-align: center;
            margin-top: 15px;
            padding: 12px;
            background: rgba(0, 212, 255, 0.1);
            border-radius: 12px;
            border: 1px solid rgba(0, 212, 255, 0.2);
        }

        .round-value {
            font-family: 'Orbitron', monospace;
            font-size: 2rem;
            font-weight: 700;
            color: var(--secondary);
        }

        .round-label {
            font-size: 0.8rem;
            color: var(--text-muted);
        }

        .led-strip-visual {
            display: flex;
            gap: 8px;
            justify-content: center;
            margin: 20px 0;
            padding: 15px;
            background: rgba(0, 0, 0, 0.3);
            border-radius: 30px;
        }

        .led-dot {
            width: 40px;
            height: 40px;
            border-radius: 50%;
            background: #1a1a2e;
            border: 2px solid #2a2a3a;
            transition: all 0.3s ease;
        }

        .led-dot.on {
            border-color: transparent;
            box-shadow: 0 0 15px currentColor, 0 0 30px currentColor;
        }

        .led-dot.flash {
            animation: ledFlash 0.3s ease-out;
        }

        @keyframes ledFlash {
            0% { transform: scale(1.4); }
            100% { transform: scale(1); }
        }

        .red-timer-display {
            text-align: center;
            margin: 15px 0;
            display: none;
        }

        .red-timer-display.active {
            display: block;
        }

        .red-timer-bar {
            height: 8px;
            background: #2a2a3a;
            border-radius: 4px;
            overflow: hidden;
            margin: 10px 0;
        }

        .red-timer-fill {
            height: 100%;
            background: linear-gradient(90deg, var(--danger), #ff6666);
            border-radius: 4px;
            transition: width 0.1s linear;
            box-shadow: 0 0 10px var(--danger);
        }

        .red-timer-text {
            font-family: 'Orbitron', monospace;
            font-size: 1.8rem;
            color: var(--danger);
            text-shadow: 0 0 20px var(--danger);
        }

        .red-timer-label {
            font-size: 0.8rem;
            color: var(--text-muted);
            margin-top: 5px;
        }

        .fitness-progress {
            display: flex;
            justify-content: center;
            margin: 15px 0;
        }

        .progress-ring {
            position: relative;
            width: 100px;
            height: 100px;
        }

        .progress-ring svg {
            transform: rotate(-90deg);
        }

        .progress-ring circle {
            fill: none;
            stroke-width: 8;
        }

        .progress-bg { stroke: #2a2a3a; }

        .progress-fill {
            stroke: var(--primary);
            stroke-linecap: round;
            transition: stroke-dashoffset 0.3s ease;
            filter: drop-shadow(0 0 6px var(--primary));
        }

        .progress-text {
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            font-family: 'Orbitron', monospace;
            font-size: 1rem;
            font-weight: 700;
        }

        .fitness-setting-group {
            margin-bottom: 16px;
        }

        .fitness-setting-label {
            display: block;
            font-size: 0.8rem;
            color: var(--text-muted);
            margin-bottom: 6px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }

        .fitness-setting-row {
            display: flex;
            align-items: center;
            gap: 8px;
        }

        .fitness-setting-input {
            flex: 1;
            background: var(--bg-dark);
            border: 2px solid var(--border);
            border-radius: 10px;
            padding: 10px 14px;
            color: var(--text);
            font-family: 'Orbitron', monospace;
            font-size: 1rem;
            text-align: center;
            outline: none;
            transition: border-color 0.3s;
        }

        .fitness-setting-input:focus {
            border-color: var(--primary);
            box-shadow: 0 0 10px rgba(0, 255, 136, 0.2);
        }

        .fitness-adj-btn {
            width: 38px;
            height: 38px;
            border-radius: 10px;
            border: 1px solid var(--border);
            background: var(--bg-dark);
            color: var(--text);
            font-size: 1.2rem;
            cursor: pointer;
            display: flex;
            align-items: center;
            justify-content: center;
            transition: all 0.3s;
        }

        .fitness-adj-btn:hover {
            background: var(--primary);
            color: #000;
            border-color: var(--primary);
        }

        .fitness-hint {
            font-size: 0.7rem;
            color: var(--text-muted);
            margin-top: 4px;
        }

        .fitness-btn-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin-top: 20px;
        }

        .fitness-btn-full {
            grid-column: 1 / -1;
        }

        .fitness-stats-grid {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: 10px;
            margin-top: 15px;
        }

        .fitness-stat-item {
            text-align: center;
            padding: 12px;
            background: rgba(0, 0, 0, 0.3);
            border-radius: 12px;
        }

        .fitness-stat-value {
            font-family: 'Orbitron', monospace;
            font-size: 1.3rem;
            font-weight: 700;
            color: var(--primary);
        }

        .fitness-stat-label {
            font-size: 0.7rem;
            color: var(--text-muted);
            margin-top: 4px;
        }

        .fitness-mode-selector {
            display: flex;
            gap: 8px;
            margin-bottom: 15px;
            flex-wrap: wrap;
        }

        .fitness-mode-btn {
            flex: 1;
            min-width: 100px;
            padding: 10px 12px;
            border: 1px solid var(--border);
            border-radius: 10px;
            background: var(--bg-dark);
            color: var(--text-muted);
            font-family: 'Inter', sans-serif;
            font-weight: 600;
            font-size: 0.8rem;
            cursor: pointer;
            transition: all 0.3s;
            text-align: center;
        }

        .fitness-mode-btn.active {
            border-color: var(--primary);
            color: var(--primary);
            background: rgba(0, 255, 136, 0.1);
        }

        .fitness-log {
            max-height: 180px;
            overflow-y: auto;
            font-family: 'JetBrains Mono', monospace;
            font-size: 0.8rem;
            padding: 10px;
            background: rgba(0, 0, 0, 0.3);
            border-radius: 10px;
        }

        .fitness-log .log-entry.detection { color: var(--primary); }
        .fitness-log .log-entry.led { color: var(--gold); }
        .fitness-log .log-entry.full { color: var(--warning); font-weight: bold; }
        .fitness-log .log-entry.round { color: var(--secondary); font-weight: bold; }
        .fitness-log .log-entry.timeout { color: var(--danger); }

        @media (max-width: 480px) {
            .count-value { font-size: 3.5rem; }
            .led-dot { width: 32px; height: 32px; }
            .fitness-stats-grid { grid-template-columns: 1fr 1fr; }
            .fitness-btn-grid { grid-template-columns: 1fr; }
            .fitness-btn-full { grid-column: 1; }
            .tab-btn { font-size: 0.75rem; padding: 12px 10px; }
        }
    </style>
</head>
<body>
    <div class="container">
        <!-- Header -->
        <header>
            <div class="logo">
                <div class="logo-icon">🎯</div>
                <h1>RAF RTT TRAINING</h1>
            </div>
            <div class="header-controls">
                <a href="/running_test.html" style="text-decoration:none;padding:8px 16px;border:1px solid #2a2a3a;border-radius:10px;color:#00d4ff;font-weight:600;font-size:0.85rem;transition:all 0.3s;">Hardloop Test</a>
                <button class="tv-mode-btn" onclick="toggleTVMode()">📺 TV Mode</button>
                <div class="status-indicator">
                    <span class="status-dot" id="wsStatus"></span>
                    <span id="wsStatusText">Verbinden...</span>
                </div>
            </div>
        </header>

        <!-- Tab Navigation -->
        <div class="tab-nav">
            <button class="tab-btn active" onclick="switchTab('schieten')">Schieten</button>
            <button class="tab-btn" onclick="switchTab('fitness')">Fitness Training</button>
        </div>

        <!-- ============================================================ -->
        <!-- TAB: SCHIETEN -->
        <!-- ============================================================ -->
        <div class="tab-content active" id="tab-schieten">
        <div class="main-grid">
            <!-- Left Column -->
            <div class="left-column">
                <!-- Game Mode -->
                <div class="card">
                    <div class="card-header">
                        <h2>Game Modus</h2>
                    </div>
                    <div class="mode-grid">
                        <button class="mode-btn active" data-mode="freeplay" onclick="setMode('freeplay')">
                            <strong>🎯 Free Play</strong>
                            <small>Alles aan</small>
                        </button>
                        <button class="mode-btn" data-mode="sequence" onclick="setMode('sequence')">
                            <strong>🔢 Sequence</strong>
                            <small>Volgorde</small>
                        </button>
                        <button class="mode-btn" data-mode="random" onclick="setMode('random')">
                            <strong>🎲 Random</strong>
                            <small>Willekeurig</small>
                        </button>
                        <button class="mode-btn" data-mode="shootnoshoot" onclick="setMode('shootnoshoot')">
                            <strong>🚦 Shoot/No</strong>
                            <small>Groen vs Rood</small>
                        </button>
                    </div>
                </div>

                <!-- Settings -->
                <div class="card" style="margin-top: 20px;">
                    <div class="card-header">
                        <h2>Instellingen</h2>
                    </div>
                    <div class="settings-group">
                        <label>Speler Naam</label>
                        <input type="text" id="playerName" placeholder="Naam" maxlength="20" value="Speler">
                    </div>
                    <div class="settings-group">
                        <label>Speeltijd (seconden)</label>
                        <input type="number" id="gameTime" value="60" min="10" max="300">
                    </div>
                    <div class="settings-group">
                        <label>Target tijd (sec) - Sequence/Random</label>
                        <input type="number" id="targetTime" value="3" min="1" max="10">
                    </div>
                </div>

                <!-- Numpad -->
                <div class="card" style="margin-top: 20px;">
                    <div class="card-header">
                        <h2>Handmatig</h2>
                    </div>
                    <div class="numpad">
                        <button class="numpad-btn" onclick="activateTarget(1)">1</button>
                        <button class="numpad-btn" onclick="activateTarget(2)">2</button>
                        <button class="numpad-btn" onclick="activateTarget(3)">3</button>
                        <button class="numpad-btn" onclick="activateTarget(4)">4</button>
                        <button class="numpad-btn" onclick="activateTarget(5)">5</button>
                        <button class="numpad-btn" onclick="activateTarget(6)">6</button>
                        <button class="numpad-btn" onclick="activateTarget(7)">7</button>
                        <button class="numpad-btn" onclick="activateTarget(8)">8</button>
                        <button class="numpad-btn all" onclick="activateAll()">ALL</button>
                    </div>
                </div>
            </div>

            <!-- Center Column - Timer, Score, Targets -->
            <div class="center-column">
                <!-- Timer & Score -->
                <div class="card timer-score-card">
                    <div class="timer-container">
                        <div class="timer-label">Tijd</div>
                        <div class="timer-value" id="timer">00:00.0</div>
                    </div>
                    <div class="score-container">
                        <div class="score-label">Score</div>
                        <div class="score-value" id="score">0</div>
                    </div>
                    <div class="stats-row">
                        <div class="stat-item">
                            <div class="stat-value hits" id="hits">0</div>
                            <div class="stat-label">Hits</div>
                        </div>
                        <div class="stat-item">
                            <div class="stat-value misses" id="misses">0</div>
                            <div class="stat-label">Misses</div>
                        </div>
                    </div>
                    <div class="controls">
                        <button class="btn btn-start" id="btnStart" onclick="startGame()">
                            ▶ START
                        </button>
                        <button class="btn btn-pause" id="btnPause" onclick="pauseGame()" disabled>
                            ⏸ PAUZE
                        </button>
                        <button class="btn btn-stop" id="btnStop" onclick="stopGame()" disabled>
                            ⏹ STOP
                        </button>
                        <button class="btn btn-reset" onclick="resetGame()">
                            ↺ RESET
                        </button>
                    </div>
                </div>

                <!-- Targets -->
                <div class="card" style="margin-top: 20px;">
                    <div class="card-header">
                        <h2>Targets</h2>
                    </div>
                    <div class="targets-grid" id="targetsGrid">
                        <!-- Generated by JS -->
                    </div>
                    <div class="target-count" id="targetCount">0/8 targets online</div>
                </div>
            </div>

            <!-- Right Column -->
            <div class="right-column">
                <!-- Highscores -->
                <div class="card">
                    <div class="card-header">
                        <h2>🏆 Top 3 van Vandaag</h2>
                    </div>
                    <ul class="highscores-list" id="highscores">
                        <li class="highscore-item">
                            <span class="rank rank-1">#1</span>
                            <span class="highscore-name">---</span>
                            <span class="highscore-score">0</span>
                        </li>
                        <li class="highscore-item">
                            <span class="rank rank-2">#2</span>
                            <span class="highscore-name">---</span>
                            <span class="highscore-score">0</span>
                        </li>
                        <li class="highscore-item">
                            <span class="rank rank-3">#3</span>
                            <span class="highscore-name">---</span>
                            <span class="highscore-score">0</span>
                        </li>
                    </ul>
                    <div class="controls" style="margin-top: 15px;">
                        <button class="btn btn-reset" onclick="loadHighscores()" style="padding: 10px 20px;">
                            Vernieuwen
                        </button>
                        <button class="btn btn-stop" onclick="clearHighscores()" style="padding: 10px 20px;">
                            Wissen
                        </button>
                    </div>
                </div>

                <!-- Event Log -->
                <div class="card" style="margin-top: 20px;">
                    <div class="card-header">
                        <h2>📋 Event Log</h2>
                        <button class="btn btn-reset" onclick="clearLog()" style="padding: 8px 15px; font-size: 0.8rem;">
                            Wissen
                        </button>
                    </div>
                    <div class="log-container" id="log">
                        <!-- Generated by JS -->
                    </div>
                </div>
            </div>
        </div>
        </div><!-- /tab-schieten -->

        <!-- ============================================================ -->
        <!-- TAB: FITNESS TRAINING -->
        <!-- ============================================================ -->
        <div class="tab-content" id="tab-fitness">
            <div class="fitness-grid">
                <!-- Counter Display -->
                <div class="card">
                    <div class="card-header">
                        <h2>Ultrasonic Counter</h2>
                    </div>
                    <div class="counter-display">
                        <div class="count-value" id="fitCountValue">0</div>
                        <div class="count-label">detecties</div>
                    </div>

                    <div class="round-display">
                        <div class="round-label">Ronde</div>
                        <div class="round-value"><span id="fitCurrentRound">0</span> / <span id="fitTotalRounds">--</span></div>
                    </div>

                    <!-- Red Timer -->
                    <div class="red-timer-display" id="fitRedTimerDisplay">
                        <div class="red-timer-text" id="fitRedTimerText">0.0</div>
                        <div class="red-timer-bar">
                            <div class="red-timer-fill" id="fitRedTimerFill" style="width: 100%"></div>
                        </div>
                        <div class="red-timer-label">Tijd om bij sensor te komen!</div>
                    </div>

                    <!-- LED Strip -->
                    <div class="led-strip-visual" id="fitLedStrip"></div>

                    <!-- Progress Ring -->
                    <div class="fitness-progress">
                        <div class="progress-ring">
                            <svg width="100" height="100">
                                <circle class="progress-bg" cx="50" cy="50" r="42"></circle>
                                <circle class="progress-fill" id="fitProgressCircle" cx="50" cy="50" r="42"
                                        stroke-dasharray="263.89" stroke-dashoffset="263.89"></circle>
                            </svg>
                            <div class="progress-text"><span id="fitProgressCount">0</span>/<span id="fitProgressTotal">1</span></div>
                        </div>
                    </div>

                    <!-- Fitness Stats -->
                    <div class="fitness-stats-grid">
                        <div class="fitness-stat-item">
                            <div class="fitness-stat-value" id="fitStatTotal">0</div>
                            <div class="fitness-stat-label">Totaal</div>
                        </div>
                        <div class="fitness-stat-item">
                            <div class="fitness-stat-value" id="fitStatLeds">0/6</div>
                            <div class="fitness-stat-label">LEDs Aan</div>
                        </div>
                        <div class="fitness-stat-item">
                            <div class="fitness-stat-value" id="fitStatRounds">0</div>
                            <div class="fitness-stat-label">Rondes</div>
                        </div>
                        <div class="fitness-stat-item">
                            <div class="fitness-stat-value" id="fitStatSpeed">--</div>
                            <div class="fitness-stat-label">Gem. Snelheid</div>
                        </div>
                        <div class="fitness-stat-item">
                            <div class="fitness-stat-value" id="fitStatTimeouts">0</div>
                            <div class="fitness-stat-label">Timeouts</div>
                        </div>
                        <div class="fitness-stat-item">
                            <div class="fitness-stat-value" id="fitStatDistance">--</div>
                            <div class="fitness-stat-label">Afstand (cm)</div>
                        </div>
                    </div>
                </div>

                <!-- Settings & Controls -->
                <div class="card">
                    <div class="card-header">
                        <h2>Instellingen</h2>
                    </div>

                    <!-- Training Mode -->
                    <div class="fitness-mode-selector">
                        <button class="fitness-mode-btn active" data-fitmode="free" onclick="setFitnessMode('free')">Vrij Tellen</button>
                        <button class="fitness-mode-btn" data-fitmode="timed" onclick="setFitnessMode('timed')">Timed</button>
                        <button class="fitness-mode-btn" data-fitmode="interval" onclick="setFitnessMode('interval')">Interval</button>
                    </div>

                    <div class="fitness-setting-group">
                        <label class="fitness-setting-label">Detecties per LED</label>
                        <div class="fitness-setting-row">
                            <button class="fitness-adj-btn" onclick="adjustFit('fitCountsPerLed', -1)">-</button>
                            <input type="number" class="fitness-setting-input" id="fitCountsPerLed" value="1" min="1" max="100" onchange="applyFit('fitCountsPerLed')">
                            <button class="fitness-adj-btn" onclick="adjustFit('fitCountsPerLed', 1)">+</button>
                        </div>
                        <div class="fitness-hint">Na hoeveel keer hand over sensor = 1 LED aan</div>
                    </div>

                    <div class="fitness-setting-group">
                        <label class="fitness-setting-label">Rode LED timer (seconden)</label>
                        <div class="fitness-setting-row">
                            <button class="fitness-adj-btn" onclick="adjustFit('fitRedTimer', -1)">-</button>
                            <input type="number" class="fitness-setting-input" id="fitRedTimer" value="0" min="0" max="60" onchange="applyFit('fitRedTimer')">
                            <button class="fitness-adj-btn" onclick="adjustFit('fitRedTimer', 1)">+</button>
                        </div>
                        <div class="fitness-hint">0 = uit. LED brandt rood, wordt groen als je op tijd bent</div>
                    </div>

                    <div class="fitness-setting-group">
                        <label class="fitness-setting-label">Aantal rondes</label>
                        <div class="fitness-setting-row">
                            <button class="fitness-adj-btn" onclick="adjustFit('fitRounds', -1)">-</button>
                            <input type="number" class="fitness-setting-input" id="fitRounds" value="0" min="0" max="99" onchange="applyFit('fitRounds')">
                            <button class="fitness-adj-btn" onclick="adjustFit('fitRounds', 1)">+</button>
                        </div>
                        <div class="fitness-hint">0 = oneindig. 6 LEDs vol = 1 ronde</div>
                    </div>

                    <div class="fitness-setting-group">
                        <label class="fitness-setting-label">Detectie afstand (cm)</label>
                        <div class="fitness-setting-row">
                            <button class="fitness-adj-btn" onclick="adjustFit('fitDistance', -5)">-</button>
                            <input type="number" class="fitness-setting-input" id="fitDistance" value="20" min="5" max="100" onchange="applyFit('fitDistance')">
                            <button class="fitness-adj-btn" onclick="adjustFit('fitDistance', 5)">+</button>
                        </div>
                    </div>

                    <div class="fitness-btn-grid">
                        <button class="btn btn-start" id="fitBtnStart" onclick="startFitness()">START</button>
                        <button class="btn btn-stop" id="fitBtnStop" onclick="stopFitness()">STOP</button>
                        <button class="btn btn-pause fitness-btn-full" onclick="resetFitness()">RESET TELLER</button>
                    </div>

                    <!-- Fitness Log -->
                    <div style="margin-top: 20px;">
                        <div class="card-header" style="margin-bottom: 10px;">
                            <h2>Log</h2>
                            <button class="btn btn-reset" onclick="clearFitnessLog()" style="padding: 6px 12px; font-size: 0.75rem;">Wis</button>
                        </div>
                        <div class="fitness-log" id="fitnessLog"></div>
                    </div>
                </div>
            </div>
        </div><!-- /tab-fitness -->

    </div>

    <!-- Connection Toast -->
    <div class="connection-toast disconnected" id="connectionToast">
        <span class="status-dot"></span>
        <span id="connectionText">Niet verbonden</span>
    </div>

    <!-- Game End Overlay -->
    <div class="game-status" id="gameStatus">
        <h2>GAME OVER</h2>
        <div class="final-score" id="finalScore">0</div>
        <p id="highscoreMsg" style="color: var(--gold); margin-bottom: 20px; display: none;">
            🏆 NIEUWE HIGHSCORE! 🏆
        </p>
        <button class="btn btn-start" onclick="closeGameStatus()">OK</button>
    </div>

    <script>
        // ============================================================
        // RAF RTT TRAINING SYSTEM - WEBINTERFACE
        // ============================================================

        let ws;
        const MAX_TARGETS = 8;
        const FIT_NUM_LEDS = 6;

        let gameState = {
            running: false,
            paused: false,
            mode: 'freeplay',
            score: 0,
            hits: 0,
            misses: 0,
            time: 0,
            targets: {}
        };

        // Initialize targets
        for (let i = 1; i <= MAX_TARGETS; i++) {
            gameState.targets[i] = { online: false, state: 'offline' };
        }

        // ============================================================
        // WEBSOCKET
        // ============================================================

        function connect() {
            const host = location.hostname || '192.168.4.1';
            ws = new WebSocket(`ws://${host}:81/ws`);

            ws.onopen = () => {
                console.log('WebSocket connected');
                updateConnectionStatus(true);
                log('Verbonden met master controller', 'info');
                ws.send(JSON.stringify({ cmd: 'getState' }));
                loadHighscores();
            };

            ws.onclose = () => {
                console.log('WebSocket disconnected');
                updateConnectionStatus(false);
                log('Verbinding verbroken...', 'miss');
                setTimeout(connect, 2000);
            };

            ws.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    handleMessage(data);
                } catch (err) {
                    console.error('Parse error:', err);
                }
            };

            ws.onerror = (err) => {
                console.error('WebSocket error:', err);
            };
        }

        function updateConnectionStatus(connected) {
            const dot = document.getElementById('wsStatus');
            const text = document.getElementById('wsStatusText');
            const toast = document.getElementById('connectionToast');
            const toastText = document.getElementById('connectionText');

            if (connected) {
                dot.classList.add('online');
                text.textContent = 'Verbonden';
                toast.className = 'connection-toast connected';
                toastText.textContent = 'Verbonden';
            } else {
                dot.classList.remove('online');
                text.textContent = 'Niet verbonden';
                toast.className = 'connection-toast disconnected';
                toastText.textContent = 'Niet verbonden';
            }
        }

        function send(data) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify(data));
            }
        }

        // ============================================================
        // MESSAGE HANDLERS
        // ============================================================

        function handleMessage(data) {
            switch (data.event) {
                case 'state':
                    handleState(data.data);
                    break;
                case 'hit':
                    handleHit(data.data);
                    break;
                case 'timer':
                    handleTimer(data.data);
                    break;
                case 'score':
                    document.getElementById('score').textContent = data.data.score;
                    gameState.score = data.data.score;
                    break;
                case 'targetUpdate':
                    handleTargetUpdate(data.data);
                    break;
                case 'gameEnd':
                    handleGameEnd(data.data);
                    break;
                case 'highscores':
                    displayHighscores(data.data);
                    break;
                // Fitness counter events
                case 'counterUpdate':
                    handleFitnessUpdate(data.data);
                    break;
                case 'counterDetection':
                    handleFitnessDetection(data.data);
                    break;
                case 'counterFull':
                    handleFitnessFull(data.data);
                    break;
                case 'counterReset':
                    handleFitnessReset(data.data);
                    break;
                case 'counterTimeout':
                    handleFitnessTimeout(data.data);
                    break;
            }
        }

        function handleState(data) {
            gameState = { ...gameState, ...data };

            // Update UI
            document.getElementById('btnStart').disabled = gameState.running;
            document.getElementById('btnPause').disabled = !gameState.running;
            document.getElementById('btnStop').disabled = !gameState.running;
            document.getElementById('btnPause').innerHTML = gameState.paused ? '▶ HERVAT' : '⏸ PAUZE';

            // Mode buttons
            document.querySelectorAll('.mode-btn').forEach(btn => {
                btn.classList.toggle('active', btn.dataset.mode === gameState.mode);
            });

            // Timer & Score
            document.getElementById('timer').textContent = formatTime(gameState.time);
            document.getElementById('score').textContent = gameState.score;

            // Update targets from state
            if (data.targets) {
                Object.keys(data.targets).forEach(id => {
                    gameState.targets[id] = data.targets[id];
                });
                updateTargetsUI();
            }
        }

        function handleHit(data) {
            const target = document.querySelector(`[data-target="${data.target}"]`);
            if (target) {
                target.classList.add('hit');
                setTimeout(() => target.classList.remove('hit'), 300);
            }

            document.getElementById('score').textContent = data.totalScore;
            gameState.score = data.totalScore;

            if (data.points > 0) {
                gameState.hits++;
                document.getElementById('hits').textContent = gameState.hits;
                log(`Target ${data.target} geraakt! +${data.points} punten`, 'hit');
            } else if (data.points < 0) {
                gameState.misses++;
                document.getElementById('misses').textContent = gameState.misses;
                log(`Target ${data.target} - ${data.points} punten`, 'miss');
            }
        }

        function handleTimer(data) {
            document.getElementById('timer').textContent = formatTime(data.time);
            gameState.time = data.time;

            // Warning color when < 10 seconds remaining
            const remaining = data.remaining || 0;
            const timerEl = document.getElementById('timer');
            if (remaining < 10000 && remaining > 0) {
                timerEl.classList.add('warning');
            } else {
                timerEl.classList.remove('warning');
            }
        }

        function handleTargetUpdate(data) {
            gameState.targets[data.id] = data;
            updateTargetUI(data.id);
            updateTargetCount();
        }

        function handleGameEnd(data) {
            gameState.running = false;
            document.getElementById('btnStart').disabled = false;
            document.getElementById('btnPause').disabled = true;
            document.getElementById('btnStop').disabled = true;

            // Show overlay
            document.getElementById('finalScore').textContent = data.score;
            document.getElementById('highscoreMsg').style.display = data.isHighscore ? 'block' : 'none';
            document.getElementById('gameStatus').classList.add('show');

            log(`Game afgelopen! Score: ${data.score} | Hits: ${data.hits} | Misses: ${data.misses}`, 'info');

            loadHighscores();
        }

        // ============================================================
        // UI FUNCTIONS
        // ============================================================

        function initTargets() {
            const grid = document.getElementById('targetsGrid');
            grid.innerHTML = '';

            for (let i = 1; i <= MAX_TARGETS; i++) {
                const target = document.createElement('div');
                target.className = 'target offline';
                target.dataset.target = i;
                target.innerHTML = `
                    <span class="target-num">${i}</span>
                    <span class="target-status">OFFLINE</span>
                `;
                target.onclick = () => activateTarget(i);
                grid.appendChild(target);
            }
        }

        function updateTargetsUI() {
            for (let i = 1; i <= MAX_TARGETS; i++) {
                updateTargetUI(i);
            }
            updateTargetCount();
        }

        function updateTargetUI(id) {
            const target = document.querySelector(`[data-target="${id}"]`);
            if (!target) return;

            const data = gameState.targets[id];
            const status = target.querySelector('.target-status');

            // Remove all state classes
            target.className = 'target';

            if (!data || !data.online) {
                target.classList.add('offline');
                status.textContent = 'OFFLINE';
            } else {
                switch (data.state) {
                    case 'active':
                        target.classList.add('active');
                        status.textContent = 'ACTIVE';
                        break;
                    case 'noshoot':
                        target.classList.add('noshoot');
                        status.textContent = 'NO SHOOT';
                        break;
                    case 'hit':
                        target.classList.add('hit');
                        status.textContent = 'HIT!';
                        break;
                    default:
                        target.classList.add('inactive');
                        status.textContent = 'READY';
                }
            }
        }

        function updateTargetCount() {
            const online = Object.values(gameState.targets).filter(t => t.online).length;
            document.getElementById('targetCount').textContent = `${online}/${MAX_TARGETS} targets online`;
        }

        function formatTime(ms) {
            const totalSec = Math.floor(ms / 1000);
            const min = Math.floor(totalSec / 60);
            const sec = totalSec % 60;
            const tenths = Math.floor((ms % 1000) / 100);
            return `${min.toString().padStart(2, '0')}:${sec.toString().padStart(2, '0')}.${tenths}`;
        }

        function displayHighscores(scores) {
            const list = document.getElementById('highscores');
            list.innerHTML = '';

            for (let i = 0; i < 3; i++) {
                const s = scores[i] || { name: '---', score: 0 };
                const li = document.createElement('li');
                li.className = 'highscore-item';
                li.innerHTML = `
                    <span class="rank rank-${i + 1}">#${i + 1}</span>
                    <span class="highscore-name">${s.name}</span>
                    <span class="highscore-score">${s.score}</span>
                `;
                list.appendChild(li);
            }
        }

        function log(msg, type = '') {
            const logContainer = document.getElementById('log');
            const entry = document.createElement('div');
            entry.className = 'log-entry ' + type;

            const time = new Date().toLocaleTimeString('nl-NL');
            entry.innerHTML = `<span class="log-time">[${time}]</span><span>${msg}</span>`;

            logContainer.insertBefore(entry, logContainer.firstChild);

            // Limit log entries
            while (logContainer.children.length > 50) {
                logContainer.lastChild.remove();
            }
        }

        function clearLog() {
            document.getElementById('log').innerHTML = '';
            log('Log gewist', 'info');
        }

        // ============================================================
        // GAME CONTROLS
        // ============================================================

        function startGame() {
            const playerName = document.getElementById('playerName').value || 'Speler';
            const gameTime = parseInt(document.getElementById('gameTime').value) || 60;
            const targetTime = parseInt(document.getElementById('targetTime').value) || 3;

            // Reset stats
            gameState.hits = 0;
            gameState.misses = 0;
            document.getElementById('hits').textContent = '0';
            document.getElementById('misses').textContent = '0';

            send({
                cmd: 'start',
                player: playerName,
                gameTime: gameTime,
                targetTime: targetTime
            });

            log(`Game gestart - ${gameState.mode} mode`, 'info');
        }

        function pauseGame() {
            send({ cmd: 'pause' });
            log(gameState.paused ? 'Game hervat' : 'Game gepauzeerd', 'info');
        }

        function stopGame() {
            send({ cmd: 'stop' });
            log('Game gestopt', 'info');
        }

        function resetGame() {
            send({ cmd: 'reset' });
            gameState.hits = 0;
            gameState.misses = 0;
            document.getElementById('hits').textContent = '0';
            document.getElementById('misses').textContent = '0';
            document.getElementById('score').textContent = '0';
            document.getElementById('timer').textContent = '00:00.0';
            log('Game gereset', 'info');
        }

        function setMode(mode) {
            send({ cmd: 'setMode', mode: mode });
            gameState.mode = mode;

            document.querySelectorAll('.mode-btn').forEach(btn => {
                btn.classList.toggle('active', btn.dataset.mode === mode);
            });

            const modeNames = {
                'freeplay': 'Free Play',
                'sequence': 'Sequence',
                'random': 'Random',
                'shootnoshoot': 'Shoot/No Shoot'
            };
            log(`Modus: ${modeNames[mode]}`, 'info');
        }

        function activateTarget(id) {
            send({ cmd: 'activateTarget', target: id });
        }

        function activateAll() {
            send({ cmd: 'activateAll' });
        }

        function loadHighscores() {
            send({ cmd: 'getHighscores' });
        }

        function clearHighscores() {
            if (confirm('Weet je zeker dat je alle highscores wilt wissen?')) {
                send({ cmd: 'clearHighscores' });
                log('Highscores gewist', 'info');
                setTimeout(loadHighscores, 500);
            }
        }

        function closeGameStatus() {
            document.getElementById('gameStatus').classList.remove('show');
        }

        function toggleTVMode() {
            document.body.classList.toggle('tv-mode');
            const isTv = document.body.classList.contains('tv-mode');
            log(isTv ? 'TV Mode ingeschakeld' : 'TV Mode uitgeschakeld', 'info');
        }

        // ============================================================
        // TAB SWITCHING
        // ============================================================

        function switchTab(tab) {
            document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
            document.querySelectorAll('.tab-btn').forEach(btn => btn.classList.remove('active'));
            document.getElementById('tab-' + tab).classList.add('active');
            event.target.classList.add('active');

            if (tab === 'fitness') {
                initFitnessLeds();
                send({cmd: 'getCounterState'});
            }
        }

        // ============================================================
        // FITNESS COUNTER
        // ============================================================

        let fitState = {
            counting: false,
            count: 0,
            ledsLit: 0,
            countsPerLed: 1,
            countsSinceLed: 0,
            currentRound: 0,
            totalRounds: 0,
            redTimerSeconds: 0,
            redTimerActive: false,
            redTimerRemaining: 0,
            detectionDistance: 20,
            mode: 'free',
            timeouts: 0,
            lastDetectionTime: 0,
            avgSpeed: 0,
            detectionTimes: [],
            lastDistance: 0
        };

        let fitRedTimerInterval = null;

        // --- Fitness Event Handlers ---

        function handleFitnessUpdate(data) {
            if (data.count !== undefined) fitState.count = data.count;
            if (data.ledsLit !== undefined) fitState.ledsLit = data.ledsLit;
            if (data.countsPerLed !== undefined) {
                fitState.countsPerLed = data.countsPerLed;
                document.getElementById('fitCountsPerLed').value = data.countsPerLed;
            }
            if (data.countsSinceLed !== undefined) fitState.countsSinceLed = data.countsSinceLed;
            if (data.round !== undefined) fitState.currentRound = data.round;
            if (data.totalRounds !== undefined) fitState.totalRounds = data.totalRounds;
            if (data.counting !== undefined) fitState.counting = data.counting;
            if (data.distance !== undefined) fitState.lastDistance = data.distance;
            updateFitnessUI();
        }

        function handleFitnessDetection(data) {
            fitState.count = data.count || fitState.count + 1;
            fitState.ledsLit = data.ledsLit || fitState.ledsLit;
            fitState.countsSinceLed = data.countsSinceLed || 0;

            const now = Date.now();
            if (fitState.lastDetectionTime > 0) {
                const interval = (now - fitState.lastDetectionTime) / 1000;
                fitState.detectionTimes.push(interval);
                if (fitState.detectionTimes.length > 20) fitState.detectionTimes.shift();
                const avg = fitState.detectionTimes.reduce((a,b) => a+b, 0) / fitState.detectionTimes.length;
                fitState.avgSpeed = avg.toFixed(1);
            }
            fitState.lastDetectionTime = now;

            // Stop red timer if active = on time! Show green
            if (fitState.redTimerActive) {
                stopFitRedTimer();
                showFitGreenSuccess();
                fitnessLog('OP TIJD! LEDs groen!', 'detection');
            }

            // Restart red timer
            if (fitState.redTimerSeconds > 0 && fitState.counting) {
                setTimeout(() => startFitRedTimer(), 800);
            }

            updateFitnessUI();
            fitnessLog(`Detectie #${fitState.count} (LEDs: ${fitState.ledsLit}/${FIT_NUM_LEDS})`, 'detection');
        }

        function handleFitnessFull(data) {
            fitState.ledsLit = FIT_NUM_LEDS;
            fitState.count = data.count || fitState.count;
            celebrateFitLeds();
            fitnessLog('ALLE LEDS VOL!', 'full');

            if (fitState.totalRounds > 0) {
                fitState.currentRound++;
                fitnessLog(`Ronde ${fitState.currentRound}/${fitState.totalRounds} voltooid!`, 'round');

                if (fitState.currentRound >= fitState.totalRounds) {
                    fitnessLog('ALLE RONDES VOLTOOID!', 'full');
                    stopFitness();
                } else {
                    setTimeout(() => {
                        send({cmd: 'counterReset'});
                        fitState.count = 0;
                        fitState.ledsLit = 0;
                        fitState.countsSinceLed = 0;
                        updateFitnessUI();
                        fitnessLog(`Ronde ${fitState.currentRound + 1} gestart...`, 'round');
                        if (fitState.redTimerSeconds > 0) startFitRedTimer();
                    }, 2000);
                }
            }
            updateFitnessUI();
        }

        function handleFitnessReset() {
            fitState.count = 0;
            fitState.ledsLit = 0;
            fitState.countsSinceLed = 0;
            fitState.detectionTimes = [];
            fitState.avgSpeed = 0;
            fitState.lastDetectionTime = 0;
            stopFitRedTimer();
            updateFitnessUI();
            fitnessLog('Teller gereset', 'info');
        }

        function handleFitnessTimeout() {
            fitState.timeouts++;
            updateFitnessUI();
            fitnessLog('TIMEOUT! Te laat bij sensor!', 'timeout');
        }

        // --- Red Timer ---

        function startFitRedTimer() {
            stopFitRedTimer();
            if (fitState.redTimerSeconds <= 0) return;

            fitState.redTimerActive = true;
            fitState.redTimerRemaining = fitState.redTimerSeconds;

            document.getElementById('fitRedTimerDisplay').classList.add('active');
            send({cmd: 'counterRedTimer', seconds: fitState.redTimerSeconds});

            fitRedTimerInterval = setInterval(() => {
                fitState.redTimerRemaining -= 0.1;
                if (fitState.redTimerRemaining <= 0) {
                    fitState.redTimerRemaining = 0;
                    fitState.timeouts++;
                    stopFitRedTimer();
                    send({cmd: 'counterTimeout'});
                    fitnessLog('TIMEOUT! Niet op tijd!', 'timeout');
                    if (fitState.counting && fitState.redTimerSeconds > 0) {
                        setTimeout(() => startFitRedTimer(), 1000);
                    }
                }
                updateFitRedTimerUI();
            }, 100);
            updateFitRedTimerUI();
        }

        function stopFitRedTimer() {
            fitState.redTimerActive = false;
            if (fitRedTimerInterval) {
                clearInterval(fitRedTimerInterval);
                fitRedTimerInterval = null;
            }
            document.getElementById('fitRedTimerDisplay').classList.remove('active');
        }

        function updateFitRedTimerUI() {
            const pct = (fitState.redTimerRemaining / fitState.redTimerSeconds) * 100;
            document.getElementById('fitRedTimerFill').style.width = pct + '%';
            document.getElementById('fitRedTimerText').textContent = fitState.redTimerRemaining.toFixed(1);
        }

        // --- Green Success Flash ---

        function showFitGreenSuccess() {
            const strip = document.getElementById('fitLedStrip');
            const leds = strip.querySelectorAll('.led-dot');
            leds.forEach(led => {
                led.style.backgroundColor = '#00ff88';
                led.style.color = '#00ff88';
                led.classList.add('on');
                led.style.boxShadow = '0 0 20px #00ff88, 0 0 40px #00ff88';
            });
            const countEl = document.getElementById('fitCountValue');
            countEl.style.background = '#00ff88';
            countEl.style.webkitBackgroundClip = 'text';
            setTimeout(() => {
                updateFitnessLedStrip();
                countEl.style.background = 'linear-gradient(135deg, var(--primary), var(--secondary))';
                countEl.style.webkitBackgroundClip = 'text';
            }, 600);
        }

        function celebrateFitLeds() {
            const strip = document.getElementById('fitLedStrip');
            const leds = strip.querySelectorAll('.led-dot');
            let hue = 0;
            const interval = setInterval(() => {
                leds.forEach((led, i) => {
                    const h = (hue + i * 60) % 360;
                    led.style.backgroundColor = `hsl(${h}, 100%, 50%)`;
                    led.style.color = `hsl(${h}, 100%, 50%)`;
                    led.classList.add('on');
                });
                hue += 10;
            }, 50);
            setTimeout(() => {
                clearInterval(interval);
                leds.forEach(led => {
                    led.style.backgroundColor = '#ffd700';
                    led.style.color = '#ffd700';
                });
            }, 5000);
        }

        // --- Controls ---

        function startFitness() {
            fitState.counting = true;
            fitState.currentRound = 0;
            fitState.timeouts = 0;
            fitState.totalRounds = parseInt(document.getElementById('fitRounds').value) || 0;
            fitState.countsPerLed = parseInt(document.getElementById('fitCountsPerLed').value) || 1;
            fitState.redTimerSeconds = parseInt(document.getElementById('fitRedTimer').value) || 0;
            fitState.detectionDistance = parseInt(document.getElementById('fitDistance').value) || 20;

            send({
                cmd: 'counterStart',
                countsPerLed: fitState.countsPerLed,
                redTimer: fitState.redTimerSeconds,
                rounds: fitState.totalRounds,
                distance: fitState.detectionDistance
            });

            document.getElementById('fitTotalRounds').textContent = fitState.totalRounds || '--';
            document.getElementById('fitCurrentRound').textContent = '0';
            fitnessLog(`Training gestart! ${fitState.totalRounds > 0 ? fitState.totalRounds + ' rondes' : 'Oneindig'}, ${fitState.countsPerLed} per LED`, 'info');
            if (fitState.redTimerSeconds > 0) setTimeout(() => startFitRedTimer(), 500);
            updateFitnessUI();
        }

        function stopFitness() {
            fitState.counting = false;
            stopFitRedTimer();
            send({cmd: 'counterStop'});
            fitnessLog('Training gestopt', 'info');
            updateFitnessUI();
        }

        function resetFitness() {
            fitState.count = 0;
            fitState.ledsLit = 0;
            fitState.countsSinceLed = 0;
            fitState.currentRound = 0;
            fitState.timeouts = 0;
            fitState.detectionTimes = [];
            fitState.avgSpeed = 0;
            stopFitRedTimer();
            send({cmd: 'counterReset'});
            fitnessLog('Teller gereset', 'info');
            updateFitnessUI();
        }

        function setFitnessMode(mode) {
            fitState.mode = mode;
            document.querySelectorAll('.fitness-mode-btn').forEach(btn => {
                btn.classList.toggle('active', btn.dataset.fitmode === mode);
            });
            if (mode === 'timed') {
                document.getElementById('fitRedTimer').value = 5;
                fitState.redTimerSeconds = 5;
                document.getElementById('fitRounds').value = 1;
            } else if (mode === 'interval') {
                document.getElementById('fitRedTimer').value = 3;
                fitState.redTimerSeconds = 3;
                document.getElementById('fitRounds').value = 5;
            }
            send({cmd: 'counterSetMode', mode: mode});
        }

        // --- Settings ---

        function adjustFit(id, delta) {
            const input = document.getElementById(id);
            let val = parseInt(input.value) + delta;
            val = Math.max(parseInt(input.min), Math.min(parseInt(input.max), val));
            input.value = val;
            applyFit(id);
        }

        function applyFit(id) {
            const val = parseInt(document.getElementById(id).value);
            switch(id) {
                case 'fitCountsPerLed':
                    fitState.countsPerLed = val;
                    send({cmd: 'counterSetCountsPerLed', value: val});
                    document.getElementById('fitProgressTotal').textContent = val;
                    break;
                case 'fitRedTimer':
                    fitState.redTimerSeconds = val;
                    send({cmd: 'counterSetRedTimer', value: val});
                    break;
                case 'fitRounds':
                    fitState.totalRounds = val;
                    document.getElementById('fitTotalRounds').textContent = val || '--';
                    send({cmd: 'counterSetRounds', value: val});
                    break;
                case 'fitDistance':
                    fitState.detectionDistance = val;
                    send({cmd: 'counterSetDistance', value: val});
                    break;
            }
        }

        // --- UI ---

        function updateFitnessUI() {
            document.getElementById('fitCountValue').textContent = fitState.count;
            document.getElementById('fitCurrentRound').textContent = fitState.currentRound;
            document.getElementById('fitTotalRounds').textContent = fitState.totalRounds || '--';

            updateFitnessLedStrip();

            // Progress ring
            const circumference = 263.89;
            const offset = circumference - (fitState.countsSinceLed / fitState.countsPerLed) * circumference;
            document.getElementById('fitProgressCircle').style.strokeDashoffset = offset;
            document.getElementById('fitProgressCount').textContent = fitState.countsSinceLed;
            document.getElementById('fitProgressTotal').textContent = fitState.countsPerLed;

            // Stats
            document.getElementById('fitStatTotal').textContent = fitState.count;
            document.getElementById('fitStatLeds').textContent = `${fitState.ledsLit}/${FIT_NUM_LEDS}`;
            document.getElementById('fitStatRounds').textContent = fitState.currentRound;
            document.getElementById('fitStatSpeed').textContent = fitState.avgSpeed > 0 ? fitState.avgSpeed + 's' : '--';
            document.getElementById('fitStatTimeouts').textContent = fitState.timeouts;
            document.getElementById('fitStatDistance').textContent = fitState.lastDistance > 0 ? fitState.lastDistance.toFixed(0) : '--';

            // Buttons
            document.getElementById('fitBtnStart').disabled = fitState.counting;
            document.getElementById('fitBtnStop').disabled = !fitState.counting;
        }

        function initFitnessLeds() {
            updateFitnessLedStrip();
        }

        function updateFitnessLedStrip() {
            const strip = document.getElementById('fitLedStrip');
            strip.innerHTML = '';
            for (let i = 0; i < FIT_NUM_LEDS; i++) {
                const led = document.createElement('div');
                led.className = 'led-dot' + (i < fitState.ledsLit ? ' on' : '');
                led.id = 'fit-led-' + i;
                if (i < fitState.ledsLit) {
                    const progress = FIT_NUM_LEDS > 1 ? i / (FIT_NUM_LEDS - 1) : 0;
                    const r = Math.round(progress * 255);
                    const g = Math.round(255 - (progress * 85));
                    led.style.backgroundColor = `rgb(${r}, ${g}, 0)`;
                    led.style.color = `rgb(${r}, ${g}, 0)`;
                }
                strip.appendChild(led);
            }
        }

        function fitnessLog(msg, type = '') {
            const logDiv = document.getElementById('fitnessLog');
            const entry = document.createElement('div');
            entry.className = 'log-entry ' + type;
            const time = new Date().toLocaleTimeString('nl-NL');
            entry.innerHTML = `<span class="log-time">[${time}]</span><span>${msg}</span>`;
            logDiv.insertBefore(entry, logDiv.firstChild);
            if (logDiv.children.length > 50) logDiv.lastChild.remove();
        }

        function clearFitnessLog() {
            document.getElementById('fitnessLog').innerHTML = '';
        }

        // ============================================================
        // INITIALIZATION
        // ============================================================

        document.addEventListener('DOMContentLoaded', () => {
            initTargets();
            initFitnessLeds();
            connect();
            log('Raf RTT Training System gestart', 'info');

            // Keyboard shortcuts
            document.addEventListener('keydown', (e) => {
                if (e.target.tagName === 'INPUT') return;

                switch (e.key) {
                    case ' ':
                        e.preventDefault();
                        if (!gameState.running) startGame();
                        else pauseGame();
                        break;
                    case 'Escape':
                        if (gameState.running) stopGame();
                        break;
                    case 'r':
                        resetGame();
                        break;
                    case '1': case '2': case '3': case '4':
                    case '5': case '6': case '7': case '8':
                        activateTarget(parseInt(e.key));
                        break;
                    case 't':
                        toggleTVMode();
                        break;
                }
            });
        });
    </script>
</body>
</html>

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
        if (counterState.ledsLit < 6) {
            counterState.ledsLit++;
        }

        // Alle LEDs vol?
        if (counterState.ledsLit >= 6) {
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

    Serial.printf("Counter detectie: %d (LEDs: %d/6, Ronde: %d)\n",
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
