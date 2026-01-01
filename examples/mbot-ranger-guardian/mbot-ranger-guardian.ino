/**
 * mBot Ranger Guardian (Gardien)
 *
 * Robot de surveillance qui patrouille une zone et alerte en cas d'intrusion:
 * - Mode Patrouille: se déplace et surveille
 * - Mode Sentinelle: reste statique et surveille
 * - Détection d'intrusion par capteur ultrasonique
 * - Alarme sonore et visuelle
 *
 * Matériel: Makeblock mBot Ranger (carte Me Auriga)
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

#define BUZZER_PIN 45

// ==================== PARAMÈTRES ====================

// Vitesses
const int SPEED_PATROL = 100;
const int SPEED_TURN = 80;

// Détection
const float DETECTION_DISTANCE = 50.0;   // Distance de détection (cm)
const float BASELINE_TOLERANCE = 20.0;   // Tolérance pour changement (cm)
const int DETECTION_SAMPLES = 3;         // Échantillons pour confirmer

// Timing
const unsigned long PATROL_SEGMENT_TIME = 2000;  // Durée d'un segment (ms)
const unsigned long SCAN_INTERVAL = 100;         // Intervalle de scan (ms)
const unsigned long ALARM_DURATION = 5000;       // Durée de l'alarme (ms)

// ==================== ÉTATS ====================

enum GuardianMode {
  MODE_IDLE,
  MODE_PATROL,
  MODE_SENTINEL,
  MODE_ALARM,
  MODE_TRACKING
};

enum PatrolState {
  PATROL_FORWARD,
  PATROL_TURN_LEFT,
  PATROL_TURN_RIGHT,
  PATROL_BACKWARD
};

GuardianMode currentMode = MODE_IDLE;
PatrolState patrolState = PATROL_FORWARD;

// ==================== VARIABLES ====================

float baselineDistance = 0;           // Distance de référence
unsigned long lastScanTime = 0;
unsigned long patrolStateStartTime = 0;
unsigned long alarmStartTime = 0;
int patrolStep = 0;
int detectionCount = 0;
bool intruderDetected = false;

// Historique des distances pour lissage
const int DISTANCE_HISTORY_SIZE = 5;
float distanceHistory[DISTANCE_HISTORY_SIZE];
int historyIndex = 0;

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  Serial.println("mBot Ranger Guardian");
  Serial.println("====================");

  // Initialisation
  buzzer.setpin(BUZZER_PIN);
  rgbLed.setpin(44);
  clearLeds();

  // Initialiser l'historique des distances
  for (int i = 0; i < DISTANCE_HISTORY_SIZE; i++) {
    distanceHistory[i] = 0;
  }

  // Animation de démarrage
  startupAnimation();

  // Afficher les commandes
  printHelp();

  // LED verte = prêt
  setLedColor(0, 50, 0);
}

// ==================== LOOP PRINCIPAL ====================

void loop() {
  // Lire les commandes série
  handleSerialCommands();

  // Scanner périodiquement
  if (millis() - lastScanTime >= SCAN_INTERVAL) {
    lastScanTime = millis();
    performScan();
  }

  // Machine à états
  switch (currentMode) {
    case MODE_PATROL:
      patrolLoop();
      break;

    case MODE_SENTINEL:
      sentinelLoop();
      break;

    case MODE_ALARM:
      alarmLoop();
      break;

    case MODE_TRACKING:
      trackingLoop();
      break;

    default:
      break;
  }
}

// ==================== COMMANDES SÉRIE ====================

void handleSerialCommands() {
  if (Serial.available()) {
    char cmd = Serial.read();

    switch (cmd) {
      case 'p':
      case 'P':
        startPatrol();
        break;

      case 's':
      case 'S':
        startSentinel();
        break;

      case 't':
      case 'T':
        startTracking();
        break;

      case 'q':
      case 'Q':
        stopAll();
        break;

      case 'c':
      case 'C':
        calibrate();
        break;

      case 'h':
      case 'H':
      case '?':
        printHelp();
        break;
    }
  }
}

void printHelp() {
  Serial.println("\n=== COMMANDES ===");
  Serial.println("p - Mode Patrouille (déplacement + surveillance)");
  Serial.println("s - Mode Sentinelle (statique + surveillance)");
  Serial.println("t - Mode Tracking (suit les mouvements)");
  Serial.println("c - Calibrer la distance de référence");
  Serial.println("q - Arrêter");
  Serial.println("h - Aide");
  Serial.println();
}

// ==================== MODES DE FONCTIONNEMENT ====================

void startPatrol() {
  Serial.println("\n*** MODE PATROUILLE ACTIVÉ ***");
  Serial.println("Le gardien patrouille la zone...\n");

  currentMode = MODE_PATROL;
  patrolState = PATROL_FORWARD;
  patrolStep = 0;
  patrolStateStartTime = millis();
  intruderDetected = false;

  calibrate();

  // LED bleue = patrouille
  setLedColor(0, 0, 100);
  buzzer.tone(800, 200);
}

void startSentinel() {
  Serial.println("\n*** MODE SENTINELLE ACTIVÉ ***");
  Serial.println("Le gardien surveille la zone...\n");

  currentMode = MODE_SENTINEL;
  intruderDetected = false;
  stopMotors();

  calibrate();

  // LED cyan = sentinelle
  setLedColor(0, 100, 100);
  buzzer.tone(600, 200);
}

void startTracking() {
  Serial.println("\n*** MODE TRACKING ACTIVÉ ***");
  Serial.println("Le gardien suit les mouvements...\n");

  currentMode = MODE_TRACKING;
  intruderDetected = false;

  // LED magenta = tracking
  setLedColor(100, 0, 100);
  buzzer.tone(1000, 200);
}

void stopAll() {
  Serial.println("\n*** ARRÊT ***\n");
  currentMode = MODE_IDLE;
  stopMotors();
  intruderDetected = false;
  detectionCount = 0;

  // LED verte = veille
  setLedColor(0, 50, 0);
  buzzer.tone(400, 200);
}

void triggerAlarm() {
  Serial.println("\n!!! ALERTE INTRUSION !!!\n");
  currentMode = MODE_ALARM;
  alarmStartTime = millis();
  stopMotors();
}

// ==================== SCAN ET DÉTECTION ====================

void performScan() {
  float distance = ultraSensor.distanceCm();

  // Ignorer les valeurs invalides
  if (distance <= 0 || distance > 400) {
    return;
  }

  // Ajouter à l'historique
  distanceHistory[historyIndex] = distance;
  historyIndex = (historyIndex + 1) % DISTANCE_HISTORY_SIZE;

  // Calculer la moyenne
  float avgDistance = getAverageDistance();

  // Vérifier intrusion (seulement si on n'est pas déjà en alarme)
  if (currentMode != MODE_ALARM && currentMode != MODE_IDLE) {
    checkForIntrusion(avgDistance);
  }
}

float getAverageDistance() {
  float sum = 0;
  int count = 0;

  for (int i = 0; i < DISTANCE_HISTORY_SIZE; i++) {
    if (distanceHistory[i] > 0) {
      sum += distanceHistory[i];
      count++;
    }
  }

  return (count > 0) ? (sum / count) : 0;
}

void checkForIntrusion(float currentDistance) {
  // Quelque chose de plus proche que la baseline?
  if (currentDistance < DETECTION_DISTANCE &&
      currentDistance < baselineDistance - BASELINE_TOLERANCE) {

    detectionCount++;

    // Afficher un indicateur
    if (detectionCount == 1) {
      Serial.print("! Mouvement détecté à ");
      Serial.print(currentDistance);
      Serial.println(" cm");
    }

    // Confirmer après plusieurs détections
    if (detectionCount >= DETECTION_SAMPLES) {
      intruderDetected = true;
      triggerAlarm();
    }
  } else {
    // Reset du compteur si pas de détection continue
    if (detectionCount > 0) {
      detectionCount--;
    }
  }
}

void calibrate() {
  Serial.println("Calibration en cours...");
  setLedColor(255, 255, 0);  // Jaune

  // Prendre plusieurs mesures
  float sum = 0;
  int validSamples = 0;

  for (int i = 0; i < 10; i++) {
    float d = ultraSensor.distanceCm();
    if (d > 0 && d < 400) {
      sum += d;
      validSamples++;
    }
    delay(100);
  }

  if (validSamples > 0) {
    baselineDistance = sum / validSamples;
    Serial.print("Distance de référence: ");
    Serial.print(baselineDistance);
    Serial.println(" cm");
  } else {
    baselineDistance = 200;  // Valeur par défaut
    Serial.println("Calibration échouée, valeur par défaut: 200 cm");
  }

  // Remplir l'historique avec la baseline
  for (int i = 0; i < DISTANCE_HISTORY_SIZE; i++) {
    distanceHistory[i] = baselineDistance;
  }

  detectionCount = 0;
}

// ==================== MODE PATROUILLE ====================

void patrolLoop() {
  unsigned long elapsed = millis() - patrolStateStartTime;

  // Exécuter le mouvement actuel
  switch (patrolState) {
    case PATROL_FORWARD:
      breatheLed(0, 0, 255);  // Bleu pulsé
      moveForward(SPEED_PATROL);
      if (elapsed > PATROL_SEGMENT_TIME) {
        nextPatrolState();
      }
      break;

    case PATROL_TURN_LEFT:
      setLedColor(0, 100, 100);
      spinLeft(SPEED_TURN);
      if (elapsed > 600) {
        nextPatrolState();
      }
      break;

    case PATROL_TURN_RIGHT:
      setLedColor(100, 100, 0);
      spinRight(SPEED_TURN);
      if (elapsed > 600) {
        nextPatrolState();
      }
      break;

    case PATROL_BACKWARD:
      setLedColor(100, 0, 100);
      moveBackward(SPEED_PATROL);
      if (elapsed > PATROL_SEGMENT_TIME / 2) {
        nextPatrolState();
      }
      break;
  }

  // Vérifier les obstacles
  float distance = getAverageDistance();
  if (distance > 0 && distance < 20) {
    // Obstacle proche, reculer
    stopMotors();
    moveBackward(SPEED_PATROL);
    delay(300);
    patrolState = (random(2) == 0) ? PATROL_TURN_LEFT : PATROL_TURN_RIGHT;
    patrolStateStartTime = millis();
  }
}

void nextPatrolState() {
  patrolStep++;
  patrolStateStartTime = millis();

  // Séquence de patrouille: carré avec variations
  switch (patrolStep % 8) {
    case 0:
    case 2:
    case 4:
    case 6:
      patrolState = PATROL_FORWARD;
      break;
    case 1:
    case 5:
      patrolState = PATROL_TURN_RIGHT;
      break;
    case 3:
      patrolState = PATROL_TURN_LEFT;
      break;
    case 7:
      patrolState = PATROL_BACKWARD;
      break;
  }

  // Recalibrer périodiquement
  if (patrolStep % 16 == 0) {
    stopMotors();
    delay(500);
    calibrate();
  }
}

// ==================== MODE SENTINELLE ====================

void sentinelLoop() {
  // Reste immobile, juste surveille
  // Le scan est fait dans performScan()

  // Animation LED discrète
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 2000) {
    lastBlink = millis();
    setLedColor(0, 100, 100);
    delay(100);
    setLedColor(0, 30, 30);
  }

  // Afficher la distance périodiquement
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 3000) {
    lastPrint = millis();
    Serial.print("Distance: ");
    Serial.print(getAverageDistance());
    Serial.println(" cm");
  }
}

// ==================== MODE TRACKING ====================

void trackingLoop() {
  float distance = getAverageDistance();

  if (distance <= 0) {
    stopMotors();
    return;
  }

  // Suivre l'objet détecté
  if (distance < 30) {
    // Trop proche, reculer
    setLedColor(255, 0, 0);
    moveBackward(SPEED_PATROL);
  } else if (distance < 60) {
    // Distance idéale, rester
    setLedColor(0, 255, 0);
    stopMotors();
  } else if (distance < 150) {
    // Suivre
    setLedColor(100, 0, 100);
    moveForward(SPEED_PATROL);
  } else {
    // Trop loin, arrêter
    setLedColor(50, 50, 50);
    stopMotors();
  }

  // Afficher la distance
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 500) {
    lastPrint = millis();
    Serial.print("Tracking: ");
    Serial.print(distance);
    Serial.println(" cm");
  }
}

// ==================== MODE ALARME ====================

void alarmLoop() {
  unsigned long elapsed = millis() - alarmStartTime;

  // Sirène et LEDs
  int phase = (elapsed / 200) % 4;

  switch (phase) {
    case 0:
      setLedColor(255, 0, 0);
      buzzer.tone(1000, 150);
      break;
    case 1:
      setLedColor(0, 0, 255);
      buzzer.tone(800, 150);
      break;
    case 2:
      setLedColor(255, 0, 0);
      buzzer.tone(1200, 150);
      break;
    case 3:
      setLedColor(0, 0, 0);
      break;
  }

  // Fin de l'alarme
  if (elapsed > ALARM_DURATION) {
    Serial.println("Alarme terminée, retour en surveillance...\n");
    intruderDetected = false;
    detectionCount = 0;

    // Retourner au mode précédent ou sentinelle
    startSentinel();
  }
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

// ==================== CONTRÔLE DES LEDs ====================

void clearLeds() {
  rgbLed.setColor(0, 0, 0, 0);
  rgbLed.show();
}

void setLedColor(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setColor(0, r, g, b);
  rgbLed.show();
}

void breatheLed(uint8_t r, uint8_t g, uint8_t b) {
  // Effet de respiration
  float breath = (sin(millis() / 500.0) + 1) / 2;  // 0 à 1
  rgbLed.setColor(0, r * breath, g * breath, b * breath);
  rgbLed.show();
}

void startupAnimation() {
  // Animation circulaire
  for (int i = 1; i <= 12; i++) {
    rgbLed.setColor(i, 255, 50, 0);
    rgbLed.show();
    buzzer.tone(400 + i * 50, 30);
    delay(60);
    rgbLed.setColor(i, 30, 10, 0);
    rgbLed.show();
  }

  // Flash final
  setLedColor(0, 255, 0);
  buzzer.tone(1000, 200);
  delay(300);
  clearLeds();
}
