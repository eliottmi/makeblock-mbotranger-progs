/**
 * mBot Ranger Pi Notifier
 *
 * Interface le mBot Ranger avec un Raspberry Pi pour envoyer
 * des notifications par email lors de détections d'événements.
 *
 * Communication: Série USB (115200 baud)
 * Protocole: Messages JSON simples
 *
 * Événements détectés:
 * - Obstacle proche
 * - Mouvement détecté (changement de distance)
 * - Batterie faible (simulation)
 * - Bouton pressé
 *
 * Matériel: Makeblock mBot Ranger + Raspberry Pi
 *           Capteur ultrasonique sur port 10
 *
 * @author Makeblock Community
 * @license MIT
 */

#include <MeAuriga.h>

// ==================== CONFIGURATION MATÉRIELLE ====================

MeEncoderOnBoard motorLeft(SLOT1);
MeEncoderOnBoard motorRight(SLOT2);
MeUltrasonicSensor ultraSensor(PORT_10);
MeRGBLed rgbLed(0, 12);
MeBuzzer buzzer;
MeLightSensor lightSensor(PORT_6);  // Optionnel: capteur de lumière

#define BUZZER_PIN 45

// Bouton intégré sur la carte Auriga
#define BUTTON_PIN 21

// ==================== PARAMÈTRES ====================

// Seuils de détection
const float OBSTACLE_DISTANCE = 30.0;      // Distance obstacle (cm)
const float MOTION_THRESHOLD = 15.0;       // Seuil de mouvement (cm)
const int LIGHT_THRESHOLD = 100;           // Seuil de lumière (0-1023)
const unsigned long DEBOUNCE_TIME = 5000;  // Anti-rebond notifications (ms)

// Timing
const unsigned long SCAN_INTERVAL = 200;   // Intervalle de scan (ms)
const unsigned long HEARTBEAT_INTERVAL = 30000; // Heartbeat (30s)
const unsigned long STATUS_INTERVAL = 10000;    // Status périodique (10s)

// ==================== VARIABLES ====================

float lastDistance = 0;
float baselineDistance = 0;
int lastLightLevel = 0;
bool lastButtonState = HIGH;

unsigned long lastScanTime = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long lastStatusTime = 0;
unsigned long lastObstacleNotif = 0;
unsigned long lastMotionNotif = 0;
unsigned long lastLightNotif = 0;

bool monitoringEnabled = false;
bool obstacleAlertEnabled = true;
bool motionAlertEnabled = true;
bool lightAlertEnabled = false;

// Buffer pour commandes série
String serialBuffer = "";

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);

  // Attendre la connexion série
  delay(1000);

  // Initialisation
  buzzer.setpin(BUZZER_PIN);
  rgbLed.setpin(44);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // LED de démarrage
  setLedColor(0, 0, 50);

  // Message de démarrage
  sendEvent("startup", "mBot Ranger Pi Notifier ready");

  // Calibration initiale
  calibrate();

  // Prêt
  setLedColor(0, 50, 0);
  sendEvent("ready", "System initialized");
}

// ==================== LOOP PRINCIPAL ====================

void loop() {
  // Lire les commandes du Raspberry Pi
  handleSerialInput();

  // Scan périodique
  if (millis() - lastScanTime >= SCAN_INTERVAL) {
    lastScanTime = millis();

    if (monitoringEnabled) {
      performScan();
    }

    checkButton();
  }

  // Heartbeat périodique
  if (millis() - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
    lastHeartbeatTime = millis();
    sendHeartbeat();
  }

  // Status périodique
  if (monitoringEnabled && millis() - lastStatusTime >= STATUS_INTERVAL) {
    lastStatusTime = millis();
    sendStatus();
  }
}

// ==================== COMMUNICATION SÉRIE ====================

void handleSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (serialBuffer.length() > 0) {
        processCommand(serialBuffer);
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
    }
  }
}

void processCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "start" || cmd == "monitor") {
    startMonitoring();
  }
  else if (cmd == "stop") {
    stopMotors();
    // Ne pas arrêter le monitoring automatiquement, juste les moteurs
  }
  else if (cmd == "stop_monitor") {
    stopMonitoring();
  }
  else if (cmd == "status") {
    sendStatus();
  }
  else if (cmd == "calibrate") {
    calibrate();
  }
  else if (cmd == "ping") {
    sendEvent("pong", "Connection OK");
  }
  else if (cmd == "led_red") {
    setLedColor(100, 0, 0);
    sendAck("LED red");
  }
  else if (cmd == "led_green") {
    setLedColor(0, 100, 0);
    sendAck("LED green");
  }
  else if (cmd == "led_blue") {
    setLedColor(0, 0, 100);
    sendAck("LED blue");
  }
  else if (cmd == "led_off") {
    setLedColor(0, 0, 0);
    sendAck("LED off");
  }
  else if (cmd == "beep") {
    buzzer.tone(1000, 200);
    sendAck("Beep");
  }
  else if (cmd == "alarm") {
    playAlarm();
    sendAck("Alarm played");
  }
  else if (cmd == "forward") {
    moveForward(100);
    delay(500);
    stopMotors();
    sendAck("Moved forward");
  }
  else if (cmd == "backward") {
    moveBackward(100);
    delay(500);
    stopMotors();
    sendAck("Moved backward");
  }
  else if (cmd == "left") {
    spinLeft(100);
    delay(300);
    stopMotors();
    sendAck("Turned left");
  }
  else if (cmd == "right") {
    spinRight(100);
    delay(300);
    stopMotors();
    sendAck("Turned right");
  }
  else if (cmd == "help") {
    sendHelp();
  }
  else if (cmd.startsWith("move:")) {
    // Format: move:leftSpeed:rightSpeed
    // Vitesses de -255 à 255
    int firstColon = cmd.indexOf(':', 5);
    if (firstColon > 0) {
      int leftSpeed = cmd.substring(5, firstColon).toInt();
      int rightSpeed = cmd.substring(firstColon + 1).toInt();
      setMotors(leftSpeed, rightSpeed);
      // Pas de ACK pour éviter de saturer la liaison série
    }
  }
  else if (cmd.startsWith("obstacle_")) {
    obstacleAlertEnabled = cmd.endsWith("on");
    sendAck(obstacleAlertEnabled ? "Obstacle alerts ON" : "Obstacle alerts OFF");
  }
  else if (cmd.startsWith("motion_")) {
    motionAlertEnabled = cmd.endsWith("on");
    sendAck(motionAlertEnabled ? "Motion alerts ON" : "Motion alerts OFF");
  }
  else {
    sendError("Unknown command: " + cmd);
  }
}

// ==================== ENVOI DE MESSAGES ====================

void sendEvent(const char* type, const char* message) {
  Serial.print("{\"type\":\"");
  Serial.print(type);
  Serial.print("\",\"message\":\"");
  Serial.print(message);
  Serial.print("\",\"timestamp\":");
  Serial.print(millis());
  Serial.println("}");
}

void sendAlert(const char* alertType, const char* message, float value) {
  Serial.print("{\"type\":\"alert\",\"alert\":\"");
  Serial.print(alertType);
  Serial.print("\",\"message\":\"");
  Serial.print(message);
  Serial.print("\",\"value\":");
  Serial.print(value);
  Serial.print(",\"timestamp\":");
  Serial.print(millis());
  Serial.println("}");

  // Feedback visuel
  flashLed(255, 0, 0, 3);
  buzzer.tone(1500, 100);
}

void sendStatus() {
  float distance = ultraSensor.distanceCm();
  int light = lightSensor.read();

  Serial.print("{\"type\":\"status\",\"distance\":");
  Serial.print(distance);
  Serial.print(",\"baseline\":");
  Serial.print(baselineDistance);
  Serial.print(",\"light\":");
  Serial.print(light);
  Serial.print(",\"monitoring\":");
  Serial.print(monitoringEnabled ? "true" : "false");
  Serial.print(",\"uptime\":");
  Serial.print(millis() / 1000);
  Serial.println("}");
}

void sendHeartbeat() {
  Serial.print("{\"type\":\"heartbeat\",\"uptime\":");
  Serial.print(millis() / 1000);
  Serial.println("}");
}

void sendAck(const char* message) {
  Serial.print("{\"type\":\"ack\",\"message\":\"");
  Serial.print(message);
  Serial.println("\"}");
}

void sendError(String message) {
  Serial.print("{\"type\":\"error\",\"message\":\"");
  Serial.print(message);
  Serial.println("\"}");
}

void sendHelp() {
  Serial.println("{\"type\":\"help\",\"commands\":[");
  Serial.println("\"start/monitor - Start monitoring\",");
  Serial.println("\"stop - Stop motors/monitoring\",");
  Serial.println("\"status - Get current status\",");
  Serial.println("\"calibrate - Recalibrate sensors\",");
  Serial.println("\"ping - Test connection\",");
  Serial.println("\"led_red/green/blue/off - Control LED\",");
  Serial.println("\"beep - Play beep\",");
  Serial.println("\"alarm - Play alarm\",");
  Serial.println("\"forward/backward/left/right - Move robot\",");
  Serial.println("\"move:left:right - Set motor speeds (-255 to 255)\",");
  Serial.println("\"obstacle_on/off - Toggle obstacle alerts\",");
  Serial.println("\"motion_on/off - Toggle motion alerts\"");
  Serial.println("]}");
}

// ==================== MONITORING ====================

void startMonitoring() {
  monitoringEnabled = true;
  calibrate();
  setLedColor(0, 0, 100);  // Bleu = monitoring
  sendEvent("monitoring", "Monitoring started");
}

void stopMonitoring() {
  monitoringEnabled = false;
  setLedColor(0, 50, 0);  // Vert = idle
  sendEvent("monitoring", "Monitoring stopped");
}

void calibrate() {
  setLedColor(255, 255, 0);  // Jaune = calibration

  float sum = 0;
  int validSamples = 0;

  for (int i = 0; i < 10; i++) {
    float d = ultraSensor.distanceCm();
    if (d > 0 && d < 400) {
      sum += d;
      validSamples++;
    }
    delay(50);
  }

  if (validSamples > 0) {
    baselineDistance = sum / validSamples;
    lastDistance = baselineDistance;
  } else {
    baselineDistance = 200;
  }

  lastLightLevel = lightSensor.read();

  sendEvent("calibration", "Calibration complete");

  if (monitoringEnabled) {
    setLedColor(0, 0, 100);
  } else {
    setLedColor(0, 50, 0);
  }
}

void performScan() {
  float distance = ultraSensor.distanceCm();
  int lightLevel = lightSensor.read();

  // Ignorer les valeurs invalides
  if (distance <= 0 || distance > 400) {
    return;
  }

  // Détection d'obstacle proche
  if (obstacleAlertEnabled && distance < OBSTACLE_DISTANCE) {
    if (millis() - lastObstacleNotif > DEBOUNCE_TIME) {
      lastObstacleNotif = millis();
      sendAlert("obstacle", "Obstacle detected nearby", distance);
    }
  }

  // Détection de mouvement (changement significatif)
  if (motionAlertEnabled) {
    float diff = abs(distance - lastDistance);
    if (diff > MOTION_THRESHOLD && lastDistance > 0) {
      if (millis() - lastMotionNotif > DEBOUNCE_TIME) {
        lastMotionNotif = millis();
        if (distance < lastDistance) {
          sendAlert("motion", "Object approaching", distance);
        } else {
          sendAlert("motion", "Object moving away", distance);
        }
      }
    }
  }

  // Détection de changement de lumière
  if (lightAlertEnabled) {
    int lightDiff = abs(lightLevel - lastLightLevel);
    if (lightDiff > LIGHT_THRESHOLD) {
      if (millis() - lastLightNotif > DEBOUNCE_TIME) {
        lastLightNotif = millis();
        sendAlert("light", "Light level changed", lightLevel);
      }
    }
    lastLightLevel = lightLevel;
  }

  lastDistance = distance;
}

void checkButton() {
  bool buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW && lastButtonState == HIGH) {
    // Bouton pressé
    sendAlert("button", "Button pressed", 1);
    buzzer.tone(800, 100);
  }

  lastButtonState = buttonState;
}

// ==================== CONTRÔLE DES MOTEURS ====================

void moveForward(int speed) {
  motorLeft.setMotorPwm(-speed);
  motorRight.setMotorPwm(speed);
}

void moveBackward(int speed) {
  motorLeft.setMotorPwm(speed);
  motorRight.setMotorPwm(-speed);
}

void spinLeft(int speed) {
  motorLeft.setMotorPwm(speed);
  motorRight.setMotorPwm(speed);
}

void spinRight(int speed) {
  motorLeft.setMotorPwm(-speed);
  motorRight.setMotorPwm(-speed);
}

void stopMotors() {
  motorLeft.setMotorPwm(0);
  motorRight.setMotorPwm(0);
}

void setMotors(int leftSpeed, int rightSpeed) {
  // Limiter les vitesses à -255/+255
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  // Appliquer aux moteurs (polarité inversée pour le moteur gauche)
  motorLeft.setMotorPwm(-leftSpeed);
  motorRight.setMotorPwm(rightSpeed);
}

// ==================== CONTRÔLE DES LEDs ====================

void setLedColor(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setColor(0, r, g, b);
  rgbLed.show();
}

void flashLed(uint8_t r, uint8_t g, uint8_t b, int times) {
  for (int i = 0; i < times; i++) {
    setLedColor(r, g, b);
    delay(100);
    setLedColor(0, 0, 0);
    delay(100);
  }

  // Restaurer la couleur selon l'état
  if (monitoringEnabled) {
    setLedColor(0, 0, 100);
  } else {
    setLedColor(0, 50, 0);
  }
}

void playAlarm() {
  for (int i = 0; i < 5; i++) {
    setLedColor(255, 0, 0);
    buzzer.tone(1000, 150);
    delay(200);
    setLedColor(0, 0, 255);
    buzzer.tone(800, 150);
    delay(200);
  }
  setLedColor(0, 0, 0);
}
