#!/usr/bin/env python3
"""
mBot Ranger Pi Notifier - Raspberry Pi Script

Ce script:
- Communique avec le mBot Ranger via port série USB
- Reçoit les alertes et notifications
- Envoie des emails lors des détections
- Permet de contrôler le robot à distance

Prérequis:
    pip3 install pyserial

Usage:
    python3 pi_notifier.py [--port /dev/ttyUSB0] [--config config.json]
"""

import serial
import json
import time
import smtplib
import argparse
import logging
import signal
import sys
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from datetime import datetime
from pathlib import Path

# Configuration par défaut
DEFAULT_CONFIG = {
    "serial_port": "/dev/ttyUSB0",
    "baud_rate": 115200,
    "email": {
        "enabled": False,
        "smtp_server": "smtp.gmail.com",
        "smtp_port": 587,
        "sender_email": "votre_email@gmail.com",
        "sender_password": "votre_mot_de_passe_app",
        "recipient_email": "destinataire@email.com"
    },
    "alerts": {
        "obstacle": True,
        "motion": True,
        "button": True,
        "light": False
    },
    "cooldown_seconds": 60
}

class MBotNotifier:
    def __init__(self, config):
        self.config = config
        self.serial_port = None
        self.running = False
        self.last_alert_time = {}

        # Setup logging
        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s - %(levelname)s - %(message)s'
        )
        self.logger = logging.getLogger(__name__)

    def connect(self):
        """Connexion au port série"""
        try:
            self.serial_port = serial.Serial(
                port=self.config["serial_port"],
                baudrate=self.config["baud_rate"],
                timeout=1
            )
            self.logger.info(f"Connecté à {self.config['serial_port']}")
            time.sleep(2)  # Attendre l'initialisation Arduino
            return True
        except serial.SerialException as e:
            self.logger.error(f"Erreur de connexion: {e}")
            return False

    def disconnect(self):
        """Déconnexion du port série"""
        if self.serial_port and self.serial_port.is_open:
            self.send_command("stop")
            self.serial_port.close()
            self.logger.info("Déconnecté")

    def send_command(self, command):
        """Envoie une commande au robot"""
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.write(f"{command}\n".encode())
            self.logger.debug(f"Envoyé: {command}")

    def read_message(self):
        """Lit un message du robot"""
        if self.serial_port and self.serial_port.is_open:
            try:
                line = self.serial_port.readline().decode('utf-8').strip()
                if line:
                    return json.loads(line)
            except json.JSONDecodeError:
                self.logger.debug(f"Message non-JSON: {line}")
            except Exception as e:
                self.logger.error(f"Erreur de lecture: {e}")
        return None

    def send_email(self, subject, body):
        """Envoie un email de notification"""
        if not self.config["email"]["enabled"]:
            self.logger.info(f"Email désactivé - Sujet: {subject}")
            return False

        try:
            msg = MIMEMultipart()
            msg['From'] = self.config["email"]["sender_email"]
            msg['To'] = self.config["email"]["recipient_email"]
            msg['Subject'] = f"[mBot Alert] {subject}"

            # Corps du message
            html_body = f"""
            <html>
            <body>
                <h2>🤖 Alerte mBot Ranger</h2>
                <p><strong>Type:</strong> {subject}</p>
                <p><strong>Message:</strong> {body}</p>
                <p><strong>Date:</strong> {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
                <hr>
                <p><em>Message automatique envoyé par mBot Ranger Pi Notifier</em></p>
            </body>
            </html>
            """
            msg.attach(MIMEText(html_body, 'html'))

            # Connexion SMTP
            server = smtplib.SMTP(
                self.config["email"]["smtp_server"],
                self.config["email"]["smtp_port"]
            )
            server.starttls()
            server.login(
                self.config["email"]["sender_email"],
                self.config["email"]["sender_password"]
            )
            server.send_message(msg)
            server.quit()

            self.logger.info(f"Email envoyé: {subject}")
            return True

        except Exception as e:
            self.logger.error(f"Erreur d'envoi email: {e}")
            return False

    def handle_alert(self, data):
        """Gère une alerte reçue"""
        alert_type = data.get("alert", "unknown")
        message = data.get("message", "")
        value = data.get("value", 0)

        # Vérifier si ce type d'alerte est activé
        if not self.config["alerts"].get(alert_type, False):
            self.logger.debug(f"Alerte {alert_type} ignorée (désactivée)")
            return

        # Vérifier le cooldown
        now = time.time()
        last_time = self.last_alert_time.get(alert_type, 0)
        cooldown = self.config["cooldown_seconds"]

        if now - last_time < cooldown:
            self.logger.debug(f"Alerte {alert_type} ignorée (cooldown)")
            return

        self.last_alert_time[alert_type] = now

        # Logger l'alerte
        self.logger.warning(f"ALERTE: {alert_type} - {message} (valeur: {value})")

        # Envoyer l'email
        subject = f"{alert_type.upper()}: {message}"
        body = f"Détection: {message}\nValeur mesurée: {value}\nTimestamp: {data.get('timestamp', 'N/A')}"
        self.send_email(subject, body)

    def handle_message(self, data):
        """Traite un message reçu"""
        msg_type = data.get("type", "unknown")

        if msg_type == "alert":
            self.handle_alert(data)
        elif msg_type == "status":
            self.logger.info(f"Status - Distance: {data.get('distance')}cm, "
                           f"Uptime: {data.get('uptime')}s")
        elif msg_type == "heartbeat":
            self.logger.debug(f"Heartbeat - Uptime: {data.get('uptime')}s")
        elif msg_type == "startup":
            self.logger.info(f"Robot démarré: {data.get('message')}")
        elif msg_type == "ready":
            self.logger.info("Robot prêt")
        elif msg_type == "ack":
            self.logger.info(f"ACK: {data.get('message')}")
        elif msg_type == "error":
            self.logger.error(f"Erreur robot: {data.get('message')}")
        else:
            self.logger.debug(f"Message: {data}")

    def run(self):
        """Boucle principale"""
        self.running = True
        self.logger.info("Démarrage du monitoring...")

        # Démarrer le monitoring sur le robot
        time.sleep(1)
        self.send_command("start")

        while self.running:
            try:
                data = self.read_message()
                if data:
                    self.handle_message(data)
                time.sleep(0.1)
            except KeyboardInterrupt:
                break
            except Exception as e:
                self.logger.error(f"Erreur: {e}")
                time.sleep(1)

        self.logger.info("Arrêt du monitoring")

    def stop(self):
        """Arrête le monitoring"""
        self.running = False


def load_config(config_path):
    """Charge la configuration depuis un fichier JSON"""
    if config_path and Path(config_path).exists():
        with open(config_path, 'r') as f:
            user_config = json.load(f)
            # Fusionner avec la config par défaut
            config = DEFAULT_CONFIG.copy()
            config.update(user_config)
            return config
    return DEFAULT_CONFIG


def create_sample_config(path="config.json"):
    """Crée un fichier de configuration exemple"""
    with open(path, 'w') as f:
        json.dump(DEFAULT_CONFIG, f, indent=2)
    print(f"Configuration exemple créée: {path}")


def interactive_mode(notifier):
    """Mode interactif pour envoyer des commandes"""
    print("\n=== Mode Interactif ===")
    print("Commandes: start, stop, status, calibrate, ping")
    print("           led_red, led_green, led_blue, led_off")
    print("           forward, backward, left, right")
    print("           beep, alarm, help, quit")
    print()

    while True:
        try:
            cmd = input("mBot> ").strip()
            if cmd.lower() == 'quit':
                break
            if cmd:
                notifier.send_command(cmd)
                time.sleep(0.5)
                # Lire la réponse
                data = notifier.read_message()
                if data:
                    print(f"Réponse: {json.dumps(data, indent=2)}")
        except KeyboardInterrupt:
            break
        except EOFError:
            break


def main():
    parser = argparse.ArgumentParser(description='mBot Ranger Pi Notifier')
    parser.add_argument('--port', '-p', default='/dev/ttyUSB0',
                       help='Port série (défaut: /dev/ttyUSB0)')
    parser.add_argument('--config', '-c', default=None,
                       help='Fichier de configuration JSON')
    parser.add_argument('--create-config', action='store_true',
                       help='Créer un fichier de configuration exemple')
    parser.add_argument('--interactive', '-i', action='store_true',
                       help='Mode interactif')
    parser.add_argument('--debug', '-d', action='store_true',
                       help='Mode debug')

    args = parser.parse_args()

    if args.create_config:
        create_sample_config()
        return

    # Charger la configuration
    config = load_config(args.config)
    config["serial_port"] = args.port

    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)

    # Créer le notifier
    notifier = MBotNotifier(config)

    # Gérer les signaux
    def signal_handler(sig, frame):
        print("\nArrêt...")
        notifier.stop()
        notifier.disconnect()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    # Connexion
    if not notifier.connect():
        print("Impossible de se connecter au robot")
        print("Vérifiez que le robot est connecté et le port est correct")
        print(f"Port utilisé: {config['serial_port']}")
        sys.exit(1)

    try:
        if args.interactive:
            interactive_mode(notifier)
        else:
            notifier.run()
    finally:
        notifier.disconnect()


if __name__ == "__main__":
    main()
