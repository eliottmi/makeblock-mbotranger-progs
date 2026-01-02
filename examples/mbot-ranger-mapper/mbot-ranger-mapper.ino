/**
 * mBot Ranger Mapper (Cartographe)
 *
 * Ce programme fait cartographier une zone au robot:
 * - Utilise l'odométrie (encodeurs) pour estimer sa position
 * - Scanne avec le capteur ultrasonique pour détecter les obstacles
 * - Construit une carte 2D de l'environnement
 * - Affiche la carte sur le moniteur série
 *
 * Matériel: Makeblock mBot Ranger (carte Me Auriga)
 *           Capteur ultrasonique sur port 10
 *
 * Bibliothèque requise: MakeblockDrive
 *
 * @author Makeblock Community
 * @license MIT
 */

#include <MeAuriga.h>
#include <math.h>

// ==================== CONFIGURATION MATÉRIELLE ====================

// Moteurs encodeurs
MeEncoderOnBoard motorLeft(SLOT1);
MeEncoderOnBoard motorRight(SLOT2);

// Capteur ultrasonique
MeUltrasonicSensor ultraSensor(PORT_10);

// LEDs RGB
MeRGBLed rgbLed(0, 12);

// Buzzer
MeBuzzer buzzer;
#define BUZZER_PIN 45

// ==================== PARAMÈTRES DU ROBOT ====================

// Dimensions du robot (en cm)
const float WHEEL_DIAMETER = 6.4;      // Diamètre des roues
const float WHEEL_BASE = 13.0;         // Distance entre les roues
const float PULSES_PER_REV = 9.0;      // Impulsions par tour encodeur
const float GEAR_RATIO = 39.267;       // Rapport de réduction
const float CM_PER_PULSE = (PI * WHEEL_DIAMETER) / (PULSES_PER_REV * GEAR_RATIO);

// Vitesses
const int SPEED_MOVE = 120;
const int SPEED_TURN = 100;

// ==================== PARAMÈTRES DE LA CARTE ====================

// Taille de la grille (en cellules)
const int MAP_WIDTH = 30;
const int MAP_HEIGHT = 30;

// Taille d'une cellule (en cm)
const float CELL_SIZE = 10.0;

// Types de cellules
const uint8_t CELL_UNKNOWN = 0;
const uint8_t CELL_FREE = 1;
const uint8_t CELL_OBSTACLE = 2;
const uint8_t CELL_ROBOT = 3;

// La carte
uint8_t gridMap[MAP_HEIGHT][MAP_WIDTH];

// ==================== ODOMÉTRIE ====================

// Position et orientation du robot (en cm et radians)
float robotX = MAP_WIDTH * CELL_SIZE / 2;   // Centre de la carte
float robotY = MAP_HEIGHT * CELL_SIZE / 2;
float robotAngle = 0;  // 0 = vers la droite, PI/2 = vers le haut

// Compteurs d'encodeurs
volatile long encoderLeft = 0;
volatile long encoderRight = 0;
long lastEncoderLeft = 0;
long lastEncoderRight = 0;

// ==================== ÉTAT DU ROBOT ====================

enum RobotState {
  STATE_IDLE,
  STATE_SCANNING,
  STATE_MOVING,
  STATE_TURNING,
  STATE_FINISHED
};

RobotState currentState = STATE_IDLE;
int scanAngleIndex = 0;
int explorationStep = 0;

// Angles de scan (en degrés, relatifs à l'avant du robot)
const int SCAN_ANGLES[] = {0, 45, 90, 135, 180, -135, -90, -45};
const int NUM_SCAN_ANGLES = 8;

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  Serial.println("mBot Ranger Mapper");
  Serial.println("==================");

  // Initialisation du buzzer
  buzzer.setpin(BUZZER_PIN);

  // Initialisation des LEDs
  rgbLed.setpin(44);
  clearLeds();

  // Initialisation des encodeurs
  attachInterrupt(motorLeft.getIntNum(), interruptLeft, RISING);
  attachInterrupt(motorRight.getIntNum(), interruptRight, RISING);

  // Configuration des moteurs
  motorLeft.setPulse(9);
  motorRight.setPulse(9);
  motorLeft.setRatio(39.267);
  motorRight.setRatio(39.267);

  // Initialiser la carte
  initMap();

  // Animation de démarrage
  startupAnimation();

  Serial.println("\nCommandes disponibles:");
  Serial.println("  's' - Démarrer la cartographie");
  Serial.println("  'p' - Afficher la carte");
  Serial.println("  'r' - Réinitialiser la carte");
  Serial.println("  'q' - Arrêter");

  delay(1000);
}

// ==================== LOOP PRINCIPAL ====================

void loop() {
  // Lire les commandes série
  if (Serial.available()) {
    char cmd = Serial.read();
    handleCommand(cmd);
  }

  // Machine à états
  switch (currentState) {
    case STATE_SCANNING:
      scanLoop();
      break;

    case STATE_MOVING:
      moveLoop();
      break;

    case STATE_TURNING:
      turnLoop();
      break;

    case STATE_FINISHED:
      finishedLoop();
      break;

    default:
      break;
  }

  // Mise à jour de l'odométrie
  updateOdometry();
}

// ==================== GESTION DES COMMANDES ====================

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
      Serial.println("Carte réinitialisée");
      break;

    case 'q':
    case 'Q':
      stopMapping();
      break;
  }
}

// ==================== CARTOGRAPHIE ====================

void startMapping() {
  Serial.println("\n*** DÉMARRAGE CARTOGRAPHIE ***\n");
  setLedColor(0, 0, 255);  // Bleu

  // Réinitialiser la position au centre
  robotX = MAP_WIDTH * CELL_SIZE / 2;
  robotY = MAP_HEIGHT * CELL_SIZE / 2;
  robotAngle = 0;
  explorationStep = 0;

  // Marquer la position initiale
  markRobotPosition();

  // Commencer par un scan
  scanAngleIndex = 0;
  currentState = STATE_SCANNING;

  buzzer.tone(1000, 200);
}

void stopMapping() {
  Serial.println("\n*** ARRÊT CARTOGRAPHIE ***\n");
  stopMotors();
  currentState = STATE_IDLE;
  setLedColor(255, 0, 0);  // Rouge

  printMap();
  buzzer.tone(500, 200);
}

// ==================== SCAN ====================

void scanLoop() {
  static unsigned long lastScanTime = 0;
  static bool turning = false;
  static float targetAngle = 0;

  if (!turning) {
    // Effectuer une mesure
    float distance = ultraSensor.distanceCm();

    if (distance > 0 && distance < 300) {
      // Calculer la position de l'obstacle
      float scanAngleRad = robotAngle + radians(SCAN_ANGLES[scanAngleIndex]);
      float obsX = robotX + distance * cos(scanAngleRad);
      float obsY = robotY + distance * sin(scanAngleRad);

      // Marquer les cellules libres sur le trajet
      markFreePath(robotX, robotY, obsX, obsY);

      // Marquer l'obstacle
      if (distance < 200) {  // Obstacle détecté
        markObstacle(obsX, obsY);
      }

      Serial.print("Scan ");
      Serial.print(SCAN_ANGLES[scanAngleIndex]);
      Serial.print("°: ");
      Serial.print(distance);
      Serial.println(" cm");
    }

    // Passer à l'angle suivant
    scanAngleIndex++;

    if (scanAngleIndex >= NUM_SCAN_ANGLES) {
      // Scan complet, passer à l'exploration
      scanAngleIndex = 0;
      setLedColor(0, 255, 0);  // Vert

      // Décider du prochain mouvement
      decideNextMove();
    } else {
      // Tourner vers le prochain angle
      turning = true;
      targetAngle = robotAngle + radians(SCAN_ANGLES[scanAngleIndex] - SCAN_ANGLES[scanAngleIndex - 1]);
    }
  } else {
    // Tourner vers l'angle cible
    float angleDiff = targetAngle - robotAngle;

    // Normaliser l'angle
    while (angleDiff > PI) angleDiff -= 2 * PI;
    while (angleDiff < -PI) angleDiff += 2 * PI;

    if (abs(angleDiff) < 0.1) {
      stopMotors();
      turning = false;
      delay(200);
    } else if (angleDiff > 0) {
      spinLeft(SPEED_TURN);
    } else {
      spinRight(SPEED_TURN);
    }
  }
}

void decideNextMove() {
  // Stratégie simple: avancer si possible, sinon tourner
  float frontDistance = ultraSensor.distanceCm();

  if (frontDistance > 30) {
    // Voie libre, avancer
    Serial.println("Voie libre - Avancer");
    currentState = STATE_MOVING;
    setLedColor(0, 255, 0);
  } else {
    // Obstacle, tourner vers la direction la plus libre
    Serial.println("Obstacle - Tourner");
    currentState = STATE_TURNING;
    setLedColor(255, 255, 0);
  }

  explorationStep++;

  // Arrêter après un certain nombre d'étapes
  if (explorationStep >= 20) {
    currentState = STATE_FINISHED;
  }
}

// ==================== MOUVEMENT ====================

void moveLoop() {
  static unsigned long moveStartTime = 0;
  static bool moving = false;

  if (!moving) {
    moveStartTime = millis();
    moving = true;
    moveForward(SPEED_MOVE);
  }

  // Vérifier les obstacles pendant le mouvement
  float distance = ultraSensor.distanceCm();

  if (distance > 0 && distance < 20) {
    // Obstacle proche, arrêter
    stopMotors();
    moving = false;
    markRobotPosition();
    currentState = STATE_SCANNING;
    return;
  }

  // Avancer pendant un temps limité
  if (millis() - moveStartTime > 1000) {
    stopMotors();
    moving = false;
    markRobotPosition();
    delay(300);
    currentState = STATE_SCANNING;
  }
}

void turnLoop() {
  static unsigned long turnStartTime = 0;
  static bool turningState = false;
  static int turnDirection = 1;

  if (!turningState) {
    turnStartTime = millis();
    turningState = true;

    // Choisir la direction de rotation (alterner)
    turnDirection = (explorationStep % 2 == 0) ? 1 : -1;

    if (turnDirection > 0) {
      spinLeft(SPEED_TURN);
    } else {
      spinRight(SPEED_TURN);
    }
  }

  // Tourner pendant un temps limité (~90 degrés)
  if (millis() - turnStartTime > 800) {
    stopMotors();
    turningState = false;
    delay(300);
    currentState = STATE_SCANNING;
  }
}

void finishedLoop() {
  static bool done = false;

  if (!done) {
    Serial.println("\n*** CARTOGRAPHIE TERMINÉE ***\n");
    stopMotors();
    printMap();

    // Animation de fin
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
  // Calculer le déplacement depuis la dernière mise à jour
  long deltaLeft = encoderLeft - lastEncoderLeft;
  long deltaRight = encoderRight - lastEncoderRight;

  lastEncoderLeft = encoderLeft;
  lastEncoderRight = encoderRight;

  // Calculer la distance parcourue par chaque roue
  float distLeft = deltaLeft * CM_PER_PULSE;
  float distRight = deltaRight * CM_PER_PULSE;

  // Calculer le déplacement du robot
  float distCenter = (distLeft + distRight) / 2.0;
  float deltaAngle = (distRight - distLeft) / WHEEL_BASE;

  // Mettre à jour la position
  robotX += distCenter * cos(robotAngle + deltaAngle / 2.0);
  robotY += distCenter * sin(robotAngle + deltaAngle / 2.0);
  robotAngle += deltaAngle;

  // Normaliser l'angle
  while (robotAngle > PI) robotAngle -= 2 * PI;
  while (robotAngle < -PI) robotAngle += 2 * PI;
}

// ==================== GESTION DE LA CARTE ====================

void initMap() {
  for (int y = 0; y < MAP_HEIGHT; y++) {
    for (int x = 0; x < MAP_WIDTH; x++) {
      gridMap[y][x] = CELL_UNKNOWN;
    }
  }
}

void markRobotPosition() {
  int cellX = (int)(robotX / CELL_SIZE);
  int cellY = (int)(robotY / CELL_SIZE);

  if (cellX >= 0 && cellX < MAP_WIDTH && cellY >= 0 && cellY < MAP_HEIGHT) {
    gridMap[cellY][cellX] = CELL_FREE;
  }
}

void markObstacle(float x, float y) {
  int cellX = (int)(x / CELL_SIZE);
  int cellY = (int)(y / CELL_SIZE);

  if (cellX >= 0 && cellX < MAP_WIDTH && cellY >= 0 && cellY < MAP_HEIGHT) {
    gridMap[cellY][cellX] = CELL_OBSTACLE;
  }
}

void markFreePath(float x1, float y1, float x2, float y2) {
  // Algorithme de Bresenham pour tracer une ligne
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
  Serial.print("Position robot: (");
  Serial.print(robotX);
  Serial.print(", ");
  Serial.print(robotY);
  Serial.print(") angle: ");
  Serial.print(degrees(robotAngle));
  Serial.println("°");
  Serial.println();

  // Position du robot sur la carte
  int robotCellX = (int)(robotX / CELL_SIZE);
  int robotCellY = (int)(robotY / CELL_SIZE);

  // Afficher la carte (Y inversé pour affichage correct)
  for (int y = MAP_HEIGHT - 1; y >= 0; y--) {
    for (int x = 0; x < MAP_WIDTH; x++) {
      if (x == robotCellX && y == robotCellY) {
        // Afficher le robot avec sa direction
        if (robotAngle > -PI/4 && robotAngle <= PI/4) {
          Serial.print(">");  // Droite
        } else if (robotAngle > PI/4 && robotAngle <= 3*PI/4) {
          Serial.print("^");  // Haut
        } else if (robotAngle > -3*PI/4 && robotAngle <= -PI/4) {
          Serial.print("v");  // Bas
        } else {
          Serial.print("<");  // Gauche
        }
      } else {
        switch (gridMap[y][x]) {
          case CELL_UNKNOWN:
            Serial.print(".");
            break;
          case CELL_FREE:
            Serial.print(" ");
            break;
          case CELL_OBSTACLE:
            Serial.print("#");
            break;
        }
      }
    }
    Serial.println();
  }

  Serial.println("===========================");
  Serial.println("Légende: . = inconnu, espace = libre, # = obstacle");
  Serial.println("         > ^ v < = robot (direction)");
  Serial.println();
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

void startupAnimation() {
  for (int i = 1; i <= 12; i++) {
    rgbLed.setColor(i, 0, 100, 255);
    rgbLed.show();
    delay(50);
  }
  delay(300);
  clearLeds();
}

// ==================== INTERRUPTIONS ENCODEURS ====================

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
