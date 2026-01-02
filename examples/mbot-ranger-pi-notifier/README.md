# mBot Ranger Pi Notifier

Interface entre le mBot Ranger et un Raspberry Pi pour envoyer des notifications par email et contrôler le robot via une interface web.

## Architecture

```
┌─────────────────┐     USB/Série     ┌─────────────────┐     Internet     ┌─────────────┐
│   mBot Ranger   │ ◄───────────────► │  Raspberry Pi   │ ───────────────► │    Email    │
│   (Arduino)     │    JSON 115200    │   (Python)      │      SMTP        │   Server    │
└─────────────────┘                   └─────────────────┘                  └─────────────┘
       │                                      │
       ▼                                      ▼
  Capteur ultrason                    pi_notifier.py (CLI)
  LEDs, Buzzer                        web_controller.py (Web)
  Moteurs                             Interface navigateur
```

## Fonctionnalités

### Détections (Arduino → Pi)
- **Obstacle** : Objet détecté à moins de 30 cm
- **Mouvement** : Changement de distance > 15 cm
- **Bouton** : Appui sur le bouton intégré
- **Lumière** : Changement de niveau lumineux (optionnel)

### Contrôle (Pi → Arduino)
- Démarrer/arrêter le monitoring
- Contrôler les LEDs
- Déplacer le robot
- Jouer des sons

### Interface Web
- Contrôle en temps réel via navigateur
- Affichage des capteurs (distance, lumière)
- Historique des alertes
- Raccourcis clavier (flèches, ZQSD)
- **Streaming vidéo live** de la caméra Pi
- Capture de snapshots

## Matériel requis

### mBot Ranger
- Carte Me Auriga
- Capteur ultrasonique (port 10)
- Câble USB

### Raspberry Pi
- Raspberry Pi (tout modèle avec USB)
- Connexion Internet (WiFi ou Ethernet)
- Python 3.x
- **Optionnel**: Caméra Pi ou webcam USB

## Installation

### 1. Côté mBot Ranger

1. Ouvrir `mbot-ranger-pi-notifier.ino` dans Arduino IDE
2. Sélectionner **Tools > Board > Arduino Mega 2560**
3. Téléverser le programme

### 2. Côté Raspberry Pi

```bash
# Installer les dépendances (CLI uniquement)
pip3 install pyserial

# Installer les dépendances (avec interface web)
pip3 install pyserial flask flask-socketio

# Installer les dépendances caméra (optionnel)
# Pour Pi Camera (recommandé sur Raspberry Pi):
pip3 install picamera2 opencv-python
# Pour webcam USB uniquement:
pip3 install opencv-python

# Cloner le projet (ou copier les fichiers)
cd /home/pi
git clone <repo_url>
cd mbot-ranger-pi-notifier

# Créer la configuration (pour les emails)
cp config.example.json config.json
nano config.json  # Éditer avec vos paramètres
```

### 3. Configuration email (Gmail)

Pour Gmail, vous devez créer un **mot de passe d'application**:

1. Activer la validation en 2 étapes sur votre compte Google
2. Aller dans: Compte Google > Sécurité > Mots de passe des applications
3. Créer un nouveau mot de passe pour "Mail" sur "Autre"
4. Utiliser ce mot de passe dans `config.json`

## Utilisation

### Connexion du robot

1. Connecter le mBot Ranger au Raspberry Pi via USB
2. Trouver le port série:
   ```bash
   ls /dev/ttyUSB*
   # ou
   ls /dev/ttyACM*
   ```

### Démarrer le monitoring

```bash
# Avec le port par défaut (/dev/ttyUSB0)
python3 pi_notifier.py

# Avec un port spécifique
python3 pi_notifier.py --port /dev/ttyACM0

# Avec un fichier de configuration
python3 pi_notifier.py --config config.json

# Mode debug
python3 pi_notifier.py --debug
```

### Mode interactif

```bash
python3 pi_notifier.py --interactive

mBot> start      # Démarrer le monitoring
mBot> status     # Obtenir le statut
mBot> led_red    # LED rouge
mBot> forward    # Avancer
mBot> quit       # Quitter
```

### Interface Web

```bash
# Démarrer le serveur web (accessible sur tout le réseau)
python3 web_controller.py

# Avec un port spécifique
python3 web_controller.py --web-port 8080

# Avec un port série spécifique
python3 web_controller.py --port /dev/ttyACM0

# Mode debug
python3 web_controller.py --debug
```

Ouvrir dans un navigateur: `http://<ip-du-raspberry>:5000`

**Fonctionnalités de l'interface web:**
- **Streaming vidéo live** de la caméra Pi/USB
- Contrôle de mouvement (flèches directionnelles)
- Contrôle des LEDs et sons
- Affichage temps réel des capteurs
- Activation/désactivation du monitoring
- Historique des alertes
- Capture de snapshots
- Raccourcis clavier: Flèches ou ZQSD + Espace (stop)

**Options caméra:**
```bash
# Désactiver la caméra
python3 web_controller.py --no-camera

# La caméra détecte automatiquement:
# 1. Pi Camera (via picamera2)
# 2. Webcam USB (via OpenCV)
```

## Configuration

### config.json

```json
{
  "serial_port": "/dev/ttyUSB0",
  "baud_rate": 115200,
  "email": {
    "enabled": true,
    "smtp_server": "smtp.gmail.com",
    "smtp_port": 587,
    "sender_email": "votre_email@gmail.com",
    "sender_password": "mot_de_passe_app",
    "recipient_email": "destinataire@email.com"
  },
  "alerts": {
    "obstacle": true,
    "motion": true,
    "button": true,
    "light": false
  },
  "cooldown_seconds": 60
}
```

### Paramètres

| Paramètre | Description |
|-----------|-------------|
| `serial_port` | Port série du robot |
| `email.enabled` | Activer l'envoi d'emails |
| `alerts.*` | Types d'alertes à notifier |
| `cooldown_seconds` | Délai entre deux alertes identiques |

## Protocole de communication

### Messages JSON (Arduino → Pi)

**Alerte:**
```json
{
  "type": "alert",
  "alert": "obstacle",
  "message": "Obstacle detected nearby",
  "value": 25.5,
  "timestamp": 12345
}
```

**Status:**
```json
{
  "type": "status",
  "distance": 150.0,
  "baseline": 200.0,
  "light": 512,
  "monitoring": true,
  "uptime": 3600
}
```

**Heartbeat:**
```json
{
  "type": "heartbeat",
  "uptime": 3600
}
```

### Commandes (Pi → Arduino)

| Commande | Description |
|----------|-------------|
| `start` / `monitor` | Démarrer le monitoring |
| `stop` | Arrêter le monitoring |
| `status` | Demander le statut |
| `calibrate` | Recalibrer les capteurs |
| `ping` | Test de connexion |
| `led_red/green/blue/off` | Contrôler les LEDs |
| `beep` | Jouer un bip |
| `alarm` | Jouer l'alarme |
| `forward/backward/left/right` | Déplacer le robot |
| `obstacle_on/off` | Activer/désactiver alertes obstacle |
| `motion_on/off` | Activer/désactiver alertes mouvement |

## Service systemd (optionnel)

Pour démarrer automatiquement au boot:

### Service CLI (notifications email)

```bash
sudo nano /etc/systemd/system/mbot-notifier.service
```

```ini
[Unit]
Description=mBot Ranger Pi Notifier
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/mbot-ranger-pi-notifier
ExecStart=/usr/bin/python3 pi_notifier.py --config config.json
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

### Service Web (interface navigateur)

```bash
sudo nano /etc/systemd/system/mbot-web.service
```

```ini
[Unit]
Description=mBot Ranger Web Controller
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/mbot-ranger-pi-notifier
ExecStart=/usr/bin/python3 web_controller.py --port /dev/ttyUSB0
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

### Activation des services

```bash
# Pour le service CLI
sudo systemctl enable mbot-notifier
sudo systemctl start mbot-notifier

# Pour le service Web
sudo systemctl enable mbot-web
sudo systemctl start mbot-web

# Vérifier le statut
sudo systemctl status mbot-notifier
sudo systemctl status mbot-web
```

## Dépannage

### Le port série n'est pas trouvé
```bash
# Lister les ports
ls -la /dev/ttyUSB* /dev/ttyACM*

# Ajouter l'utilisateur au groupe dialout
sudo usermod -a -G dialout $USER
# Puis se reconnecter
```

### Permission denied sur le port
```bash
sudo chmod 666 /dev/ttyUSB0
# ou
sudo usermod -a -G dialout pi
```

### Les emails ne sont pas envoyés
- Vérifier la connexion Internet
- Vérifier les paramètres SMTP
- Pour Gmail: utiliser un mot de passe d'application
- Vérifier les logs: `python3 pi_notifier.py --debug`

### Le robot ne répond pas
- Vérifier la connexion USB
- Vérifier que le bon port est utilisé
- Redémarrer le robot

## Exemples d'utilisation

### Surveillance d'une pièce
1. Placer le robot dans la pièce
2. Démarrer le script sur le Pi
3. Recevoir un email si quelqu'un entre

### Alarme connectée
1. Combiner avec le projet Guardian
2. Modifier le script pour envoyer des SMS (Twilio)
3. Ajouter une caméra Pi pour capturer des images

### Domotique
1. Intégrer avec Home Assistant
2. Déclencher des actions sur détection
3. Contrôler le robot via l'interface domotique

## Licence

MIT License
