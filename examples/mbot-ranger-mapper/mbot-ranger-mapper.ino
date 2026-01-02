/**
 * mBot Ranger Mapper (Cartographe) v3
 *
 * Ce programme fait cartographier une zone au robot:
 * - Utilise le GYROSCOPE pour contrôler précisément les rotations
 * - Effectue un SCAN 360° complet avec contrôle gyroscopique
 * - Détecte les COLLISIONS via l'accéléromètre
 * - Construit une carte 2D de l'environnement
 * - Préchauffage des moteurs pour éviter reset batterie
 *
 * Commandes:
 *   s - Démarrer la cartographie
 *   t - Test scan 360°
 *   g - Debug gyroscope en live
 *   c - Calibrer gyroscope
 *   p - Afficher carte
 *   r - Réinitialiser
 *   q - Arrêter
 *
 * Matériel: Makeblock mBot Ranger (carte Me Auriga)
 *           Capteur ultrasonique sur port 10
 *           Gyroscope intégré sur la carte Auriga
 *
 * @author Makeblock Community
 * @license MIT
 */

#include <MeAuriga.h>
#include <Wire.h>
#include <math.h>

// ==================== CONFIGURATION MATÉRIELLE ====================

// Moteurs encodeurs
MeEncoderOnBoard motorLeft(SLOT1);
MeEncoderOnBoard motorRight(SLOT2);

// Capteur ultrasonique
MeUltrasonicSensor ultraSensor(PORT_10);

// Gyroscope intégré (sur la carte Auriga)
MeGyro gyro(1, 0x69);  // Port 1, adresse I2C

// LEDs RGB
MeRGBLed rgbLed(0, 12);

// Buzzer
MeBuzzer buzzer;
#define BUZZER_PIN 45

// ==================== PARAMÈTRES DU ROBOT ====================

// Dimensions du robot (en cm)
const float WHEEL_DIAMETER = 6.4;
const float WHEEL_BASE = 13.0;
const float PULSES_PER_REV = 9.0;
const float GEAR_RATIO = 39.267;
const float CM_PER_PULSE = (PI * WHEEL_DIAMETER) / (PULSES_PER_REV * GEAR_RATIO);

// Vitesses (augmentées pour assurer le mouvement)
const int SPEED_MOVE = 180;
const int SPEED_TURN = 150;
const int SPEED_SCAN = 120;  // Vitesse pour le scan rotatif

// ==================== PARAMÈTRES DE LA CARTE ====================

const int MAP_WIDTH = 25;
const int MAP_HEIGHT = 25;
const float CELL_SIZE = 10.0;  // 10 cm par cellule

// Types de cellules
const uint8_t CELL_UNKNOWN = 0;
const uint8_t CELL_FREE = 1;
const uint8_t CELL_OBSTACLE = 2;

// La carte
uint8_t gridMap[MAP_HEIGHT][MAP_WIDTH];

// ==================== PARAMÈTRES DE SCAN ====================

// Scan 360° avec mesures tous les 15 degrés
const int SCAN_STEP = 15;
const int NUM_SCAN_POINTS = 360 / SCAN_STEP;  // 24 mesures
float scanDistances[24];  // Distances mesurées
float scanAngles[24];     // Angles correspondants

// Seuils
const float MAX_DISTANCE = 150.0;     // Distance max fiable (cm)
const float MIN_DISTANCE = 5.0;       // Distance min (cm)
const float COLLISION_THRESHOLD = 2.0; // Seuil accéléromètre pour collision

// ==================== ODOMÉTRIE ====================

float robotX = MAP_WIDTH * CELL_SIZE / 2;
float robotY = MAP_HEIGHT * CELL_SIZE / 2;
float robotAngle = 0;  // Angle en radians (du gyroscope)
float gyroAngleOffset = 0;

// Encodeurs
volatile long encoderLeft = 0;
volatile long encoderRight = 0;
long lastEncoderLeft = 0;
long lastEncoderRight = 0;

// ==================== ÉTAT DU ROBOT ====================

enum RobotState {
  STATE_IDLE,
  STATE_CALIBRATING,
  STATE_SCANNING_360,
  STATE_PROCESSING_SCAN,
  STATE_MOVING,
  STATE_AVOIDING,
  STATE_FINISHED
};

RobotState currentState = STATE_IDLE;
int explorationStep = 0;
int currentScanIndex = 0;
float targetAngle = 0;
bool collisionDetected = false;

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  Serial.println("mBot Ranger Mapper v2");
  Serial.println("=====================");
  Serial.println("Avec Gyroscope et Scan 360°");

  // Initialisation buzzer
  buzzer.setpin(BUZZER_PIN);

  // Initialisation LEDs
  rgbLed.setpin(44);
  clearLeds();

  // Initialisation gyroscope
  gyro.begin();
  delay(100);

  // Initialisation encodeurs
  attachInterrupt(motorLeft.getIntNum(), interruptLeft, RISING);
  attachInterrupt(motorRight.getIntNum(), interruptRight, RISING);

  motorLeft.setPulse(9);
  motorRight.setPulse(9);
  motorLeft.setRatio(39.267);
  motorRight.setRatio(39.267);

  // Initialiser la carte
  initMap();

  // Animation de démarrage
  startupAnimation();

  Serial.println("\nCommandes:");
  Serial.println("  's' - Démarrer la cartographie");
  Serial.println("  't' - Test scan 360° (gyroscope)");
  Serial.println("  'g' - Debug gyroscope en live");
  Serial.println("  'c' - Calibrer gyroscope");
  Serial.println("  'p' - Afficher la carte");
  Serial.println("  'r' - Réinitialiser");
  Serial.println("  'q' - Arrêter");
}

// ==================== LOOP PRINCIPAL ====================

void loop() {
  // Mise à jour du gyroscope
  gyro.update();

  // Lire les commandes série
  if (Serial.available()) {
    char cmd = Serial.read();
    handleCommand(cmd);
  }

  // Machine à états
  switch (currentState) {
    case STATE_CALIBRATING:
      calibrateLoop();
      break;

    case STATE_SCANNING_360:
      scanning360Loop();
      break;

    case STATE_PROCESSING_SCAN:
      processScanData();
      break;

    case STATE_MOVING:
      moveLoop();
      break;

    case STATE_AVOIDING:
      avoidLoop();
      break;

    case STATE_FINISHED:
      finishedLoop();
      break;

    default:
      break;
  }

  // Vérifier les collisions en permanence
  checkCollision();

  // Mise à jour odométrie
  updateOdometry();
}

// ==================== COMMANDES ====================

void handleCommand(char cmd) {
  switch (cmd) {
    case 's':
    case 'S':
      startMapping();
      break;

    case 'p':
    case 'P':
      printMap();
      break;

    case 'r':
    case 'R':
      initMap();
      robotX = MAP_WIDTH * CELL_SIZE / 2;
      robotY = MAP_HEIGHT * CELL_SIZE / 2;
      explorationStep = 0;
      Serial.println("Carte réinitialisée");
      break;

    case 'c':
    case 'C':
      startCalibration();
      break;

    case 't':
    case 'T':
      testScan360();
      break;

    case 'g':
    case 'G':
      debugGyro();
      break;

    case 'q':
    case 'Q':
      stopMapping();
      break;
  }
}

// Debug: afficher les valeurs du gyroscope en live
void debugGyro() {
  Serial.println("\n=== DEBUG GYROSCOPE ===");
  Serial.println("Appuyer sur une touche pour arrêter...");
  Serial.println("AngleX | AngleY | AngleZ");
  Serial.println("------------------------");

  while (!Serial.available()) {
    gyro.update();

    float angleX = gyro.getAngleX();
    float angleY = gyro.getAngleY();
    float angleZ = gyro.getAngleZ();

    Serial.print(angleX, 2);
    Serial.print("\t| ");
    Serial.print(angleY, 2);
    Serial.print("\t| ");
    Serial.println(angleZ, 2);

    // LED indique le niveau d'inclinaison
    int tilt = abs((int)angleX) + abs((int)angleY);
    if (tilt < 5) {
      setLedColor(0, 255, 0);  // Vert = stable
    } else if (tilt < 15) {
      setLedColor(255, 255, 0);  // Jaune = légère inclinaison
    } else {
      setLedColor(255, 0, 0);  // Rouge = forte inclinaison
    }

    delay(100);
  }

  // Vider le buffer
  while (Serial.available()) Serial.read();

  Serial.println("=== FIN DEBUG ===\n");
  setLedColor(0, 50, 0);
}

// ==================== CALIBRATION GYROSCOPE ====================

void startCalibration() {
  Serial.println("\nCalibration du gyroscope...");
  Serial.println("Ne pas bouger le robot!");
  setLedColor(255, 255, 0);  // Jaune

  currentState = STATE_CALIBRATING;
}

void calibrateLoop() {
  static int calibCount = 0;
  static float angleSum = 0;

  gyro.update();

  if (calibCount < 50) {
    angleSum += gyro.getAngleZ();
    calibCount++;
    delay(20);
  } else {
    gyroAngleOffset = angleSum / 50.0;
    robotAngle = 0;

    Serial.print("Calibration terminée. Offset: ");
    Serial.println(gyroAngleOffset);

    calibCount = 0;
    angleSum = 0;
    currentState = STATE_IDLE;
    setLedColor(0, 255, 0);
    buzzer.tone(1000, 200);
  }
}

// ==================== CARTOGRAPHIE ====================

void startMapping() {
  Serial.println("\n*** DÉMARRAGE CARTOGRAPHIE ***");
  setLedColor(0, 0, 255);

  // Préchauffer les moteurs (évite reset batterie)
  warmupMotors();

  // Calibrer le gyroscope
  gyro.update();
  delay(100);
  gyroAngleOffset = gyro.getAngleZ();
  robotAngle = 0;
  explorationStep = 0;

  Serial.print("Offset gyroscope: ");
  Serial.println(gyroAngleOffset);

  // Marquer la position initiale
  markPosition(robotX, robotY, CELL_FREE);

  // Commencer par un scan 360°
  startScan360();

  buzzer.tone(800, 200);
}

void stopMapping() {
  Serial.println("\n*** ARRÊT ***");
  stopMotors();
  currentState = STATE_IDLE;
  setLedColor(0, 50, 0);
  printMap();
}

// ==================== SCAN 360° ====================

// Variables pour le scan gyroscopique
float scanStartAngle = 0;      // Angle initial du scan
float scanTargetAngle = 0;     // Angle cible actuel
bool scanRotating = false;     // En train de tourner?
unsigned long scanTimeout = 0; // Timeout de sécurité

void startScan360() {
  Serial.println("\nScan 360° en cours (contrôle gyroscope)...");
  setLedColor(0, 100, 255);

  currentScanIndex = 0;
  scanRotating = false;

  // Réinitialiser les mesures
  for (int i = 0; i < NUM_SCAN_POINTS; i++) {
    scanDistances[i] = 0;
    scanAngles[i] = 0;
  }

  // Calibrer l'angle de départ
  gyro.update();
  scanStartAngle = gyro.getAngleZ();
  scanTargetAngle = scanStartAngle;

  Serial.print("Angle de départ: ");
  Serial.println(scanStartAngle);

  // Première mesure à la position actuelle
  stopMotors();
  delay(300);

  currentState = STATE_SCANNING_360;
}

void scanning360Loop() {
  gyro.update();
  float currentGyroAngle = gyro.getAngleZ();

  if (scanRotating) {
    // En rotation vers l'angle cible
    float angleDiff = scanTargetAngle - currentGyroAngle;

    // Normaliser la différence d'angle
    while (angleDiff > 180) angleDiff -= 360;
    while (angleDiff < -180) angleDiff += 360;

    // Vérifier timeout de sécurité (3 secondes max par rotation)
    if (millis() > scanTimeout) {
      Serial.println("  (timeout rotation)");
      stopMotors();
      delay(200);
      scanRotating = false;
      return;
    }

    if (abs(angleDiff) > 3.0) {  // Tolérance de 3 degrés
      // Continuer à tourner
      spinLeft(SPEED_SCAN);

      // Feedback LED pendant rotation
      int progress = (currentScanIndex * 12) / NUM_SCAN_POINTS;
      for (int i = 1; i <= 12; i++) {
        if (i <= progress) {
          rgbLed.setColor(i, 0, 255, 0);
        } else {
          rgbLed.setColor(i, 0, 50, 100);
        }
      }
      rgbLed.show();
    } else {
      // Angle atteint
      stopMotors();
      delay(250);  // Stabiliser avant mesure
      scanRotating = false;
    }
  } else {
    // Prendre une mesure
    float distance = measureDistance();

    scanDistances[currentScanIndex] = distance;
    scanAngles[currentScanIndex] = radians(currentScanIndex * SCAN_STEP);

    // Afficher
    Serial.print("  ");
    Serial.print(currentScanIndex * SCAN_STEP);
    Serial.print("° (gyro:");
    Serial.print(currentGyroAngle, 1);
    Serial.print("°): ");
    if (distance > 0) {
      Serial.print(distance);
      Serial.println(" cm");
    } else {
      Serial.println("--");
    }

    // LED selon distance
    if (distance > 0 && distance < 30) {
      setLedColor(255, 0, 0);  // Rouge = proche
    } else if (distance > 0 && distance < 80) {
      setLedColor(255, 165, 0);  // Orange = moyen
    } else {
      setLedColor(0, 100, 255);  // Bleu = loin
    }

    currentScanIndex++;

    if (currentScanIndex >= NUM_SCAN_POINTS) {
      // Scan complet - revenir à l'angle de départ
      Serial.println("Scan 360° terminé!");
      stopMotors();

      // Tourner pour revenir à l'orientation initiale
      Serial.print("Retour à l'angle initial: ");
      Serial.println(scanStartAngle);
      rotateToAngle(scanStartAngle);

      buzzer.tone(800, 100);
      currentState = STATE_PROCESSING_SCAN;
    } else {
      // Préparer la rotation vers le prochain angle
      scanTargetAngle = scanStartAngle + (currentScanIndex * SCAN_STEP);
      // Normaliser
      while (scanTargetAngle > 180) scanTargetAngle -= 360;
      while (scanTargetAngle < -180) scanTargetAngle += 360;

      scanRotating = true;
      scanTimeout = millis() + 3000;  // 3 secondes max
    }
  }
}

// Rotation vers un angle absolu du gyroscope
void rotateToAngle(float targetAngle) {
  unsigned long timeout = millis() + 5000;  // 5 secondes max

  while (millis() < timeout) {
    gyro.update();
    float currentAngle = gyro.getAngleZ();
    float diff = targetAngle - currentAngle;

    while (diff > 180) diff -= 360;
    while (diff < -180) diff += 360;

    if (abs(diff) < 3.0) {
      stopMotors();
      return;
    }

    if (diff > 0) {
      spinLeft(SPEED_SCAN);
    } else {
      spinRight(SPEED_SCAN);
    }

    delay(20);
  }

  stopMotors();
  Serial.println("  (timeout retour angle)");
}

float measureDistance() {
  // Moyenne de 3 mesures pour plus de fiabilité
  float sum = 0;
  int validCount = 0;

  for (int i = 0; i < 3; i++) {
    float d = ultraSensor.distanceCm();
    if (d > MIN_DISTANCE && d < MAX_DISTANCE) {
      sum += d;
      validCount++;
    }
    delay(30);
  }

  if (validCount > 0) {
    return sum / validCount;
  }
  return 0;  // Pas de mesure valide
}

void testScan360() {
  Serial.println("\n=== TEST SCAN 360° (GYROSCOPE) ===");
  setLedColor(255, 0, 255);

  // Préchauffer les moteurs
  warmupMotors();

  // Calibrer le gyroscope
  gyro.update();
  float startAngle = gyro.getAngleZ();
  Serial.print("Angle de départ: ");
  Serial.println(startAngle);

  // Mesure initiale à 0°
  stopMotors();
  delay(300);
  float distance = measureDistance();
  Serial.print("0° (gyro: ");
  Serial.print(startAngle, 1);
  Serial.print("°): ");
  Serial.print(distance);
  Serial.println(" cm");

  // Tourner et mesurer tous les 30° en utilisant le gyroscope
  for (int angle = 30; angle < 360; angle += 30) {
    float targetAngle = startAngle + angle;
    // Normaliser
    while (targetAngle > 180) targetAngle -= 360;
    while (targetAngle < -180) targetAngle += 360;

    Serial.print("Rotation vers ");
    Serial.print(angle);
    Serial.print("° (cible gyro: ");
    Serial.print(targetAngle, 1);
    Serial.println("°)...");

    // Tourner vers l'angle cible avec le gyroscope
    setLedColor(0, 0, 255);
    rotateToAngle(targetAngle);
    delay(300);  // Stabiliser

    // Mesurer
    gyro.update();
    float currentAngle = gyro.getAngleZ();
    distance = measureDistance();

    // LED selon distance
    if (distance > 0 && distance < 30) {
      setLedColor(255, 0, 0);
    } else if (distance > 0 && distance < 80) {
      setLedColor(255, 165, 0);
    } else {
      setLedColor(0, 255, 0);
    }

    Serial.print(angle);
    Serial.print("° (gyro: ");
    Serial.print(currentAngle, 1);
    Serial.print("°): ");
    if (distance > 0) {
      Serial.print(distance);
      Serial.println(" cm");
    } else {
      Serial.println("-- (pas de détection)");
    }
  }

  // Retour à l'angle initial
  Serial.println("Retour à l'orientation initiale...");
  rotateToAngle(startAngle);

  stopMotors();
  setLedColor(0, 255, 0);
  buzzer.tone(1000, 200);
  Serial.println("=== FIN TEST ===\n");
}

// ==================== TRAITEMENT DU SCAN ====================

void processScanData() {
  Serial.println("Traitement des données de scan...");

  // Mettre à jour l'angle du robot depuis le gyroscope
  gyro.update();
  robotAngle = radians(gyro.getAngleZ() - gyroAngleOffset);

  // Traiter chaque mesure
  for (int i = 0; i < NUM_SCAN_POINTS; i++) {
    float distance = scanDistances[i];
    float angle = scanAngles[i] + robotAngle;

    if (distance > MIN_DISTANCE && distance < MAX_DISTANCE) {
      // Calculer la position de l'obstacle
      float obsX = robotX + distance * cos(angle);
      float obsY = robotY + distance * sin(angle);

      // Marquer le chemin libre
      markFreePath(robotX, robotY, obsX, obsY);

      // Marquer l'obstacle
      markPosition(obsX, obsY, CELL_OBSTACLE);
    } else if (distance <= 0 || distance >= MAX_DISTANCE) {
      // Pas d'obstacle détecté dans cette direction
      // Marquer comme libre sur une distance limitée
      float freeX = robotX + (MAX_DISTANCE * 0.8) * cos(angle);
      float freeY = robotY + (MAX_DISTANCE * 0.8) * sin(angle);
      markFreePath(robotX, robotY, freeX, freeY);
    }
  }

  explorationStep++;

  // Décider du prochain mouvement
  decideNextMove();
}

void decideNextMove() {
  // Trouver la direction avec le plus d'espace
  float bestDistance = 0;
  int bestIndex = 0;

  for (int i = 0; i < NUM_SCAN_POINTS; i++) {
    if (scanDistances[i] > bestDistance) {
      bestDistance = scanDistances[i];
      bestIndex = i;
    }
  }

  Serial.print("Meilleure direction: ");
  Serial.print(bestIndex * SCAN_STEP);
  Serial.print("° avec ");
  Serial.print(bestDistance);
  Serial.println(" cm");

  if (bestDistance < 30 || explorationStep >= 15) {
    // Pas assez d'espace ou exploration terminée
    currentState = STATE_FINISHED;
  } else {
    // Se tourner vers la meilleure direction et avancer
    targetAngle = radians(bestIndex * SCAN_STEP);
    currentState = STATE_MOVING;
    setLedColor(0, 255, 0);
  }
}

// ==================== MOUVEMENT ====================

void moveLoop() {
  static unsigned long moveStartTime = 0;
  static bool moving = false;
  static bool turning = true;
  static float moveTargetGyroAngle = 0;

  gyro.update();
  float currentGyroAngle = gyro.getAngleZ();

  if (turning) {
    // Calculer l'angle cible en degrés du gyroscope
    // targetAngle est relatif au robot (en radians), convertir en absolu gyro
    if (moveStartTime == 0) {
      moveTargetGyroAngle = currentGyroAngle + degrees(targetAngle);
      // Normaliser
      while (moveTargetGyroAngle > 180) moveTargetGyroAngle -= 360;
      while (moveTargetGyroAngle < -180) moveTargetGyroAngle += 360;
      moveStartTime = 1;  // Flag initialisé

      Serial.print("Rotation vers angle gyro: ");
      Serial.println(moveTargetGyroAngle);
    }

    float angleDiff = moveTargetGyroAngle - currentGyroAngle;
    while (angleDiff > 180) angleDiff -= 360;
    while (angleDiff < -180) angleDiff += 360;

    if (abs(angleDiff) > 5.0) {  // Tolérance 5 degrés
      if (angleDiff > 0) {
        spinLeft(SPEED_TURN);
      } else {
        spinRight(SPEED_TURN);
      }
    } else {
      stopMotors();
      delay(200);
      turning = false;
      moving = true;
      moveStartTime = millis();
      Serial.println("Angle atteint, avance...");
    }
  } else if (moving) {
    // Vérifier les obstacles pendant le mouvement
    float distance = ultraSensor.distanceCm();

    if (distance > 0 && distance < 25) {
      // Obstacle! Arrêter
      stopMotors();
      moving = false;
      turning = true;
      moveStartTime = 0;
      markRobotPosition();

      Serial.println("Obstacle détecté pendant le mouvement!");
      currentState = STATE_SCANNING_360;
      startScan360();
      return;
    }

    // Avancer
    moveForward(SPEED_MOVE);

    // Avancer pendant un temps limité (2 secondes)
    if (millis() - moveStartTime > 2000) {
      stopMotors();
      moving = false;
      turning = true;
      moveStartTime = 0;
      markRobotPosition();

      delay(300);

      // Nouveau scan
      startScan360();
    }
  }
}

void avoidLoop() {
  // Reculer puis tourner
  static int avoidStep = 0;

  switch (avoidStep) {
    case 0:
      moveBackward(SPEED_MOVE);
      delay(500);
      stopMotors();
      avoidStep = 1;
      break;

    case 1:
      spinRight(SPEED_TURN);
      delay(600);
      stopMotors();
      avoidStep = 0;
      collisionDetected = false;
      currentState = STATE_SCANNING_360;
      startScan360();
      break;
  }
}

// ==================== DÉTECTION DE COLLISION ====================

void checkCollision() {
  if (currentState == STATE_IDLE || currentState == STATE_FINISHED) {
    return;
  }

  gyro.update();

  // Lire l'accéléromètre pour détecter les chocs
  float accelX = gyro.getAngleX();  // Utilisation simplifiée
  float accelY = gyro.getAngleY();

  // Un changement brusque indique une collision
  static float lastAccelX = 0;
  static float lastAccelY = 0;

  float deltaX = abs(accelX - lastAccelX);
  float deltaY = abs(accelY - lastAccelY);

  if (deltaX > COLLISION_THRESHOLD || deltaY > COLLISION_THRESHOLD) {
    if (!collisionDetected) {
      collisionDetected = true;
      Serial.println("!!! COLLISION DÉTECTÉE !!!");
      stopMotors();
      buzzer.tone(2000, 100);
      setLedColor(255, 0, 0);

      currentState = STATE_AVOIDING;
    }
  }

  lastAccelX = accelX;
  lastAccelY = accelY;
}

void finishedLoop() {
  static bool done = false;

  if (!done) {
    Serial.println("\n*** EXPLORATION TERMINÉE ***");
    stopMotors();
    printMap();

    for (int i = 0; i < 3; i++) {
      setLedColor(0, 255, 0);
      buzzer.tone(1000 + i * 200, 150);
      delay(200);
      setLedColor(0, 0, 0);
      delay(100);
    }

    done = true;
    currentState = STATE_IDLE;
  }
}

// ==================== ODOMÉTRIE ====================

void updateOdometry() {
  // Calculer le déplacement
  long deltaLeft = encoderLeft - lastEncoderLeft;
  long deltaRight = encoderRight - lastEncoderRight;

  lastEncoderLeft = encoderLeft;
  lastEncoderRight = encoderRight;

  float distLeft = deltaLeft * CM_PER_PULSE;
  float distRight = deltaRight * CM_PER_PULSE;
  float distCenter = (distLeft + distRight) / 2.0;

  // Utiliser le gyroscope pour l'angle (plus précis)
  gyro.update();
  robotAngle = radians(gyro.getAngleZ() - gyroAngleOffset);

  // Mettre à jour la position
  robotX += distCenter * cos(robotAngle);
  robotY += distCenter * sin(robotAngle);
}

void markRobotPosition() {
  markPosition(robotX, robotY, CELL_FREE);
}

// ==================== GESTION DE LA CARTE ====================

void initMap() {
  for (int y = 0; y < MAP_HEIGHT; y++) {
    for (int x = 0; x < MAP_WIDTH; x++) {
      gridMap[y][x] = CELL_UNKNOWN;
    }
  }
}

void markPosition(float x, float y, uint8_t type) {
  int cellX = (int)(x / CELL_SIZE);
  int cellY = (int)(y / CELL_SIZE);

  if (cellX >= 0 && cellX < MAP_WIDTH && cellY >= 0 && cellY < MAP_HEIGHT) {
    // Ne pas écraser un obstacle par du libre
    if (type == CELL_FREE && gridMap[cellY][cellX] == CELL_OBSTACLE) {
      return;
    }
    gridMap[cellY][cellX] = type;
  }
}

void markFreePath(float x1, float y1, float x2, float y2) {
  int cellX1 = (int)(x1 / CELL_SIZE);
  int cellY1 = (int)(y1 / CELL_SIZE);
  int cellX2 = (int)(x2 / CELL_SIZE);
  int cellY2 = (int)(y2 / CELL_SIZE);

  int dx = abs(cellX2 - cellX1);
  int dy = abs(cellY2 - cellY1);
  int sx = (cellX1 < cellX2) ? 1 : -1;
  int sy = (cellY1 < cellY2) ? 1 : -1;
  int err = dx - dy;

  while (true) {
    if (cellX1 >= 0 && cellX1 < MAP_WIDTH && cellY1 >= 0 && cellY1 < MAP_HEIGHT) {
      if (gridMap[cellY1][cellX1] == CELL_UNKNOWN) {
        gridMap[cellY1][cellX1] = CELL_FREE;
      }
    }

    if (cellX1 == cellX2 && cellY1 == cellY2) break;

    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      cellX1 += sx;
    }
    if (e2 < dx) {
      err += dx;
      cellY1 += sy;
    }
  }
}

void printMap() {
  Serial.println("\n========== CARTE ==========");
  Serial.print("Position: (");
  Serial.print(robotX, 1);
  Serial.print(", ");
  Serial.print(robotY, 1);
  Serial.print(") angle: ");
  Serial.print(degrees(robotAngle), 1);
  Serial.println("°");
  Serial.print("Étapes d'exploration: ");
  Serial.println(explorationStep);
  Serial.println();

  int robotCellX = (int)(robotX / CELL_SIZE);
  int robotCellY = (int)(robotY / CELL_SIZE);

  // Compter les cellules
  int unknown = 0, free = 0, obstacle = 0;

  for (int y = MAP_HEIGHT - 1; y >= 0; y--) {
    for (int x = 0; x < MAP_WIDTH; x++) {
      if (x == robotCellX && y == robotCellY) {
        // Robot
        if (robotAngle > -PI/4 && robotAngle <= PI/4) {
          Serial.print(">");
        } else if (robotAngle > PI/4 && robotAngle <= 3*PI/4) {
          Serial.print("^");
        } else if (robotAngle > -3*PI/4 && robotAngle <= -PI/4) {
          Serial.print("v");
        } else {
          Serial.print("<");
        }
      } else {
        switch (gridMap[y][x]) {
          case CELL_UNKNOWN:
            Serial.print(".");
            unknown++;
            break;
          case CELL_FREE:
            Serial.print(" ");
            free++;
            break;
          case CELL_OBSTACLE:
            Serial.print("#");
            obstacle++;
            break;
        }
      }
    }
    Serial.println();
  }

  Serial.println("===========================");
  Serial.print("Cellules: ");
  Serial.print(free);
  Serial.print(" libres, ");
  Serial.print(obstacle);
  Serial.print(" obstacles, ");
  Serial.print(unknown);
  Serial.println(" inconnues");
  Serial.println("Légende: . = inconnu, espace = libre, # = obstacle");
  Serial.println();
}

// ==================== CONTRÔLE DES MOTEURS ====================

// Préchauffage progressif des moteurs (évite reset sur batterie)
void warmupMotors() {
  Serial.println("Préchauffage des moteurs...");
  for (int speed = 40; speed <= SPEED_SCAN; speed += 20) {
    motorLeft.setMotorPwm(-speed);
    motorRight.setMotorPwm(speed);
    delay(80);
  }
  stopMotors();
  delay(200);
  Serial.println("Moteurs prêts.");
}

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

// ==================== LEDs ====================

void clearLeds() {
  rgbLed.setColor(0, 0, 0, 0);
  rgbLed.show();
}

void setLedColor(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setColor(0, r, g, b);
  rgbLed.show();
}

void startupAnimation() {
  for (int i = 1; i <= 12; i++) {
    rgbLed.setColor(i, 0, 100, 255);
    rgbLed.show();
    delay(50);
  }
  delay(300);
  clearLeds();
  setLedColor(0, 50, 0);
}

// ==================== INTERRUPTIONS ====================

void interruptLeft() {
  if (digitalRead(motorLeft.getPortB()) == 0) {
    encoderLeft--;
  } else {
    encoderLeft++;
  }
}

void interruptRight() {
  if (digitalRead(motorRight.getPortB()) == 0) {
    encoderRight--;
  } else {
    encoderRight++;
  }
}
