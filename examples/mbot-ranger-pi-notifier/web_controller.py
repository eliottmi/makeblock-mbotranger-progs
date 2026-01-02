#!/usr/bin/env python3
"""
mBot Ranger Web Controller - Interface Web pour Raspberry Pi

Ce script lance un serveur web permettant de contrôler le mBot Ranger
depuis n'importe quel navigateur sur le réseau local.
Inclut le streaming vidéo de la caméra Pi.

Prérequis:
    pip3 install flask flask-socketio pyserial

    Pour la caméra (optionnel):
    pip3 install picamera2  # Pour Pi Camera (recommandé)
    # ou
    pip3 install opencv-python  # Pour webcam USB

Usage:
    python3 web_controller.py [--port 5000] [--serial /dev/ttyUSB0] [--camera]

Accès:
    http://raspberry-pi-ip:5000
"""

import serial
import json
import time
import argparse
import logging
import threading
import io
from datetime import datetime
from flask import Flask, render_template_string, jsonify, request, Response
from flask_socketio import SocketIO, emit

# Configuration
app = Flask(__name__)
app.config['SECRET_KEY'] = 'mbot-ranger-secret'
socketio = SocketIO(app, cors_allowed_origins="*")

# Variables globales
serial_port = None
serial_lock = threading.Lock()
connected = False
monitoring = False
last_status = {}
alert_history = []

# Camera
camera = None
camera_enabled = False
camera_lock = threading.Lock()
camera_type = None  # 'picamera2', 'opencv', or None

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
    <script src="https://cdnjs.cloudflare.com/ajax/libs/socket.io/4.0.1/socket.io.js"></script>
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            min-height: 100vh;
            color: #fff;
            padding: 20px;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        h1 {
            text-align: center;
            margin-bottom: 20px;
            font-size: 2em;
            text-shadow: 0 2px 4px rgba(0,0,0,0.3);
        }
        .main-layout {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
        }
        @media (max-width: 900px) {
            .main-layout {
                grid-template-columns: 1fr;
            }
        }
        .left-column, .right-column {
            display: flex;
            flex-direction: column;
            gap: 20px;
        }
        .status-bar {
            background: rgba(255,255,255,0.1);
            border-radius: 10px;
            padding: 15px;
            margin-bottom: 20px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            flex-wrap: wrap;
            gap: 10px;
        }
        .status-item {
            display: flex;
            align-items: center;
            gap: 8px;
        }
        .status-dot {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            background: #ff4444;
        }
        .status-dot.connected {
            background: #44ff44;
            box-shadow: 0 0 10px #44ff44;
        }
        .status-dot.monitoring {
            background: #4444ff;
            box-shadow: 0 0 10px #4444ff;
            animation: pulse 1.5s infinite;
        }
        .status-dot.camera-on {
            background: #ff44ff;
            box-shadow: 0 0 10px #ff44ff;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        .panel {
            background: rgba(255,255,255,0.1);
            border-radius: 15px;
            padding: 20px;
        }
        .panel h2 {
            margin-bottom: 15px;
            font-size: 1.2em;
            color: #88ccff;
        }
        .camera-container {
            position: relative;
            background: #000;
            border-radius: 10px;
            overflow: hidden;
            aspect-ratio: 4/3;
        }
        .camera-feed {
            width: 100%;
            height: 100%;
            object-fit: contain;
        }
        .camera-overlay {
            position: absolute;
            top: 10px;
            left: 10px;
            background: rgba(0,0,0,0.6);
            padding: 5px 10px;
            border-radius: 5px;
            font-size: 0.8em;
        }
        .camera-overlay.live {
            background: rgba(255,0,0,0.7);
        }
        .camera-placeholder {
            width: 100%;
            height: 100%;
            display: flex;
            align-items: center;
            justify-content: center;
            color: #666;
            font-size: 1.2em;
        }
        .camera-controls {
            display: flex;
            gap: 10px;
            margin-top: 10px;
            justify-content: center;
        }
        .controls-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
            max-width: 300px;
            margin: 0 auto;
        }
        .btn {
            background: linear-gradient(145deg, #3a3a5c, #2a2a4c);
            border: none;
            border-radius: 10px;
            padding: 20px;
            color: #fff;
            font-size: 1.5em;
            cursor: pointer;
            transition: all 0.2s;
            box-shadow: 0 4px 6px rgba(0,0,0,0.3);
        }
        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 12px rgba(0,0,0,0.4);
        }
        .btn:active {
            transform: translateY(0);
            box-shadow: 0 2px 4px rgba(0,0,0,0.3);
        }
        .btn.stop {
            background: linear-gradient(145deg, #cc4444, #aa2222);
            grid-column: 2;
        }
        .btn-row {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
            justify-content: center;
        }
        .btn-small {
            background: linear-gradient(145deg, #3a5c3a, #2a4c2a);
            border: none;
            border-radius: 8px;
            padding: 12px 20px;
            color: #fff;
            font-size: 0.9em;
            cursor: pointer;
            transition: all 0.2s;
        }
        .btn-small:hover {
            transform: translateY(-2px);
        }
        .btn-small.red { background: linear-gradient(145deg, #cc4444, #aa2222); }
        .btn-small.green { background: linear-gradient(145deg, #44cc44, #22aa22); }
        .btn-small.blue { background: linear-gradient(145deg, #4444cc, #2222aa); }
        .btn-small.yellow { background: linear-gradient(145deg, #cccc44, #aaaa22); }
        .btn-small.purple { background: linear-gradient(145deg, #cc44cc, #aa22aa); }
        .sensor-display {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
        }
        .sensor-card {
            background: rgba(0,0,0,0.2);
            border-radius: 10px;
            padding: 12px;
            text-align: center;
        }
        .sensor-value {
            font-size: 1.5em;
            font-weight: bold;
            color: #88ff88;
        }
        .sensor-label {
            font-size: 0.75em;
            color: #aaa;
            margin-top: 3px;
        }
        .alerts-log {
            max-height: 150px;
            overflow-y: auto;
            background: rgba(0,0,0,0.2);
            border-radius: 10px;
            padding: 10px;
        }
        .alert-item {
            padding: 6px;
            border-bottom: 1px solid rgba(255,255,255,0.1);
            font-size: 0.85em;
        }
        .alert-item:last-child {
            border-bottom: none;
        }
        .alert-item.obstacle { border-left: 3px solid #ff4444; padding-left: 10px; }
        .alert-item.motion { border-left: 3px solid #ffaa44; padding-left: 10px; }
        .alert-item.button { border-left: 3px solid #44aaff; padding-left: 10px; }
        .alert-time {
            color: #888;
            font-size: 0.8em;
        }
        .toggle-container {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
            justify-content: center;
        }
        .toggle-btn {
            padding: 8px 15px;
            border-radius: 20px;
            border: 2px solid #555;
            background: transparent;
            color: #888;
            cursor: pointer;
            transition: all 0.3s;
            font-size: 0.85em;
        }
        .toggle-btn.active {
            border-color: #44ff44;
            color: #44ff44;
            background: rgba(68, 255, 68, 0.1);
        }
        .snapshot-btn {
            background: linear-gradient(145deg, #5c3a5c, #4c2a4c);
        }
        @media (max-width: 500px) {
            .controls-grid {
                max-width: 220px;
            }
            .btn {
                padding: 15px;
                font-size: 1.2em;
            }
            .sensor-display {
                grid-template-columns: repeat(3, 1fr);
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>mBot Ranger Controller</h1>

        <div class="status-bar">
            <div class="status-item">
                <div class="status-dot" id="connectionDot"></div>
                <span id="connectionStatus">Deconnecte</span>
            </div>
            <div class="status-item">
                <div class="status-dot" id="monitoringDot"></div>
                <span id="monitoringStatus">Monitoring OFF</span>
            </div>
            <div class="status-item">
                <div class="status-dot" id="cameraDot"></div>
                <span id="cameraStatus">Camera OFF</span>
            </div>
            <div class="status-item">
                <span id="uptime">Uptime: --</span>
            </div>
        </div>

        <div class="main-layout">
            <div class="left-column">
                <div class="panel">
                    <h2>Camera Pi</h2>
                    <div class="camera-container" id="cameraContainer">
                        <div class="camera-placeholder" id="cameraPlaceholder">
                            Camera non disponible
                        </div>
                        <img class="camera-feed" id="cameraFeed" style="display: none;" alt="Camera Feed">
                        <div class="camera-overlay" id="cameraOverlay" style="display: none;">LIVE</div>
                    </div>
                    <div class="camera-controls">
                        <button class="btn-small green" onclick="startCamera()">Demarrer</button>
                        <button class="btn-small red" onclick="stopCamera()">Arreter</button>
                        <button class="btn-small snapshot-btn" onclick="takeSnapshot()">Snapshot</button>
                    </div>
                </div>

                <div class="panel">
                    <h2>Controles de mouvement</h2>
                    <div class="controls-grid">
                        <div></div>
                        <button class="btn" onclick="sendCommand('forward')" title="Avancer">^</button>
                        <div></div>
                        <button class="btn" onclick="sendCommand('left')" title="Gauche">&lt;</button>
                        <button class="btn stop" onclick="sendCommand('stop')" title="Stop">X</button>
                        <button class="btn" onclick="sendCommand('right')" title="Droite">&gt;</button>
                        <div></div>
                        <button class="btn" onclick="sendCommand('backward')" title="Reculer">v</button>
                        <div></div>
                    </div>
                </div>
            </div>

            <div class="right-column">
                <div class="panel">
                    <h2>LEDs & Sons</h2>
                    <div class="btn-row">
                        <button class="btn-small red" onclick="sendCommand('led_red')">Rouge</button>
                        <button class="btn-small green" onclick="sendCommand('led_green')">Vert</button>
                        <button class="btn-small blue" onclick="sendCommand('led_blue')">Bleu</button>
                        <button class="btn-small" onclick="sendCommand('led_off')">Off</button>
                        <button class="btn-small yellow" onclick="sendCommand('beep')">Beep</button>
                        <button class="btn-small purple" onclick="sendCommand('alarm')">Alarme</button>
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
                    <div class="btn-row" style="margin-bottom: 10px;">
                        <button class="btn-small green" onclick="sendCommand('start')">Demarrer</button>
                        <button class="btn-small red" onclick="sendCommand('stop')">Arreter</button>
                        <button class="btn-small yellow" onclick="sendCommand('calibrate')">Calibrer</button>
                        <button class="btn-small blue" onclick="sendCommand('status')">Status</button>
                    </div>
                    <div class="toggle-container">
                        <button class="toggle-btn active" id="toggleObstacle" onclick="toggleAlert('obstacle')">
                            Obstacles
                        </button>
                        <button class="toggle-btn active" id="toggleMotion" onclick="toggleAlert('motion')">
                            Mouvement
                        </button>
                    </div>
                </div>

                <div class="panel">
                    <h2>Historique des alertes</h2>
                    <div class="alerts-log" id="alertsLog">
                        <div class="alert-item" style="color: #888;">En attente d'alertes...</div>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script>
        const socket = io();
        let obstacleEnabled = true;
        let motionEnabled = true;
        let cameraActive = false;

        // Connexion WebSocket
        socket.on('connect', function() {
            console.log('WebSocket connecte');
            document.getElementById('connectionDot').classList.add('connected');
            document.getElementById('connectionStatus').textContent = 'Connecte';
            setTimeout(() => sendCommand('status'), 500);
            checkCameraStatus();
        });

        socket.on('disconnect', function() {
            console.log('WebSocket deconnecte');
            document.getElementById('connectionDot').classList.remove('connected');
            document.getElementById('connectionStatus').textContent = 'Deconnecte';
        });

        // Reception des mises a jour de status
        socket.on('status', function(data) {
            if (data.distance !== undefined) {
                document.getElementById('distanceValue').textContent = data.distance.toFixed(1);
            }
            if (data.light !== undefined) {
                document.getElementById('lightValue').textContent = data.light;
            }
            if (data.baseline !== undefined) {
                document.getElementById('baselineValue').textContent = data.baseline.toFixed(1);
            }
            if (data.uptime !== undefined) {
                document.getElementById('uptime').textContent = 'Uptime: ' + formatUptime(data.uptime);
            }
            if (data.monitoring !== undefined) {
                const dot = document.getElementById('monitoringDot');
                const status = document.getElementById('monitoringStatus');
                if (data.monitoring) {
                    dot.classList.add('monitoring');
                    status.textContent = 'Monitoring ON';
                } else {
                    dot.classList.remove('monitoring');
                    status.textContent = 'Monitoring OFF';
                }
            }
        });

        // Reception des alertes
        socket.on('alert', function(data) {
            addAlertToLog(data);
        });

        socket.on('message', function(data) {
            console.log('Message:', data);
        });

        function sendCommand(cmd) {
            socket.emit('command', {command: cmd});
        }

        function toggleAlert(type) {
            const btn = document.getElementById('toggle' + type.charAt(0).toUpperCase() + type.slice(1));
            if (type === 'obstacle') {
                obstacleEnabled = !obstacleEnabled;
                sendCommand(obstacleEnabled ? 'obstacle_on' : 'obstacle_off');
                btn.classList.toggle('active', obstacleEnabled);
            } else if (type === 'motion') {
                motionEnabled = !motionEnabled;
                sendCommand(motionEnabled ? 'motion_on' : 'motion_off');
                btn.classList.toggle('active', motionEnabled);
            }
        }

        function addAlertToLog(data) {
            const log = document.getElementById('alertsLog');
            const alertType = data.alert || 'info';
            const message = data.message || 'Alerte';
            const value = data.value !== undefined ? ` (${data.value})` : '';
            const time = new Date().toLocaleTimeString();

            const item = document.createElement('div');
            item.className = 'alert-item ' + alertType;
            item.innerHTML = `<span class="alert-time">${time}</span> - <strong>${alertType.toUpperCase()}</strong>: ${message}${value}`;

            if (log.children.length === 1 && log.children[0].style.color === 'rgb(136, 136, 136)') {
                log.innerHTML = '';
            }

            log.insertBefore(item, log.firstChild);

            while (log.children.length > 50) {
                log.removeChild(log.lastChild);
            }
        }

        function formatUptime(seconds) {
            const h = Math.floor(seconds / 3600);
            const m = Math.floor((seconds % 3600) / 60);
            const s = seconds % 60;
            if (h > 0) return `${h}h ${m}m ${s}s`;
            if (m > 0) return `${m}m ${s}s`;
            return `${s}s`;
        }

        // Camera functions
        function checkCameraStatus() {
            fetch('/api/camera/status')
                .then(r => r.json())
                .then(data => {
                    updateCameraUI(data.available, data.streaming);
                });
        }

        function updateCameraUI(available, streaming) {
            const dot = document.getElementById('cameraDot');
            const status = document.getElementById('cameraStatus');
            const feed = document.getElementById('cameraFeed');
            const placeholder = document.getElementById('cameraPlaceholder');
            const overlay = document.getElementById('cameraOverlay');

            if (!available) {
                status.textContent = 'Camera N/A';
                placeholder.textContent = 'Camera non disponible';
                placeholder.style.display = 'flex';
                feed.style.display = 'none';
                overlay.style.display = 'none';
            } else if (streaming) {
                dot.classList.add('camera-on');
                status.textContent = 'Camera ON';
                placeholder.style.display = 'none';
                feed.style.display = 'block';
                feed.src = '/video_feed?' + Date.now();
                overlay.style.display = 'block';
                overlay.classList.add('live');
                cameraActive = true;
            } else {
                dot.classList.remove('camera-on');
                status.textContent = 'Camera OFF';
                placeholder.textContent = 'Camera arretee';
                placeholder.style.display = 'flex';
                feed.style.display = 'none';
                overlay.style.display = 'none';
                cameraActive = false;
            }
        }

        function startCamera() {
            fetch('/api/camera/start', {method: 'POST'})
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        updateCameraUI(true, true);
                    } else {
                        alert('Erreur: ' + (data.error || 'Camera non disponible'));
                    }
                });
        }

        function stopCamera() {
            fetch('/api/camera/stop', {method: 'POST'})
                .then(r => r.json())
                .then(data => {
                    updateCameraUI(data.available, false);
                });
        }

        function takeSnapshot() {
            if (!cameraActive) {
                alert('Demarrez la camera d\'abord');
                return;
            }
            window.open('/snapshot?' + Date.now(), '_blank');
        }

        // Raccourcis clavier
        document.addEventListener('keydown', function(e) {
            switch(e.key) {
                case 'ArrowUp': case 'z': case 'w': sendCommand('forward'); break;
                case 'ArrowDown': case 's': sendCommand('backward'); break;
                case 'ArrowLeft': case 'q': case 'a': sendCommand('left'); break;
                case 'ArrowRight': case 'd': sendCommand('right'); break;
                case ' ': sendCommand('stop'); e.preventDefault(); break;
            }
        });
    </script>
</body>
</html>
"""

# ==================== CAMERA ====================

def init_camera():
    """Initialise la caméra (picamera2 ou OpenCV)"""
    global camera, camera_enabled, camera_type

    # Essayer picamera2 d'abord (Raspberry Pi Camera)
    try:
        from picamera2 import Picamera2
        camera = Picamera2()
        camera.configure(camera.create_preview_configuration(
            main={"size": (640, 480), "format": "RGB888"}
        ))
        camera_type = 'picamera2'
        camera_enabled = True
        logger.info("Camera Pi initialisee (picamera2)")
        return True
    except ImportError:
        logger.debug("picamera2 non disponible")
    except Exception as e:
        logger.warning(f"Erreur picamera2: {e}")

    # Essayer OpenCV (webcam USB)
    try:
        import cv2
        camera = cv2.VideoCapture(0)
        if camera.isOpened():
            camera.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
            camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
            camera_type = 'opencv'
            camera_enabled = True
            logger.info("Camera USB initialisee (OpenCV)")
            return True
        else:
            camera.release()
            camera = None
    except ImportError:
        logger.debug("OpenCV non disponible")
    except Exception as e:
        logger.warning(f"Erreur OpenCV: {e}")

    camera_enabled = False
    camera_type = None
    logger.info("Aucune camera disponible")
    return False

def start_camera_stream():
    """Démarre le streaming caméra"""
    global camera, camera_enabled
    with camera_lock:
        if camera_type == 'picamera2' and camera:
            try:
                camera.start()
                camera_enabled = True
                return True
            except Exception as e:
                logger.error(f"Erreur démarrage picamera2: {e}")
        elif camera_type == 'opencv' and camera:
            camera_enabled = True
            return True
    return False

def stop_camera_stream():
    """Arrête le streaming caméra"""
    global camera_enabled
    with camera_lock:
        if camera_type == 'picamera2' and camera:
            try:
                camera.stop()
            except:
                pass
        camera_enabled = False

def get_camera_frame():
    """Capture une frame de la caméra"""
    global camera
    with camera_lock:
        if not camera_enabled or not camera:
            return None

        try:
            if camera_type == 'picamera2':
                frame = camera.capture_array()
                # Convertir RGB en JPEG
                import cv2
                _, jpeg = cv2.imencode('.jpg', cv2.cvtColor(frame, cv2.COLOR_RGB2BGR),
                                       [cv2.IMWRITE_JPEG_QUALITY, 80])
                return jpeg.tobytes()
            elif camera_type == 'opencv':
                import cv2
                ret, frame = camera.read()
                if ret:
                    _, jpeg = cv2.imencode('.jpg', frame,
                                          [cv2.IMWRITE_JPEG_QUALITY, 80])
                    return jpeg.tobytes()
        except Exception as e:
            logger.error(f"Erreur capture: {e}")
    return None

def generate_frames():
    """Générateur de frames pour le streaming MJPEG"""
    while camera_enabled:
        frame = get_camera_frame()
        if frame:
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')
        time.sleep(0.033)  # ~30 FPS

# ==================== SERIAL COMMUNICATION ====================

def connect_serial(port, baudrate=115200):
    """Connexion au port série"""
    global serial_port, connected
    try:
        serial_port = serial.Serial(port, baudrate, timeout=1)
        connected = True
        logger.info(f"Connecté à {port}")
        time.sleep(2)
        return True
    except Exception as e:
        logger.error(f"Erreur connexion série: {e}")
        connected = False
        return False

def disconnect_serial():
    """Déconnexion série"""
    global serial_port, connected
    if serial_port and serial_port.is_open:
        send_serial_command("stop")
        serial_port.close()
    connected = False
    logger.info("Déconnecté du port série")

def send_serial_command(command):
    """Envoie une commande au robot"""
    global serial_port
    with serial_lock:
        if serial_port and serial_port.is_open:
            try:
                serial_port.write(f"{command}\n".encode())
                logger.debug(f"Envoyé: {command}")
                return True
            except Exception as e:
                logger.error(f"Erreur envoi: {e}")
    return False

def read_serial_message():
    """Lit un message du robot"""
    global serial_port
    with serial_lock:
        if serial_port and serial_port.is_open:
            try:
                if serial_port.in_waiting:
                    line = serial_port.readline().decode('utf-8').strip()
                    if line:
                        return json.loads(line)
            except json.JSONDecodeError:
                pass
            except Exception as e:
                logger.error(f"Erreur lecture: {e}")
    return None

def serial_reader_thread():
    """Thread de lecture série"""
    global last_status, monitoring
    while True:
        if connected:
            data = read_serial_message()
            if data:
                msg_type = data.get("type", "")

                if msg_type == "status":
                    last_status = data
                    monitoring = data.get("monitoring", False)
                    socketio.emit('status', data)

                elif msg_type == "alert":
                    alert_history.insert(0, {
                        "time": datetime.now().isoformat(),
                        **data
                    })
                    if len(alert_history) > 100:
                        alert_history.pop()
                    socketio.emit('alert', data)

                elif msg_type == "heartbeat":
                    socketio.emit('status', {"uptime": data.get("uptime", 0)})

                elif msg_type in ["ack", "monitoring", "calibration", "ready", "startup"]:
                    socketio.emit('message', data)
                    if msg_type == "monitoring":
                        monitoring = "started" in data.get("message", "").lower()
                        socketio.emit('status', {"monitoring": monitoring})

        time.sleep(0.05)

# ==================== ROUTES FLASK ====================

@app.route('/')
def index():
    """Page principale"""
    return render_template_string(HTML_PAGE)

@app.route('/api/status')
def api_status():
    """API: Status actuel"""
    return jsonify({
        "connected": connected,
        "monitoring": monitoring,
        "last_status": last_status,
        "camera_available": camera is not None,
        "camera_streaming": camera_enabled
    })

@app.route('/api/alerts')
def api_alerts():
    """API: Historique des alertes"""
    return jsonify(alert_history[:50])

@app.route('/api/command', methods=['POST'])
def api_command():
    """API: Envoyer une commande"""
    data = request.json
    cmd = data.get('command', '')
    if cmd:
        success = send_serial_command(cmd)
        return jsonify({"success": success, "command": cmd})
    return jsonify({"success": False, "error": "No command"})

# Camera API
@app.route('/api/camera/status')
def api_camera_status():
    """API: Status caméra"""
    return jsonify({
        "available": camera is not None,
        "streaming": camera_enabled,
        "type": camera_type
    })

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
    return jsonify({"success": True, "available": camera is not None})

@app.route('/video_feed')
def video_feed():
    """Streaming vidéo MJPEG"""
    if not camera_enabled:
        return "Camera not streaming", 503
    return Response(generate_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/snapshot')
def snapshot():
    """Capture une image"""
    frame = get_camera_frame()
    if frame:
        return Response(frame, mimetype='image/jpeg')
    return "Camera not available", 503

# ==================== WEBSOCKET HANDLERS ====================

@socketio.on('connect')
def handle_connect():
    """Client WebSocket connecté"""
    logger.info("Client WebSocket connecté")
    emit('status', {
        "connected": connected,
        "monitoring": monitoring,
        **last_status
    })

@socketio.on('command')
def handle_command(data):
    """Réception d'une commande WebSocket"""
    cmd = data.get('command', '')
    if cmd:
        send_serial_command(cmd)
        logger.info(f"Commande reçue: {cmd}")

# ==================== MAIN ====================

def main():
    parser = argparse.ArgumentParser(description='mBot Ranger Web Controller')
    parser.add_argument('--port', '-p', default='/dev/ttyUSB0',
                       help='Port série (défaut: /dev/ttyUSB0)')
    parser.add_argument('--web-port', '-w', type=int, default=5000,
                       help='Port web (défaut: 5000)')
    parser.add_argument('--host', '-H', default='0.0.0.0',
                       help='Adresse d\'écoute (défaut: 0.0.0.0)')
    parser.add_argument('--camera', '-c', action='store_true',
                       help='Activer la caméra')
    parser.add_argument('--no-camera', action='store_true',
                       help='Désactiver la caméra')
    parser.add_argument('--debug', '-d', action='store_true',
                       help='Mode debug')

    args = parser.parse_args()

    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)

    # Connexion série
    if not connect_serial(args.port):
        logger.warning("Démarrage sans connexion série (mode test)")

    # Initialisation caméra
    if not args.no_camera:
        init_camera()

    # Thread de lecture série
    reader_thread = threading.Thread(target=serial_reader_thread, daemon=True)
    reader_thread.start()

    # Démarrer le serveur
    logger.info(f"Serveur web sur http://{args.host}:{args.web_port}")
    logger.info("Ouvrez cette adresse dans votre navigateur")
    if camera:
        logger.info(f"Camera disponible ({camera_type})")

    try:
        socketio.run(app, host=args.host, port=args.web_port,
                    debug=args.debug, allow_unsafe_werkzeug=True)
    except KeyboardInterrupt:
        pass
    finally:
        stop_camera_stream()
        disconnect_serial()

if __name__ == "__main__":
    main()
