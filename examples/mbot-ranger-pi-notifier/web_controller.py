#!/usr/bin/env python3
"""
mBot Ranger Web Controller - Interface Web pour Raspberry Pi

Ce script lance un serveur web permettant de contrôler le mBot Ranger
depuis n'importe quel navigateur sur le réseau local.
Inclut le streaming vidéo de la caméra Pi.

Prérequis:
    pip3 install flask flask-socketio pyserial simple-websocket

    Pour la caméra (optionnel):
    pip3 install picamera2  # Pour Pi Camera (recommandé)
    # ou
    pip3 install opencv-python  # Pour webcam USB

Usage:
    python3 web_controller.py [--port 5000] [--serial /dev/ttyUSB0]

Accès:
    http://raspberry-pi-ip:5000
"""

import serial
import json
import time
import argparse
import logging
import threading
from datetime import datetime
from flask import Flask, render_template_string, jsonify, request, Response
from flask_socketio import SocketIO, emit

# Configuration Flask
app = Flask(__name__)
app.config['SECRET_KEY'] = 'mbot-ranger-secret-key-2024'

# SocketIO avec mode threading (compatible sans eventlet/gevent)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading',
                    ping_timeout=10, ping_interval=5)

# Variables globales
serial_port = None
serial_lock = threading.Lock()
robot_connected = False
monitoring = False
last_status = {}
alert_history = []

# Camera
camera = None
camera_streaming = False
camera_lock = threading.Lock()
camera_type = None
camera_rotation = 270  # Rotation en degrés (0, 90, 180, 270) - 270 = rotation gauche

# Person detection
person_detection_enabled = False
person_detected = False
person_detection_cooldown = 5  # Secondes entre alertes
person_detection_sensitivity = 0.5  # 0.0 (très sensible) à 1.0 (peu sensible)
person_alarm_enabled = True  # Jouer l'alarme sur le robot
person_led_enabled = True  # Allumer LED rouge
last_person_alert = 0
hog_detector = None

# Logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# ==================== PAGE HTML ====================

HTML_PAGE = """
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>mBot Ranger Controller</title>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/socket.io/4.6.0/socket.io.min.js"></script>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            min-height: 100vh;
            color: #fff;
            padding: 20px;
        }
        .container { max-width: 1200px; margin: 0 auto; }
        h1 { text-align: center; margin-bottom: 20px; font-size: 1.8em; }
        .main-layout { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
        @media (max-width: 900px) { .main-layout { grid-template-columns: 1fr; } }
        .status-bar {
            background: rgba(255,255,255,0.1);
            border-radius: 10px;
            padding: 15px;
            margin-bottom: 20px;
            display: flex;
            justify-content: space-around;
            flex-wrap: wrap;
            gap: 10px;
        }
        .status-item { display: flex; align-items: center; gap: 8px; }
        .status-dot {
            width: 12px; height: 12px;
            border-radius: 50%;
            background: #ff4444;
            transition: all 0.3s;
        }
        .status-dot.on { background: #44ff44; box-shadow: 0 0 10px #44ff44; }
        .status-dot.pending { background: #ffaa44; animation: blink 0.5s infinite; }
        @keyframes blink { 50% { opacity: 0.5; } }
        .panel {
            background: rgba(255,255,255,0.1);
            border-radius: 15px;
            padding: 20px;
            margin-bottom: 20px;
        }
        .panel h2 { margin-bottom: 15px; font-size: 1.1em; color: #88ccff; }
        .camera-container {
            position: relative;
            background: #000;
            border-radius: 10px;
            overflow: hidden;
            aspect-ratio: 4/3;
        }
        .camera-feed { width: 100%; height: 100%; object-fit: contain; display: none; }
        .camera-placeholder {
            width: 100%; height: 100%;
            display: flex; align-items: center; justify-content: center;
            color: #666; font-size: 1em;
        }
        .camera-overlay {
            position: absolute; top: 10px; left: 10px;
            background: rgba(255,0,0,0.8);
            padding: 3px 8px; border-radius: 5px;
            font-size: 0.7em; display: none;
        }
        .camera-controls { display: flex; gap: 10px; margin-top: 10px; justify-content: center; }
        .controls-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 8px;
            max-width: 240px;
            margin: 0 auto;
        }
        .btn {
            background: linear-gradient(145deg, #3a3a5c, #2a2a4c);
            border: none; border-radius: 10px;
            padding: 18px; color: #fff; font-size: 1.3em;
            cursor: pointer; transition: all 0.15s;
            user-select: none;
        }
        .btn:hover { transform: translateY(-2px); background: linear-gradient(145deg, #4a4a6c, #3a3a5c); }
        .btn:active { transform: translateY(0); background: linear-gradient(145deg, #2a2a4c, #1a1a3c); }
        .btn.stop { background: linear-gradient(145deg, #cc4444, #aa2222); }
        .btn-row { display: flex; gap: 8px; flex-wrap: wrap; justify-content: center; }
        .btn-small {
            background: linear-gradient(145deg, #3a5c3a, #2a4c2a);
            border: none; border-radius: 8px;
            padding: 10px 16px; color: #fff; font-size: 0.85em;
            cursor: pointer; transition: all 0.15s;
        }
        .btn-small:hover { transform: translateY(-1px); }
        .btn-small:active { transform: translateY(0); }
        .btn-small.red { background: linear-gradient(145deg, #cc4444, #aa2222); }
        .btn-small.green { background: linear-gradient(145deg, #44cc44, #22aa22); }
        .btn-small.blue { background: linear-gradient(145deg, #4444cc, #2222aa); }
        .btn-small.yellow { background: linear-gradient(145deg, #cccc44, #aaaa22); }
        .btn-small.purple { background: linear-gradient(145deg, #cc44cc, #aa22aa); }
        /* Joystick styles */
        .joystick-container {
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 15px;
        }
        .joystick-area {
            position: relative;
            width: 180px;
            height: 180px;
            background: radial-gradient(circle, #2a2a4c 0%, #1a1a2e 100%);
            border-radius: 50%;
            border: 3px solid #3a3a5c;
            touch-action: none;
            user-select: none;
        }
        .joystick-base {
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            width: 100px;
            height: 100px;
            background: radial-gradient(circle, #4a4a6c 0%, #3a3a5c 100%);
            border-radius: 50%;
            opacity: 0.3;
        }
        .joystick-stick {
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            width: 60px;
            height: 60px;
            background: radial-gradient(circle at 30% 30%, #88ccff, #4488cc);
            border-radius: 50%;
            box-shadow: 0 4px 15px rgba(0,0,0,0.4), inset 0 2px 10px rgba(255,255,255,0.2);
            cursor: grab;
            transition: box-shadow 0.1s;
        }
        .joystick-stick:active { cursor: grabbing; box-shadow: 0 2px 10px rgba(136,204,255,0.6); }
        .joystick-stick.active { box-shadow: 0 0 20px rgba(136,204,255,0.8); }
        .joystick-directions {
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            width: 160px;
            height: 160px;
            pointer-events: none;
        }
        .joystick-directions span {
            position: absolute;
            color: #555;
            font-size: 1.2em;
            font-weight: bold;
        }
        .joystick-directions .up { top: 5px; left: 50%; transform: translateX(-50%); }
        .joystick-directions .down { bottom: 5px; left: 50%; transform: translateX(-50%); }
        .joystick-directions .left { left: 5px; top: 50%; transform: translateY(-50%); }
        .joystick-directions .right { right: 5px; top: 50%; transform: translateY(-50%); }
        .speed-control {
            display: flex;
            align-items: center;
            gap: 10px;
            width: 100%;
            max-width: 200px;
        }
        .speed-control label { color: #888; font-size: 0.85em; white-space: nowrap; }
        .speed-slider {
            flex: 1;
            -webkit-appearance: none;
            height: 8px;
            border-radius: 4px;
            background: linear-gradient(90deg, #2a4c2a, #44cc44);
            outline: none;
        }
        .speed-slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #fff;
            cursor: pointer;
            box-shadow: 0 2px 6px rgba(0,0,0,0.3);
        }
        .speed-value {
            color: #88ff88;
            font-weight: bold;
            min-width: 45px;
            text-align: right;
        }
        .joystick-info {
            display: flex;
            gap: 20px;
            font-size: 0.8em;
            color: #888;
        }
        .joystick-info span { color: #88ccff; }
        .detection-controls { display: flex; flex-direction: column; gap: 5px; }
        .detection-row { display: flex; align-items: center; gap: 10px; }
        .detection-row label { display: flex; align-items: center; cursor: pointer; }
        .detection-row input[type="checkbox"] { width: 16px; height: 16px; cursor: pointer; }
        .sensor-display { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; }
        .sensor-card {
            background: rgba(0,0,0,0.2);
            border-radius: 10px; padding: 12px; text-align: center;
        }
        .sensor-value { font-size: 1.4em; font-weight: bold; color: #88ff88; }
        .sensor-label { font-size: 0.7em; color: #aaa; margin-top: 3px; }
        .log-area {
            background: rgba(0,0,0,0.3);
            border-radius: 8px; padding: 10px;
            height: 120px; overflow-y: auto;
            font-family: monospace; font-size: 0.75em;
            color: #aaa;
        }
        .log-area .ok { color: #88ff88; }
        .log-area .err { color: #ff8888; }
        .log-area .warn { color: #ffaa44; }
    </style>
</head>
<body>
    <div class="container">
        <h1>mBot Ranger Controller</h1>

        <div class="status-bar">
            <div class="status-item">
                <div class="status-dot" id="wsDot"></div>
                <span id="wsStatus">WebSocket: --</span>
            </div>
            <div class="status-item">
                <div class="status-dot" id="robotDot"></div>
                <span id="robotStatus">Robot: --</span>
            </div>
            <div class="status-item">
                <div class="status-dot" id="cameraDot"></div>
                <span id="cameraStatus">Camera: --</span>
            </div>
            <div class="status-item">
                <div class="status-dot" id="detectionDot"></div>
                <span id="detectionStatus">Detection: --</span>
            </div>
            <div class="status-item">
                <span id="uptime">Uptime: --</span>
            </div>
        </div>

        <div class="main-layout">
            <div class="left-column">
                <div class="panel">
                    <h2>Camera Pi</h2>
                    <div class="camera-container">
                        <div class="camera-placeholder" id="cameraPlaceholder">Camera arretee</div>
                        <img class="camera-feed" id="cameraFeed" alt="Camera">
                        <div class="camera-overlay" id="cameraOverlay">LIVE</div>
                    </div>
                    <div class="camera-controls">
                        <button class="btn-small green" onclick="startCamera()">Demarrer</button>
                        <button class="btn-small red" onclick="stopCamera()">Arreter</button>
                        <button class="btn-small purple" onclick="snapshot()">Photo</button>
                    </div>
                    <div class="camera-controls" style="margin-top: 8px;">
                        <span style="color: #888; font-size: 0.8em;">Rotation:</span>
                        <button class="btn-small" onclick="rotateCamera(0)">0°</button>
                        <button class="btn-small" onclick="rotateCamera(90)">90°</button>
                        <button class="btn-small" onclick="rotateCamera(180)">180°</button>
                        <button class="btn-small" onclick="rotateCamera(270)">270°</button>
                    </div>
                </div>

                <div class="panel">
                    <h2>Detection Personne</h2>
                    <div class="detection-controls">
                        <div class="detection-row">
                            <button class="btn-small green" id="btnDetectOn" onclick="toggleDetection(true)">Activer</button>
                            <button class="btn-small red" id="btnDetectOff" onclick="toggleDetection(false)">Desactiver</button>
                            <span id="detectStatus" style="margin-left: 10px; color: #888;">Inactive</span>
                        </div>
                        <div class="detection-row" style="margin-top: 10px;">
                            <label style="color: #888; font-size: 0.85em; min-width: 80px;">Sensibilite:</label>
                            <input type="range" class="speed-slider" id="sensitivitySlider" min="0" max="100" value="50" style="flex:1;">
                            <span id="sensitivityValue" style="color: #88ff88; min-width: 40px; text-align: right;">50%</span>
                        </div>
                        <div class="detection-row" style="margin-top: 8px;">
                            <label style="color: #888; font-size: 0.85em; min-width: 80px;">Cooldown:</label>
                            <input type="range" class="speed-slider" id="cooldownSlider" min="1" max="30" value="5" style="flex:1;">
                            <span id="cooldownValue" style="color: #88ff88; min-width: 40px; text-align: right;">5s</span>
                        </div>
                        <div class="detection-row" style="margin-top: 10px;">
                            <label style="color: #888; font-size: 0.85em;">
                                <input type="checkbox" id="alarmEnabled" checked style="margin-right: 5px;">
                                Alarme sonore
                            </label>
                            <label style="color: #888; font-size: 0.85em; margin-left: 15px;">
                                <input type="checkbox" id="ledEnabled" checked style="margin-right: 5px;">
                                LED rouge
                            </label>
                        </div>
                    </div>
                </div>

                <div class="panel">
                    <h2>Controles</h2>
                    <div class="joystick-container">
                        <div class="joystick-area" id="joystickArea">
                            <div class="joystick-directions">
                                <span class="up">^</span>
                                <span class="down">v</span>
                                <span class="left">&lt;</span>
                                <span class="right">&gt;</span>
                            </div>
                            <div class="joystick-base"></div>
                            <div class="joystick-stick" id="joystickStick"></div>
                        </div>
                        <div class="speed-control">
                            <label>Vitesse:</label>
                            <input type="range" class="speed-slider" id="speedSlider" min="50" max="255" value="150">
                            <span class="speed-value" id="speedValue">150</span>
                        </div>
                        <div class="joystick-info">
                            <div>X: <span id="joystickX">0</span></div>
                            <div>Y: <span id="joystickY">0</span></div>
                        </div>
                        <button class="btn stop" onclick="cmdMove('stop')" style="margin-top: 5px;">STOP</button>
                    </div>
                </div>
            </div>

            <div class="right-column">
                <div class="panel">
                    <h2>LEDs & Sons</h2>
                    <div class="btn-row">
                        <button class="btn-small red" onclick="cmd('led_red')">Rouge</button>
                        <button class="btn-small green" onclick="cmd('led_green')">Vert</button>
                        <button class="btn-small blue" onclick="cmd('led_blue')">Bleu</button>
                        <button class="btn-small" onclick="cmd('led_off')">Off</button>
                        <button class="btn-small yellow" onclick="cmd('beep')">Beep</button>
                        <button class="btn-small purple" onclick="cmd('alarm')">Alarme</button>
                    </div>
                </div>

                <div class="panel">
                    <h2>Capteurs</h2>
                    <div class="sensor-display">
                        <div class="sensor-card">
                            <div class="sensor-value" id="distanceValue">--</div>
                            <div class="sensor-label">Distance (cm)</div>
                        </div>
                        <div class="sensor-card">
                            <div class="sensor-value" id="lightValue">--</div>
                            <div class="sensor-label">Lumiere</div>
                        </div>
                        <div class="sensor-card">
                            <div class="sensor-value" id="baselineValue">--</div>
                            <div class="sensor-label">Baseline</div>
                        </div>
                    </div>
                </div>

                <div class="panel">
                    <h2>Monitoring</h2>
                    <div class="btn-row">
                        <button class="btn-small green" onclick="cmd('start')">Start</button>
                        <button class="btn-small red" onclick="cmd('stop')">Stop</button>
                        <button class="btn-small yellow" onclick="cmd('calibrate')">Calibrer</button>
                        <button class="btn-small blue" onclick="cmd('status')">Status</button>
                    </div>
                </div>

                <div class="panel">
                    <h2>Log</h2>
                    <div class="log-area" id="logArea"></div>
                </div>
            </div>
        </div>
    </div>

    <script>
        let socket = null;
        let reconnectTimer = null;
        let cameraActive = false;

        function log(msg, type) {
            const area = document.getElementById('logArea');
            const time = new Date().toLocaleTimeString();
            const cls = type || '';
            area.innerHTML = `<div class="${cls}">[${time}] ${msg}</div>` + area.innerHTML;
            while (area.children.length > 50) area.removeChild(area.lastChild);
        }

        function updateWsStatus(status) {
            const dot = document.getElementById('wsDot');
            const text = document.getElementById('wsStatus');
            dot.className = 'status-dot';
            if (status === 'connected') {
                dot.classList.add('on');
                text.textContent = 'WebSocket: OK';
            } else if (status === 'connecting') {
                dot.classList.add('pending');
                text.textContent = 'WebSocket: ...';
            } else {
                text.textContent = 'WebSocket: OFF';
            }
        }

        function connectWebSocket() {
            if (socket && socket.connected) return;

            updateWsStatus('connecting');
            log('Connexion WebSocket...', 'warn');

            socket = io({
                transports: ['websocket', 'polling'],
                reconnection: true,
                reconnectionDelay: 1000,
                reconnectionDelayMax: 5000,
                timeout: 10000
            });

            socket.on('connect', function() {
                updateWsStatus('connected');
                log('WebSocket connecte', 'ok');
                socket.emit('get_status');
            });

            socket.on('disconnect', function() {
                updateWsStatus('disconnected');
                log('WebSocket deconnecte', 'err');
            });

            socket.on('connect_error', function(err) {
                updateWsStatus('disconnected');
                log('Erreur connexion: ' + err.message, 'err');
            });

            socket.on('status', function(data) {
                // Robot status
                const robotDot = document.getElementById('robotDot');
                const robotText = document.getElementById('robotStatus');
                if (data.robot_connected) {
                    robotDot.classList.add('on');
                    robotText.textContent = 'Robot: OK';
                } else {
                    robotDot.classList.remove('on');
                    robotText.textContent = 'Robot: OFF';
                }

                // Camera status
                const camDot = document.getElementById('cameraDot');
                const camText = document.getElementById('cameraStatus');
                if (data.camera_streaming) {
                    camDot.classList.add('on');
                    camText.textContent = 'Camera: ON';
                    if (!cameraActive) showCameraFeed();
                } else if (data.camera_available) {
                    camDot.classList.remove('on');
                    camText.textContent = 'Camera: OFF';
                } else {
                    camDot.classList.remove('on');
                    camText.textContent = 'Camera: N/A';
                }

                // Detection status
                const detectDot = document.getElementById('detectionDot');
                const detectText = document.getElementById('detectionStatus');
                if (data.person_detected) {
                    detectDot.classList.add('on');
                    detectDot.style.background = '#ff4444';
                    detectDot.style.boxShadow = '0 0 10px #ff4444';
                    detectText.textContent = 'PERSONNE!';
                    detectText.style.color = '#ff4444';
                } else if (data.detection_enabled) {
                    detectDot.classList.add('on');
                    detectDot.style.background = '#44ff44';
                    detectDot.style.boxShadow = '0 0 10px #44ff44';
                    detectText.textContent = 'Detection: ON';
                    detectText.style.color = '#fff';
                } else {
                    detectDot.classList.remove('on');
                    detectDot.style.background = '#ff4444';
                    detectDot.style.boxShadow = 'none';
                    detectText.textContent = 'Detection: OFF';
                    detectText.style.color = '#fff';
                }

                // Sensors
                if (data.sensors) {
                    const s = data.sensors;
                    if (s.distance !== undefined)
                        document.getElementById('distanceValue').textContent = s.distance.toFixed(1);
                    if (s.light !== undefined)
                        document.getElementById('lightValue').textContent = s.light;
                    if (s.baseline !== undefined)
                        document.getElementById('baselineValue').textContent = s.baseline.toFixed(1);
                    if (s.uptime !== undefined)
                        document.getElementById('uptime').textContent = 'Uptime: ' + s.uptime + 's';
                }
            });

            socket.on('cmd_result', function(data) {
                if (data.success) {
                    log('OK: ' + data.command, 'ok');
                } else {
                    log('Erreur: ' + data.command, 'err');
                }
            });

            socket.on('alert', function(data) {
                log('ALERTE: ' + data.alert + ' - ' + data.message, 'warn');
            });

            socket.on('robot_message', function(data) {
                log('Robot: ' + (data.message || JSON.stringify(data)));
            });
        }

        function cmd(command) {
            if (socket && socket.connected) {
                socket.emit('command', {command: command});
                log('> ' + command);
            } else {
                // Fallback HTTP
                fetch('/api/command', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({command: command})
                }).then(r => r.json()).then(data => {
                    log((data.success ? 'OK' : 'ERR') + ': ' + command, data.success ? 'ok' : 'err');
                }).catch(e => log('Erreur: ' + e, 'err'));
            }
        }

        function startCamera() {
            log('Demarrage camera...');
            fetch('/api/camera/start', {method: 'POST'})
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    log('Camera demarree', 'ok');
                    showCameraFeed();
                } else {
                    log('Erreur camera: ' + (data.error || '?'), 'err');
                }
            }).catch(e => log('Erreur: ' + e, 'err'));
        }

        function stopCamera() {
            fetch('/api/camera/stop', {method: 'POST'})
            .then(r => r.json())
            .then(data => {
                log('Camera arretee');
                hideCameraFeed();
            });
        }

        function showCameraFeed() {
            cameraActive = true;
            document.getElementById('cameraPlaceholder').style.display = 'none';
            const feed = document.getElementById('cameraFeed');
            feed.src = '/video_feed?' + Date.now();
            feed.style.display = 'block';
            document.getElementById('cameraOverlay').style.display = 'block';
        }

        function hideCameraFeed() {
            cameraActive = false;
            document.getElementById('cameraPlaceholder').style.display = 'flex';
            document.getElementById('cameraFeed').style.display = 'none';
            document.getElementById('cameraOverlay').style.display = 'none';
        }

        function snapshot() {
            window.open('/snapshot?' + Date.now(), '_blank');
        }

        function rotateCamera(degrees) {
            fetch('/api/camera/rotate', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({rotation: degrees})
            }).then(r => r.json()).then(data => {
                if (data.success) {
                    log('Rotation camera: ' + degrees + '°', 'ok');
                    // Rafraichir le flux
                    if (cameraActive) {
                        const feed = document.getElementById('cameraFeed');
                        feed.src = '/video_feed?' + Date.now();
                    }
                } else {
                    log('Erreur rotation: ' + (data.error || '?'), 'err');
                }
            }).catch(e => log('Erreur: ' + e, 'err'));
        }

        function toggleDetection(enable) {
            const endpoint = enable ? '/api/detection/start' : '/api/detection/stop';
            fetch(endpoint, {method: 'POST'})
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    log('Detection ' + (enable ? 'activee' : 'desactivee'), 'ok');
                    document.getElementById('detectStatus').textContent = enable ? 'Active' : 'Inactive';
                    document.getElementById('detectStatus').style.color = enable ? '#88ff88' : '#888';
                } else {
                    log('Erreur detection: ' + (data.error || '?'), 'err');
                }
            }).catch(e => log('Erreur: ' + e, 'err'));
        }

        function updateDetectionSettings() {
            const sensitivity = parseInt(document.getElementById('sensitivitySlider').value);
            const cooldown = parseInt(document.getElementById('cooldownSlider').value);
            const alarmEnabled = document.getElementById('alarmEnabled').checked;
            const ledEnabled = document.getElementById('ledEnabled').checked;

            fetch('/api/detection/settings', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({
                    sensitivity: sensitivity,
                    cooldown: cooldown,
                    alarm_enabled: alarmEnabled,
                    led_enabled: ledEnabled
                })
            }).then(r => r.json()).then(data => {
                if (data.success) {
                    log('Reglages detection mis a jour', 'ok');
                }
            }).catch(e => log('Erreur: ' + e, 'err'));
        }

        function initDetectionControls() {
            const sensitivitySlider = document.getElementById('sensitivitySlider');
            const sensitivityValue = document.getElementById('sensitivityValue');
            const cooldownSlider = document.getElementById('cooldownSlider');
            const cooldownValue = document.getElementById('cooldownValue');
            const alarmEnabled = document.getElementById('alarmEnabled');
            const ledEnabled = document.getElementById('ledEnabled');

            sensitivitySlider.addEventListener('input', function() {
                sensitivityValue.textContent = this.value + '%';
            });
            sensitivitySlider.addEventListener('change', updateDetectionSettings);

            cooldownSlider.addEventListener('input', function() {
                cooldownValue.textContent = this.value + 's';
            });
            cooldownSlider.addEventListener('change', updateDetectionSettings);

            alarmEnabled.addEventListener('change', updateDetectionSettings);
            ledEnabled.addEventListener('change', updateDetectionSettings);

            // Charger les reglages actuels
            fetch('/api/detection/settings')
            .then(r => r.json())
            .then(data => {
                sensitivitySlider.value = data.sensitivity;
                sensitivityValue.textContent = data.sensitivity + '%';
                cooldownSlider.value = data.cooldown;
                cooldownValue.textContent = data.cooldown + 's';
                alarmEnabled.checked = data.alarm_enabled;
                ledEnabled.checked = data.led_enabled;
            }).catch(e => console.log('Erreur chargement reglages:', e));
        }

        // ==================== JOYSTICK ====================
        let joystickActive = false;
        let joystickX = 0, joystickY = 0;
        let lastCommand = '';
        let commandInterval = null;
        let currentSpeed = 150;

        function initJoystick() {
            const area = document.getElementById('joystickArea');
            const stick = document.getElementById('joystickStick');
            const speedSlider = document.getElementById('speedSlider');
            const speedValue = document.getElementById('speedValue');

            // Speed slider
            speedSlider.addEventListener('input', function() {
                currentSpeed = parseInt(this.value);
                speedValue.textContent = currentSpeed;
            });

            // Mouse events
            area.addEventListener('mousedown', startJoystick);
            document.addEventListener('mousemove', moveJoystick);
            document.addEventListener('mouseup', stopJoystick);

            // Touch events
            area.addEventListener('touchstart', startJoystick, {passive: false});
            document.addEventListener('touchmove', moveJoystick, {passive: false});
            document.addEventListener('touchend', stopJoystick);
        }

        function startJoystick(e) {
            e.preventDefault();
            joystickActive = true;
            document.getElementById('joystickStick').classList.add('active');
            moveJoystick(e);
            // Envoyer commandes periodiquement
            if (commandInterval) clearInterval(commandInterval);
            commandInterval = setInterval(sendJoystickCommand, 100);
        }

        function moveJoystick(e) {
            if (!joystickActive) return;
            e.preventDefault();

            const area = document.getElementById('joystickArea');
            const stick = document.getElementById('joystickStick');
            const rect = area.getBoundingClientRect();
            const centerX = rect.width / 2;
            const centerY = rect.height / 2;
            const maxDist = rect.width / 2 - 30;

            // Get position
            let clientX, clientY;
            if (e.touches) {
                clientX = e.touches[0].clientX;
                clientY = e.touches[0].clientY;
            } else {
                clientX = e.clientX;
                clientY = e.clientY;
            }

            let dx = clientX - rect.left - centerX;
            let dy = clientY - rect.top - centerY;

            // Limit to circle
            const dist = Math.sqrt(dx*dx + dy*dy);
            if (dist > maxDist) {
                dx = dx / dist * maxDist;
                dy = dy / dist * maxDist;
            }

            // Update stick position
            stick.style.left = (centerX + dx) + 'px';
            stick.style.top = (centerY + dy) + 'px';

            // Calculate normalized values (-100 to 100)
            joystickX = Math.round(dx / maxDist * 100);
            joystickY = Math.round(-dy / maxDist * 100); // Invert Y

            document.getElementById('joystickX').textContent = joystickX;
            document.getElementById('joystickY').textContent = joystickY;
        }

        function stopJoystick(e) {
            if (!joystickActive) return;
            joystickActive = false;

            const stick = document.getElementById('joystickStick');
            stick.classList.remove('active');
            stick.style.left = '50%';
            stick.style.top = '50%';

            joystickX = 0;
            joystickY = 0;
            document.getElementById('joystickX').textContent = '0';
            document.getElementById('joystickY').textContent = '0';

            if (commandInterval) {
                clearInterval(commandInterval);
                commandInterval = null;
            }
            cmdMove('stop');
        }

        function sendJoystickCommand() {
            if (!joystickActive) return;

            // Dead zone
            const deadZone = 15;
            if (Math.abs(joystickX) < deadZone && Math.abs(joystickY) < deadZone) {
                if (lastCommand !== 'stop') {
                    cmdMove('stop');
                    lastCommand = 'stop';
                }
                return;
            }

            // Calculate speed based on distance from center
            const dist = Math.sqrt(joystickX*joystickX + joystickY*joystickY);
            const speedFactor = Math.min(dist / 100, 1);
            const speed = Math.round(currentSpeed * speedFactor);

            // Determine direction
            let newCmd = '';
            const angle = Math.atan2(joystickY, joystickX) * 180 / Math.PI;

            if (angle > 60 && angle < 120) {
                newCmd = 'move:' + speed + ':' + speed;  // Forward
            } else if (angle < -60 && angle > -120) {
                newCmd = 'move:' + (-speed) + ':' + (-speed);  // Backward
            } else if (angle >= -60 && angle <= 60) {
                // Right or forward-right
                if (joystickY > deadZone) {
                    newCmd = 'move:' + speed + ':' + Math.round(speed * 0.3);  // Forward-right
                } else if (joystickY < -deadZone) {
                    newCmd = 'move:' + (-speed) + ':' + Math.round(-speed * 0.3);  // Backward-right
                } else {
                    newCmd = 'move:' + speed + ':' + (-speed);  // Spin right
                }
            } else {
                // Left or forward-left
                if (joystickY > deadZone) {
                    newCmd = 'move:' + Math.round(speed * 0.3) + ':' + speed;  // Forward-left
                } else if (joystickY < -deadZone) {
                    newCmd = 'move:' + Math.round(-speed * 0.3) + ':' + (-speed);  // Backward-left
                } else {
                    newCmd = 'move:' + (-speed) + ':' + speed;  // Spin left
                }
            }

            if (newCmd !== lastCommand) {
                cmdMove(newCmd);
                lastCommand = newCmd;
            }
        }

        function cmdMove(command) {
            if (socket && socket.connected) {
                socket.emit('command', {command: command});
            } else {
                fetch('/api/command', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({command: command})
                });
            }
        }

        // Raccourcis clavier
        let keysPressed = {};
        document.addEventListener('keydown', function(e) {
            if (e.repeat) return;
            keysPressed[e.key] = true;
            handleKeyboardMovement();
            if (e.key === ' ') e.preventDefault();
        });

        document.addEventListener('keyup', function(e) {
            delete keysPressed[e.key];
            handleKeyboardMovement();
        });

        function handleKeyboardMovement() {
            const up = keysPressed['ArrowUp'] || keysPressed['z'] || keysPressed['w'];
            const down = keysPressed['ArrowDown'] || keysPressed['s'];
            const left = keysPressed['ArrowLeft'] || keysPressed['q'] || keysPressed['a'];
            const right = keysPressed['ArrowRight'] || keysPressed['d'];
            const stop = keysPressed[' '];

            if (stop || (!up && !down && !left && !right)) {
                cmdMove('stop');
                return;
            }

            let leftSpeed = 0, rightSpeed = 0;
            const speed = currentSpeed;

            if (up) { leftSpeed += speed; rightSpeed += speed; }
            if (down) { leftSpeed -= speed; rightSpeed -= speed; }
            if (left) { leftSpeed -= speed * 0.5; rightSpeed += speed * 0.5; }
            if (right) { leftSpeed += speed * 0.5; rightSpeed -= speed * 0.5; }

            // Clamp values
            leftSpeed = Math.max(-255, Math.min(255, Math.round(leftSpeed)));
            rightSpeed = Math.max(-255, Math.min(255, Math.round(rightSpeed)));

            cmdMove('move:' + leftSpeed + ':' + rightSpeed);
        }

        // Demarrage
        log('Interface prete');
        connectWebSocket();
        initJoystick();
        initDetectionControls();
    </script>
</body>
</html>
"""

# ==================== CAMERA ====================

def init_camera():
    """Initialise la caméra (picamera2 ou OpenCV)"""
    global camera, camera_type

    # Essayer picamera2 d'abord (Raspberry Pi Camera)
    try:
        from picamera2 import Picamera2
        camera = Picamera2()
        camera.configure(camera.create_preview_configuration(
            main={"size": (640, 480), "format": "RGB888"}
        ))
        camera_type = 'picamera2'
        logger.info("Camera Pi detectee (picamera2)")
        return True
    except ImportError:
        logger.debug("picamera2 non disponible")
    except Exception as e:
        logger.warning(f"Erreur picamera2: {e}")

    # Essayer OpenCV (webcam USB)
    try:
        import cv2
        cam = cv2.VideoCapture(0)
        if cam.isOpened():
            cam.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
            cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
            camera = cam
            camera_type = 'opencv'
            logger.info("Camera USB detectee (OpenCV)")
            return True
        else:
            cam.release()
    except ImportError:
        logger.debug("OpenCV non disponible")
    except Exception as e:
        logger.warning(f"Erreur OpenCV: {e}")

    camera = None
    camera_type = None
    logger.info("Aucune camera disponible")
    return False

def init_person_detector():
    """Initialise le détecteur de personnes HOG"""
    global hog_detector
    try:
        import cv2
        hog_detector = cv2.HOGDescriptor()
        hog_detector.setSVMDetector(cv2.HOGDescriptor_getDefaultPeopleDetector())
        logger.info("Detecteur de personnes HOG initialise")
        return True
    except Exception as e:
        logger.warning(f"Erreur initialisation detecteur: {e}")
        hog_detector = None
        return False

def start_camera_stream():
    """Démarre le streaming caméra"""
    global camera_streaming
    with camera_lock:
        if camera is None:
            return False
        try:
            if camera_type == 'picamera2':
                camera.start()
            camera_streaming = True
            logger.info("Camera streaming demarre")
            return True
        except Exception as e:
            logger.error(f"Erreur demarrage camera: {e}")
            return False

def stop_camera_stream():
    """Arrête le streaming caméra"""
    global camera_streaming
    with camera_lock:
        try:
            if camera_type == 'picamera2' and camera:
                camera.stop()
        except:
            pass
        camera_streaming = False
        logger.info("Camera streaming arrete")

def rotate_frame(frame, rotation):
    """Applique une rotation à la frame"""
    import cv2
    if rotation == 90:
        return cv2.rotate(frame, cv2.ROTATE_90_CLOCKWISE)
    elif rotation == 180:
        return cv2.rotate(frame, cv2.ROTATE_180)
    elif rotation == 270:
        return cv2.rotate(frame, cv2.ROTATE_90_COUNTERCLOCKWISE)
    return frame

def get_camera_frame():
    """Capture une frame de la caméra"""
    global camera_rotation
    with camera_lock:
        if not camera_streaming or camera is None:
            return None
        try:
            import cv2
            if camera_type == 'picamera2':
                frame = camera.capture_array()
                frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
            elif camera_type == 'opencv':
                ret, frame = camera.read()
                if not ret:
                    return None
            else:
                return None

            # Appliquer la rotation
            if camera_rotation != 0:
                frame = rotate_frame(frame, camera_rotation)

            _, jpeg = cv2.imencode('.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, 70])
            return jpeg.tobytes()
        except Exception as e:
            logger.error(f"Erreur capture: {e}")
    return None

def generate_frames():
    """Générateur de frames pour le streaming MJPEG"""
    while camera_streaming:
        frame = get_camera_frame()
        if frame:
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')
        time.sleep(0.04)  # ~25 FPS

# ==================== PERSON DETECTION ====================

def detect_person_in_frame():
    """Détecte une personne dans la frame actuelle"""
    global person_detected
    if not camera_streaming or camera is None or hog_detector is None:
        logger.debug("Detection skip: camera=%s, streaming=%s, hog=%s",
                    camera is not None, camera_streaming, hog_detector is not None)
        return False

    try:
        import cv2

        # Capture frame pour analyse
        with camera_lock:
            if camera_type == 'picamera2':
                frame = camera.capture_array()
                frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
            elif camera_type == 'opencv':
                ret, frame = camera.read()
                if not ret:
                    logger.debug("Detection: echec capture frame")
                    return False
            else:
                return False

        # Appliquer rotation si nécessaire
        if camera_rotation != 0:
            frame = rotate_frame(frame, camera_rotation)

        # Réduire la taille pour accélérer la détection (garder assez grand)
        scale = 0.6
        small_frame = cv2.resize(frame, None, fx=scale, fy=scale)

        # Calculer le seuil basé sur la sensibilité (0.0 = très sensible, 1.0 = peu sensible)
        # hitThreshold: 0 = très sensible, 0.5 = moyen, 1.0+ = peu sensible
        hit_threshold = person_detection_sensitivity * 0.8

        # Détection HOG avec paramètres optimisés
        boxes, weights = hog_detector.detectMultiScale(
            small_frame,
            winStride=(4, 4),      # Plus petit = plus précis mais plus lent
            padding=(8, 8),        # Plus de padding = meilleure détection aux bords
            scale=1.02,            # Plus petit = plus de niveaux de détection
            hitThreshold=hit_threshold  # Basé sur la sensibilité configurée
        )

        # Log pour debug
        if len(boxes) > 0:
            logger.info(f"Detection HOG: {len(boxes)} zones, poids: {weights}")

        # Retourner vrai si au moins une détection
        detected = len(boxes) > 0
        return detected

    except Exception as e:
        logger.error(f"Erreur detection: {e}")
        return False

def person_detection_thread():
    """Thread de détection de personnes"""
    global person_detected, last_person_alert
    detection_count = 0

    logger.info("Thread de detection de personnes demarre")

    while True:
        if person_detection_enabled and camera_streaming:
            detection_count += 1
            if detection_count % 20 == 0:  # Log toutes les 10 secondes environ
                logger.debug(f"Detection active, scan #{detection_count}")

            detected = detect_person_in_frame()

            if detected:
                person_detected = True
                current_time = time.time()

                # Déclencher l'alarme avec cooldown
                if current_time - last_person_alert > person_detection_cooldown:
                    last_person_alert = current_time
                    logger.warning("!!! PERSONNE DETECTEE !!!")

                    # Envoyer alerte WebSocket
                    socketio.emit('alert', {
                        'type': 'person_detected',
                        'alert': 'person',
                        'message': 'Personne detectee par la camera!'
                    })

                    # Déclencher l'alarme sur le robot (si activée)
                    if person_alarm_enabled:
                        send_serial_command('alarm')
                        logger.info("Alarme sonore declenchee")

                    # Allumer LED rouge (si activée)
                    if person_led_enabled:
                        send_serial_command('led_red')
                        logger.info("LED rouge allumee")
            else:
                person_detected = False

            time.sleep(0.5)  # Vérifier 2 fois par seconde
        else:
            if person_detected:  # Reset si detection etait active
                person_detected = False
            detection_count = 0
            time.sleep(1)

def start_person_detection():
    """Active la détection de personnes"""
    global person_detection_enabled
    logger.info(f"Tentative activation detection: hog={hog_detector is not None}, cam={camera_streaming}")
    if hog_detector is None:
        init_person_detector()
    if hog_detector is None:
        logger.error("Impossible d'initialiser le detecteur HOG")
        return False
    if not camera_streaming:
        logger.warning("Camera non active - detection peut ne pas fonctionner")
    person_detection_enabled = True
    logger.info("*** Detection de personnes ACTIVEE ***")
    return True

def stop_person_detection():
    """Désactive la détection de personnes"""
    global person_detection_enabled, person_detected
    person_detection_enabled = False
    person_detected = False
    logger.info("Detection de personnes desactivee")

# ==================== SERIAL COMMUNICATION ====================

def connect_serial(port, baudrate=115200):
    """Connexion au port série"""
    global serial_port, robot_connected
    try:
        serial_port = serial.Serial(port, baudrate, timeout=1)
        robot_connected = True
        logger.info(f"Connecte au robot sur {port}")
        time.sleep(2)
        return True
    except Exception as e:
        logger.error(f"Erreur connexion serie: {e}")
        robot_connected = False
        return False

def disconnect_serial():
    """Déconnexion série"""
    global serial_port, robot_connected
    if serial_port and serial_port.is_open:
        try:
            serial_port.write(b"stop\n")
            serial_port.close()
        except:
            pass
    robot_connected = False

def send_serial_command(command):
    """Envoie une commande au robot"""
    global serial_port
    with serial_lock:
        if serial_port and serial_port.is_open:
            try:
                serial_port.write(f"{command}\n".encode())
                logger.info(f"Envoye: {command}")
                return True
            except Exception as e:
                logger.error(f"Erreur envoi: {e}")
    return False

def serial_reader_thread():
    """Thread de lecture série - envoie les données via WebSocket"""
    global last_status, monitoring
    while True:
        if robot_connected and serial_port and serial_port.is_open:
            with serial_lock:
                try:
                    while serial_port.in_waiting:
                        line = serial_port.readline().decode('utf-8').strip()
                        if line:
                            try:
                                data = json.loads(line)
                                msg_type = data.get("type", "")

                                if msg_type == "status":
                                    last_status = data
                                    monitoring = data.get("monitoring", False)
                                    # Envoyer via WebSocket
                                    socketio.emit('status', {
                                        'robot_connected': robot_connected,
                                        'camera_available': camera is not None,
                                        'camera_streaming': camera_streaming,
                                        'sensors': data
                                    })
                                elif msg_type == "alert":
                                    alert_history.insert(0, data)
                                    if len(alert_history) > 50:
                                        alert_history.pop()
                                    socketio.emit('alert', data)
                                elif msg_type in ["ack", "ready", "startup", "pong"]:
                                    socketio.emit('robot_message', data)

                                logger.debug(f"Recu: {data}")
                            except json.JSONDecodeError:
                                logger.debug(f"Message non-JSON: {line}")
                except Exception as e:
                    logger.error(f"Erreur lecture: {e}")
        time.sleep(0.05)

def status_broadcast_thread():
    """Thread qui envoie périodiquement le status"""
    while True:
        socketio.emit('status', {
            'robot_connected': robot_connected,
            'camera_available': camera is not None,
            'camera_streaming': camera_streaming,
            'detection_enabled': person_detection_enabled,
            'person_detected': person_detected,
            'sensors': last_status
        })
        time.sleep(2)

# ==================== ROUTES FLASK ====================

@app.route('/')
def index():
    """Page principale"""
    return render_template_string(HTML_PAGE)

@app.route('/api/status')
def api_status():
    """API: Status actuel"""
    return jsonify({
        "robot_connected": robot_connected,
        "monitoring": monitoring,
        "last_status": last_status,
        "camera_available": camera is not None,
        "camera_streaming": camera_streaming
    })

@app.route('/api/command', methods=['POST'])
def api_command():
    """API: Envoyer une commande (fallback HTTP)"""
    data = request.json or {}
    cmd = data.get('command', '')
    if cmd:
        success = send_serial_command(cmd)
        return jsonify({"success": success, "command": cmd})
    return jsonify({"success": False, "error": "No command"})

@app.route('/api/camera/start', methods=['POST'])
def api_camera_start():
    """API: Démarrer la caméra"""
    if camera is None:
        return jsonify({"success": False, "error": "Camera non disponible"})
    success = start_camera_stream()
    return jsonify({"success": success})

@app.route('/api/camera/stop', methods=['POST'])
def api_camera_stop():
    """API: Arrêter la caméra"""
    stop_camera_stream()
    return jsonify({"success": True})

@app.route('/api/camera/rotate', methods=['POST'])
def api_camera_rotate():
    """API: Changer la rotation de la caméra"""
    global camera_rotation
    data = request.json or {}
    rotation = data.get('rotation')
    if rotation is not None and rotation in [0, 90, 180, 270]:
        camera_rotation = rotation
        logger.info(f"Camera rotation: {rotation}°")
        return jsonify({"success": True, "rotation": camera_rotation})
    return jsonify({"success": False, "error": "Invalid rotation (0, 90, 180, 270)"})

@app.route('/api/camera/rotation', methods=['GET'])
def api_camera_rotation():
    """API: Obtenir la rotation actuelle"""
    return jsonify({"rotation": camera_rotation})

@app.route('/api/detection/start', methods=['POST'])
def api_detection_start():
    """API: Activer la détection de personnes"""
    if not camera_streaming:
        return jsonify({"success": False, "error": "Camera doit etre active"})
    success = start_person_detection()
    return jsonify({"success": success})

@app.route('/api/detection/stop', methods=['POST'])
def api_detection_stop():
    """API: Désactiver la détection de personnes"""
    stop_person_detection()
    return jsonify({"success": True})

@app.route('/api/detection/status', methods=['GET'])
def api_detection_status():
    """API: Obtenir le status de la détection"""
    return jsonify({
        "enabled": person_detection_enabled,
        "detected": person_detected,
        "detector_ready": hog_detector is not None
    })

@app.route('/api/detection/settings', methods=['GET', 'POST'])
def api_detection_settings():
    """API: Obtenir ou modifier les réglages de détection"""
    global person_detection_sensitivity, person_detection_cooldown
    global person_alarm_enabled, person_led_enabled

    if request.method == 'POST':
        data = request.json or {}
        if 'sensitivity' in data:
            # Convertir 0-100 en 0.0-1.0 (inversé: 100% = très sensible = seuil bas)
            person_detection_sensitivity = 1.0 - (data['sensitivity'] / 100.0)
            logger.info(f"Sensibilite detection: {data['sensitivity']}% (threshold={person_detection_sensitivity:.2f})")
        if 'cooldown' in data:
            person_detection_cooldown = max(1, min(30, data['cooldown']))
            logger.info(f"Cooldown detection: {person_detection_cooldown}s")
        if 'alarm_enabled' in data:
            person_alarm_enabled = bool(data['alarm_enabled'])
            logger.info(f"Alarme detection: {'ON' if person_alarm_enabled else 'OFF'}")
        if 'led_enabled' in data:
            person_led_enabled = bool(data['led_enabled'])
            logger.info(f"LED detection: {'ON' if person_led_enabled else 'OFF'}")
        return jsonify({"success": True})

    # GET - retourner les réglages actuels
    return jsonify({
        "sensitivity": int((1.0 - person_detection_sensitivity) * 100),
        "cooldown": person_detection_cooldown,
        "alarm_enabled": person_alarm_enabled,
        "led_enabled": person_led_enabled
    })

@app.route('/video_feed')
def video_feed():
    """Streaming vidéo MJPEG"""
    if not camera_streaming:
        return "Camera not streaming", 503
    return Response(generate_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/snapshot')
def snapshot():
    """Capture une image"""
    if not camera_streaming:
        start_camera_stream()
        time.sleep(0.3)
    frame = get_camera_frame()
    if frame:
        return Response(frame, mimetype='image/jpeg',
                       headers={'Content-Disposition': 'inline; filename=snapshot.jpg'})
    return "Camera not available", 503

# ==================== WEBSOCKET HANDLERS ====================

@socketio.on('connect')
def handle_connect():
    """Client WebSocket connecté"""
    logger.info("Client WebSocket connecte")
    emit('status', {
        'robot_connected': robot_connected,
        'camera_available': camera is not None,
        'camera_streaming': camera_streaming,
        'detection_enabled': person_detection_enabled,
        'person_detected': person_detected,
        'sensors': last_status
    })

@socketio.on('disconnect')
def handle_disconnect():
    """Client WebSocket déconnecté"""
    logger.info("Client WebSocket deconnecte")

@socketio.on('command')
def handle_command(data):
    """Réception d'une commande via WebSocket"""
    cmd = data.get('command', '')
    if cmd:
        success = send_serial_command(cmd)
        emit('cmd_result', {'success': success, 'command': cmd})

@socketio.on('get_status')
def handle_get_status():
    """Demande de status"""
    emit('status', {
        'robot_connected': robot_connected,
        'camera_available': camera is not None,
        'camera_streaming': camera_streaming,
        'detection_enabled': person_detection_enabled,
        'person_detected': person_detected,
        'sensors': last_status
    })

# ==================== MAIN ====================

def main():
    parser = argparse.ArgumentParser(description='mBot Ranger Web Controller')
    parser.add_argument('--port', '-p', default='/dev/ttyUSB0',
                       help='Port serie (defaut: /dev/ttyUSB0)')
    parser.add_argument('--web-port', '-w', type=int, default=5000,
                       help='Port web (defaut: 5000)')
    parser.add_argument('--host', '-H', default='0.0.0.0',
                       help='Adresse ecoute (defaut: 0.0.0.0)')
    parser.add_argument('--no-camera', action='store_true',
                       help='Desactiver la camera')
    parser.add_argument('--camera-rotation', '-r', type=int, default=270,
                       choices=[0, 90, 180, 270],
                       help='Rotation camera en degres (defaut: 270 = gauche)')
    parser.add_argument('--debug', '-d', action='store_true',
                       help='Mode debug')

    args = parser.parse_args()

    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)

    # Configuration camera rotation
    global camera_rotation
    camera_rotation = args.camera_rotation

    # Connexion série
    if not connect_serial(args.port):
        logger.warning(f"Robot non connecte sur {args.port}")

    # Initialisation caméra
    if not args.no_camera:
        init_camera()
        # Initialiser le détecteur de personnes
        init_person_detector()

    # Thread de lecture série
    serial_thread = threading.Thread(target=serial_reader_thread, daemon=True)
    serial_thread.start()

    # Thread de broadcast status
    broadcast_thread = threading.Thread(target=status_broadcast_thread, daemon=True)
    broadcast_thread.start()

    # Thread de détection de personnes
    detection_thread = threading.Thread(target=person_detection_thread, daemon=True)
    detection_thread.start()

    # Démarrer le serveur avec SocketIO
    logger.info(f"Serveur web: http://{args.host}:{args.web_port}")
    if camera:
        logger.info(f"Camera: {camera_type}, rotation: {camera_rotation}°")
    if hog_detector:
        logger.info("Detection de personnes: pret")

    try:
        socketio.run(app, host=args.host, port=args.web_port,
                    debug=args.debug, use_reloader=False, allow_unsafe_werkzeug=True)
    except KeyboardInterrupt:
        pass
    finally:
        stop_person_detection()
        stop_camera_stream()
        disconnect_serial()

if __name__ == "__main__":
    main()
