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

# Configuration
app = Flask(__name__)
app.config['SECRET_KEY'] = 'mbot-ranger-secret'

# Essayer d'importer SocketIO avec le bon backend
try:
    from flask_socketio import SocketIO, emit
    socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')
    SOCKETIO_AVAILABLE = True
except ImportError:
    SOCKETIO_AVAILABLE = False
    socketio = None

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
        }
        .status-dot.on { background: #44ff44; box-shadow: 0 0 10px #44ff44; }
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
            cursor: pointer; transition: all 0.2s;
        }
        .btn:hover { transform: translateY(-2px); }
        .btn:active { transform: translateY(0); }
        .btn.stop { background: linear-gradient(145deg, #cc4444, #aa2222); }
        .btn-row { display: flex; gap: 8px; flex-wrap: wrap; justify-content: center; }
        .btn-small {
            background: linear-gradient(145deg, #3a5c3a, #2a4c2a);
            border: none; border-radius: 8px;
            padding: 10px 16px; color: #fff; font-size: 0.85em;
            cursor: pointer;
        }
        .btn-small.red { background: linear-gradient(145deg, #cc4444, #aa2222); }
        .btn-small.green { background: linear-gradient(145deg, #44cc44, #22aa22); }
        .btn-small.blue { background: linear-gradient(145deg, #4444cc, #2222aa); }
        .btn-small.yellow { background: linear-gradient(145deg, #cccc44, #aaaa22); }
        .btn-small.purple { background: linear-gradient(145deg, #cc44cc, #aa22aa); }
        .sensor-display { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; }
        .sensor-card {
            background: rgba(0,0,0,0.2);
            border-radius: 10px; padding: 12px; text-align: center;
        }
        .sensor-value { font-size: 1.4em; font-weight: bold; color: #88ff88; }
        .sensor-label { font-size: 0.7em; color: #aaa; margin-top: 3px; }
        .alerts-log {
            max-height: 120px; overflow-y: auto;
            background: rgba(0,0,0,0.2);
            border-radius: 10px; padding: 10px;
            font-size: 0.8em;
        }
        .alert-item { padding: 5px; border-bottom: 1px solid rgba(255,255,255,0.1); }
        .alert-item.obstacle { border-left: 3px solid #ff4444; padding-left: 8px; }
        .alert-item.motion { border-left: 3px solid #ffaa44; padding-left: 8px; }
        .log-area {
            background: rgba(0,0,0,0.3);
            border-radius: 8px; padding: 10px;
            max-height: 100px; overflow-y: auto;
            font-family: monospace; font-size: 0.75em;
            color: #aaa;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>mBot Ranger Controller</h1>

        <div class="status-bar">
            <div class="status-item">
                <div class="status-dot" id="robotDot"></div>
                <span id="robotStatus">Robot: --</span>
            </div>
            <div class="status-item">
                <div class="status-dot" id="cameraDot"></div>
                <span id="cameraStatus">Camera: --</span>
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
                </div>

                <div class="panel">
                    <h2>Controles</h2>
                    <div class="controls-grid">
                        <div></div>
                        <button class="btn" onclick="cmd('forward')">^</button>
                        <div></div>
                        <button class="btn" onclick="cmd('left')">&lt;</button>
                        <button class="btn stop" onclick="cmd('stop')">X</button>
                        <button class="btn" onclick="cmd('right')">&gt;</button>
                        <div></div>
                        <button class="btn" onclick="cmd('backward')">v</button>
                        <div></div>
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
                    <div class="log-area" id="logArea">En attente...</div>
                </div>
            </div>
        </div>
    </div>

    <script>
        // Polling-based approach (plus fiable que WebSocket dans certains cas)
        let pollInterval = null;

        function log(msg) {
            const area = document.getElementById('logArea');
            const time = new Date().toLocaleTimeString();
            area.innerHTML = `[${time}] ${msg}<br>` + area.innerHTML;
            if (area.children.length > 20) {
                area.removeChild(area.lastChild);
            }
        }

        function cmd(command) {
            log('Envoi: ' + command);
            fetch('/api/command', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({command: command})
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    log('OK: ' + command);
                } else {
                    log('Erreur: ' + (data.error || 'echec'));
                }
            })
            .catch(e => log('Erreur reseau: ' + e));
        }

        function updateStatus() {
            fetch('/api/status')
            .then(r => r.json())
            .then(data => {
                // Robot
                const robotDot = document.getElementById('robotDot');
                const robotStatus = document.getElementById('robotStatus');
                if (data.robot_connected) {
                    robotDot.classList.add('on');
                    robotStatus.textContent = 'Robot: OK';
                } else {
                    robotDot.classList.remove('on');
                    robotStatus.textContent = 'Robot: Deconnecte';
                }

                // Camera
                const cameraDot = document.getElementById('cameraDot');
                const cameraStatus = document.getElementById('cameraStatus');
                if (data.camera_streaming) {
                    cameraDot.classList.add('on');
                    cameraStatus.textContent = 'Camera: ON';
                } else if (data.camera_available) {
                    cameraDot.classList.remove('on');
                    cameraStatus.textContent = 'Camera: OFF';
                } else {
                    cameraDot.classList.remove('on');
                    cameraStatus.textContent = 'Camera: N/A';
                }

                // Sensors
                if (data.last_status) {
                    const s = data.last_status;
                    if (s.distance !== undefined) {
                        document.getElementById('distanceValue').textContent = s.distance.toFixed(1);
                    }
                    if (s.light !== undefined) {
                        document.getElementById('lightValue').textContent = s.light;
                    }
                    if (s.baseline !== undefined) {
                        document.getElementById('baselineValue').textContent = s.baseline.toFixed(1);
                    }
                    if (s.uptime !== undefined) {
                        document.getElementById('uptime').textContent = 'Uptime: ' + s.uptime + 's';
                    }
                }
            })
            .catch(e => {});
        }

        function startCamera() {
            log('Demarrage camera...');
            fetch('/api/camera/start', {method: 'POST'})
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    log('Camera demarree');
                    showCameraFeed();
                } else {
                    log('Erreur camera: ' + (data.error || 'echec'));
                }
            })
            .catch(e => log('Erreur: ' + e));
        }

        function stopCamera() {
            log('Arret camera...');
            fetch('/api/camera/stop', {method: 'POST'})
            .then(r => r.json())
            .then(data => {
                log('Camera arretee');
                hideCameraFeed();
            });
        }

        function showCameraFeed() {
            document.getElementById('cameraPlaceholder').style.display = 'none';
            const feed = document.getElementById('cameraFeed');
            feed.src = '/video_feed?' + Date.now();
            feed.style.display = 'block';
            document.getElementById('cameraOverlay').style.display = 'block';
        }

        function hideCameraFeed() {
            document.getElementById('cameraPlaceholder').style.display = 'flex';
            document.getElementById('cameraFeed').style.display = 'none';
            document.getElementById('cameraOverlay').style.display = 'none';
        }

        function snapshot() {
            window.open('/snapshot?' + Date.now(), '_blank');
        }

        // Raccourcis clavier
        document.addEventListener('keydown', function(e) {
            switch(e.key) {
                case 'ArrowUp': case 'z': case 'w': cmd('forward'); break;
                case 'ArrowDown': case 's': cmd('backward'); break;
                case 'ArrowLeft': case 'q': case 'a': cmd('left'); break;
                case 'ArrowRight': case 'd': cmd('right'); break;
                case ' ': cmd('stop'); e.preventDefault(); break;
            }
        });

        // Demarrer le polling
        updateStatus();
        pollInterval = setInterval(updateStatus, 2000);
        log('Interface prete');
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

def get_camera_frame():
    """Capture une frame de la caméra"""
    with camera_lock:
        if not camera_streaming or camera is None:
            return None
        try:
            if camera_type == 'picamera2':
                import cv2
                frame = camera.capture_array()
                _, jpeg = cv2.imencode('.jpg', cv2.cvtColor(frame, cv2.COLOR_RGB2BGR),
                                       [cv2.IMWRITE_JPEG_QUALITY, 70])
                return jpeg.tobytes()
            elif camera_type == 'opencv':
                import cv2
                ret, frame = camera.read()
                if ret:
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
        time.sleep(0.05)  # ~20 FPS

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

def read_serial_messages():
    """Lit les messages du robot"""
    global serial_port, last_status, monitoring
    with serial_lock:
        if serial_port and serial_port.is_open:
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
                            elif msg_type == "alert":
                                alert_history.insert(0, data)
                                if len(alert_history) > 50:
                                    alert_history.pop()
                            logger.debug(f"Recu: {data}")
                        except json.JSONDecodeError:
                            logger.debug(f"Message non-JSON: {line}")
            except Exception as e:
                logger.error(f"Erreur lecture: {e}")

def serial_reader_thread():
    """Thread de lecture série"""
    while True:
        if robot_connected:
            read_serial_messages()
        time.sleep(0.1)

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
    """API: Envoyer une commande"""
    data = request.json or {}
    cmd = data.get('command', '')
    if cmd:
        success = send_serial_command(cmd)
        return jsonify({"success": success, "command": cmd})
    return jsonify({"success": False, "error": "No command"})

@app.route('/api/alerts')
def api_alerts():
    """API: Historique des alertes"""
    return jsonify(alert_history[:50])

# Camera API
@app.route('/api/camera/status')
def api_camera_status():
    """API: Status caméra"""
    return jsonify({
        "available": camera is not None,
        "streaming": camera_streaming,
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
    return jsonify({"success": True})

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
        # Démarrer temporairement pour la capture
        start_camera_stream()
        time.sleep(0.5)
    frame = get_camera_frame()
    if frame:
        return Response(frame, mimetype='image/jpeg',
                       headers={'Content-Disposition': 'inline; filename=snapshot.jpg'})
    return "Camera not available", 503

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
    parser.add_argument('--debug', '-d', action='store_true',
                       help='Mode debug')

    args = parser.parse_args()

    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)

    # Connexion série
    if not connect_serial(args.port):
        logger.warning(f"Robot non connecte sur {args.port}")

    # Initialisation caméra
    if not args.no_camera:
        init_camera()

    # Thread de lecture série
    reader = threading.Thread(target=serial_reader_thread, daemon=True)
    reader.start()

    # Démarrer le serveur
    logger.info(f"Serveur web: http://{args.host}:{args.web_port}")
    if camera:
        logger.info(f"Camera: {camera_type}")

    try:
        # Utiliser le serveur Flask simple (plus fiable)
        app.run(host=args.host, port=args.web_port,
                debug=args.debug, threaded=True, use_reloader=False)
    except KeyboardInterrupt:
        pass
    finally:
        stop_camera_stream()
        disconnect_serial()

if __name__ == "__main__":
    main()
