/**
 * mBot Ranger Zone Explorer
 *
 * Ce programme fait explorer une zone au robot en deux phases:
 * 1. EXPLORATION: Le robot explore lentement en détectant les obstacles
 *    et mémorise tous ses mouvements
 * 2. REPLAY: Le robot reparcourt la zone en accéléré en utilisant
 *    les mouvements mémorisés
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

// ==================== CONFIGURATION MATÉRIELLE ====================

// Moteurs encodeurs
MeEncoderOnBoard motorLeft(SLOT1);
MeEncoderOnBoard motorRight(SLOT2);

// Capteur ultrasonique (port 10 par défaut sur Auriga)
MeUltrasonicSensor ultraSensor(PORT_10);

// LEDs RGB intégrées (pin 44 sur Auriga)
MeRGBLed rgbLed(0, 12);

// Buzzer (pin 45 sur Auriga)
MeBuzzer buzzer;
#define BUZZER_PIN 45

// ==================== CONFIGURATION DES PARAMÈTRES ====================

// Vitesses (0-255)
const int SPEED_EXPLORE = 80;      // Vitesse lente d'exploration
const int SPEED_REPLAY = 200;      // Vitesse rapide de replay
const float SPEED_RATIO = (float)SPEED_REPLAY / SPEED_EXPLORE;

// Distances (en cm)
const int OBSTACLE_DISTANCE = 25;  // Distance de détection d'obstacle
const int DANGER_DISTANCE = 10;    // Distance de danger immédiat

// Durées (en ms)
const int EXPLORE_DURATION = 60000;    // Durée totale d'exploration (60 secondes)
const int MOVEMENT_CHECK_INTERVAL = 50; // Intervalle de vérification

// ==================== STRUCTURE DE MÉMORISATION ====================

// Types de mouvements
enum MoveType {
  MOVE_FORWARD,
  MOVE_BACKWARD,
  MOVE_TURN_LEFT,
  MOVE_TURN_RIGHT,
  MOVE_STOP
};

// Structure pour stocker un mouvement
struct Movement {
  MoveType type;
  unsigned long duration;  // Durée en millisecondes
};

// Tableau de mémorisation des mouvements
const int MAX_MOVEMENTS = 500;
Movement movementHistory[MAX_MOVEMENTS];
int movementCount = 0;

// ==================== VARIABLES D'ÉTAT ====================

enum RobotState {
  STATE_IDLE,
  STATE_EXPLORING,
  STATE_REPLAY,
  STATE_FINISHED
};

RobotState currentState = STATE_IDLE;
unsigned long exploreStartTime = 0;
unsigned long currentMoveStartTime = 0;
MoveType currentMoveType = MOVE_STOP;

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  Serial.println("mBot Ranger Zone Explorer");
  Serial.println("=========================");

  // Initialisation du buzzer sur pin 45
  buzzer.setpin(BUZZER_PIN);

  // Initialisation des LEDs RGB sur pin 44
  rgbLed.setpin(44);
  clearLeds();

  // Initialisation des interruptions pour les encodeurs
  attachInterrupt(motorLeft.getIntNum(), interruptLeft, RISING);
  attachInterrupt(motorRight.getIntNum(), interruptRight, RISING);

  // Configuration des moteurs
  motorLeft.setPulse(9);
  motorRight.setPulse(9);
  motorLeft.setRatio(39.267);
  motorRight.setRatio(39.267);
  motorLeft.setPosPid(1.8, 0, 1.2);
  motorRight.setPosPid(1.8, 0, 1.2);
  motorLeft.setSpeedPid(0.18, 0, 0);
  motorRight.setSpeedPid(0.18, 0, 0);

  // Configuration du Timer pour la mise à jour des moteurs
  setupMotorTimer();

  // Animation de démarrage
  startupAnimation();

  // Attendre 3 secondes avant de commencer
  Serial.println("Démarrage dans 3 secondes...");
  countdownAnimation(3);

  // Démarrer l'exploration
  startExploration();
}

// Timer interrupt pour mise à jour continue des moteurs
void setupMotorTimer() {
  // Timer1 pour appeler motorLoop à 200Hz
  cli();  // Désactiver les interruptions
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  OCR1A = 1249;  // 16MHz / (64 * 200Hz) - 1
  TCCR1B |= (1 << WGM12);  // Mode CTC
  TCCR1B |= (1 << CS11) | (1 << CS10);  // Prescaler 64
  TIMSK1 |= (1 << OCIE1A);  // Activer interruption compare
  sei();  // Réactiver les interruptions
}

// Interruption Timer1 pour mise à jour des moteurs
ISR(TIMER1_COMPA_vect) {
  motorLeft.loop();
  motorRight.loop();
}

// ==================== LOOP PRINCIPAL ====================

void loop() {
  switch (currentState) {
    case STATE_EXPLORING:
      exploreLoop();
      break;

    case STATE_REPLAY:
      replayLoop();
      break;

    case STATE_FINISHED:
      finishedLoop();
      break;

    default:
      break;
  }

  // Mise à jour des moteurs
  motorLeft.loop();
  motorRight.loop();
}

// ==================== PHASE D'EXPLORATION ====================

void startExploration() {
  Serial.println("\n*** PHASE D'EXPLORATION ***");
  Serial.println("Exploration lente avec détection d'obstacles...\n");

  currentState = STATE_EXPLORING;
  exploreStartTime = millis();
  movementCount = 0;

  // LED bleue pour l'exploration
  setAllLeds(0, 0, 255);

  // Commencer à avancer
  startMove(MOVE_FORWARD);
}

void exploreLoop() {
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - exploreStartTime;

  // Vérifier si l'exploration est terminée
  if (elapsedTime >= EXPLORE_DURATION) {
    endCurrentMove();
    stopMotors();
    startReplay();
    return;
  }

  // Afficher la progression
  static unsigned long lastProgressUpdate = 0;
  if (currentTime - lastProgressUpdate >= 5000) {
    int progress = (elapsedTime * 100) / EXPLORE_DURATION;
    Serial.print("Exploration: ");
    Serial.print(progress);
    Serial.print("% - Mouvements mémorisés: ");
    Serial.println(movementCount);
    lastProgressUpdate = currentTime;
  }

  // Lire la distance
  float distance = ultraSensor.distanceCm();

  // Gérer les obstacles
  if (distance > 0 && distance < DANGER_DISTANCE) {
    // Danger! Reculer immédiatement
    handleDanger();
  } else if (distance > 0 && distance < OBSTACLE_DISTANCE) {
    // Obstacle détecté, tourner
    handleObstacle();
  } else {
    // Voie libre, continuer à avancer
    if (currentMoveType != MOVE_FORWARD) {
      endCurrentMove();
      startMove(MOVE_FORWARD);
    }
  }

  delay(MOVEMENT_CHECK_INTERVAL);
}

void handleDanger() {
  Serial.println("! DANGER - Obstacle très proche !");

  // Flash rouge
  flashLeds(255, 0, 0, 2);

  // Sauvegarder le mouvement en cours
  endCurrentMove();

  // Reculer
  startMove(MOVE_BACKWARD);
  delay(400);
  endCurrentMove();

  // Tourner (alterner gauche/droite)
  static bool turnLeft = true;
  if (turnLeft) {
    startMove(MOVE_TURN_LEFT);
  } else {
    startMove(MOVE_TURN_RIGHT);
  }
  delay(600);
  endCurrentMove();
  turnLeft = !turnLeft;

  // LED bleue
  setAllLeds(0, 0, 255);
}

void handleObstacle() {
  Serial.println("Obstacle détecté - Changement de direction");

  // LED orange pendant la manœuvre
  setAllLeds(255, 165, 0);

  // Sauvegarder le mouvement en cours
  endCurrentMove();

  // Choisir une direction de rotation basée sur un scan
  MoveType turnDirection = chooseTurnDirection();

  // Tourner
  startMove(turnDirection);
  delay(random(300, 700));  // Rotation variable pour plus de couverture
  endCurrentMove();

  // LED bleue
  setAllLeds(0, 0, 255);
}

MoveType chooseTurnDirection() {
  // Scanner gauche et droite pour choisir la meilleure direction

  stopMotors();
  delay(100);

  // Scanner à gauche
  spinLeft(SPEED_EXPLORE);
  delay(200);
  stopMotors();
  delay(100);
  float distanceLeft = ultraSensor.distanceCm();

  // Revenir au centre
  spinRight(SPEED_EXPLORE);
  delay(200);
  stopMotors();
  delay(100);

  // Scanner à droite
  spinRight(SPEED_EXPLORE);
  delay(200);
  stopMotors();
  delay(100);
  float distanceRight = ultraSensor.distanceCm();

  // Revenir au centre
  spinLeft(SPEED_EXPLORE);
  delay(200);
  stopMotors();

  Serial.print("Distance gauche: ");
  Serial.print(distanceLeft);
  Serial.print(" cm, Distance droite: ");
  Serial.print(distanceRight);
  Serial.println(" cm");

  // Choisir la direction avec le plus d'espace
  if (distanceLeft > distanceRight) {
    return MOVE_TURN_LEFT;
  } else {
    return MOVE_TURN_RIGHT;
  }
}

// ==================== PHASE DE REPLAY ====================

void startReplay() {
  Serial.println("\n*** PHASE DE REPLAY ***");
  Serial.print("Replay de ");
  Serial.print(movementCount);
  Serial.println(" mouvements en accéléré!\n");

  // Jouer une mélodie de transition
  playTransitionMelody();

  currentState = STATE_REPLAY;

  // LED verte pour le replay
  setAllLeds(0, 255, 0);

  delay(1000);
}

void replayLoop() {
  static int currentReplayIndex = 0;

  if (currentReplayIndex >= movementCount) {
    // Replay terminé
    stopMotors();
    currentState = STATE_FINISHED;
    currentReplayIndex = 0;
    return;
  }

  // Récupérer le mouvement à rejouer
  Movement move = movementHistory[currentReplayIndex];

  // Calculer la durée réduite (proportionnelle au ratio de vitesse)
  unsigned long replayDuration = move.duration / SPEED_RATIO;
  if (replayDuration < 50) replayDuration = 50;  // Minimum 50ms

  // Afficher le mouvement
  Serial.print("Replay mouvement ");
  Serial.print(currentReplayIndex + 1);
  Serial.print("/");
  Serial.print(movementCount);
  Serial.print(": ");
  printMoveType(move.type);
  Serial.print(" (");
  Serial.print(replayDuration);
  Serial.println("ms)");

  // Effet LED selon le mouvement
  switch (move.type) {
    case MOVE_FORWARD:
      setAllLeds(0, 255, 0);  // Vert
      break;
    case MOVE_BACKWARD:
      setAllLeds(255, 255, 0);  // Jaune
      break;
    case MOVE_TURN_LEFT:
      setLeftLeds(0, 255, 255);  // Cyan à gauche
      break;
    case MOVE_TURN_RIGHT:
      setRightLeds(255, 0, 255);  // Magenta à droite
      break;
    default:
      break;
  }

  // Exécuter le mouvement à vitesse rapide
  executeMove(move.type, SPEED_REPLAY);
  delay(replayDuration);

  currentReplayIndex++;
}

void finishedLoop() {
  static bool animationDone = false;

  if (!animationDone) {
    Serial.println("\n*** EXPLORATION TERMINÉE ***");
    Serial.println("Le robot a parcouru toute la zone mémorisée!");

    stopMotors();

    // Animation de fin
    finishAnimation();

    animationDone = true;
  }

  // Attendre et recommencer
  delay(5000);
  animationDone = false;

  // Recommencer l'exploration
  startExploration();
}

// ==================== GESTION DES MOUVEMENTS ====================

void startMove(MoveType type) {
  currentMoveType = type;
  currentMoveStartTime = millis();
  executeMove(type, SPEED_EXPLORE);
}

void endCurrentMove() {
  if (currentMoveType != MOVE_STOP && movementCount < MAX_MOVEMENTS) {
    unsigned long duration = millis() - currentMoveStartTime;

    if (duration > 50) {  // Ignorer les mouvements trop courts
      movementHistory[movementCount].type = currentMoveType;
      movementHistory[movementCount].duration = duration;
      movementCount++;
    }
  }

  currentMoveType = MOVE_STOP;
  stopMotors();
}

void executeMove(MoveType type, int speed) {
  switch (type) {
    case MOVE_FORWARD:
      moveForward(speed);
      break;
    case MOVE_BACKWARD:
      moveBackward(speed);
      break;
    case MOVE_TURN_LEFT:
      spinLeft(speed);
      break;
    case MOVE_TURN_RIGHT:
      spinRight(speed);
      break;
    case MOVE_STOP:
    default:
      stopMotors();
      break;
  }
}

void printMoveType(MoveType type) {
  switch (type) {
    case MOVE_FORWARD:
      Serial.print("AVANCER");
      break;
    case MOVE_BACKWARD:
      Serial.print("RECULER");
      break;
    case MOVE_TURN_LEFT:
      Serial.print("TOURNER GAUCHE");
      break;
    case MOVE_TURN_RIGHT:
      Serial.print("TOURNER DROITE");
      break;
    case MOVE_STOP:
      Serial.print("STOP");
      break;
  }
}

// ==================== CONTRÔLE DES MOTEURS ====================

void moveForward(int speed) {
  motorLeft.setTarPWM(speed);
  motorRight.setTarPWM(-speed);
}

void moveBackward(int speed) {
  motorLeft.setTarPWM(-speed);
  motorRight.setTarPWM(speed);
}

void spinLeft(int speed) {
  motorLeft.setTarPWM(-speed);
  motorRight.setTarPWM(-speed);
}

void spinRight(int speed) {
  motorLeft.setTarPWM(speed);
  motorRight.setTarPWM(speed);
}

void stopMotors() {
  motorLeft.setTarPWM(0);
  motorRight.setTarPWM(0);
}

// ==================== CONTRÔLE DES LEDs ====================

void clearLeds() {
  rgbLed.setColor(0, 0, 0, 0);
  rgbLed.show();
}

void setAllLeds(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setColor(0, r, g, b);
  rgbLed.show();
}

void setLeftLeds(uint8_t r, uint8_t g, uint8_t b) {
  // LEDs 1-6 sont à gauche
  for (int i = 1; i <= 6; i++) {
    rgbLed.setColor(i, r, g, b);
  }
  for (int i = 7; i <= 12; i++) {
    rgbLed.setColor(i, 0, 0, 0);
  }
  rgbLed.show();
}

void setRightLeds(uint8_t r, uint8_t g, uint8_t b) {
  // LEDs 7-12 sont à droite
  for (int i = 1; i <= 6; i++) {
    rgbLed.setColor(i, 0, 0, 0);
  }
  for (int i = 7; i <= 12; i++) {
    rgbLed.setColor(i, r, g, b);
  }
  rgbLed.show();
}

void flashLeds(uint8_t r, uint8_t g, uint8_t b, int times) {
  for (int i = 0; i < times; i++) {
    setAllLeds(r, g, b);
    delay(100);
    clearLeds();
    delay(100);
  }
}

// ==================== ANIMATIONS ====================

void startupAnimation() {
  Serial.println("Animation de démarrage...");

  // Animation circulaire des LEDs
  for (int i = 1; i <= 12; i++) {
    rgbLed.setColor(i, 0, 100, 255);
    rgbLed.show();
    buzzer.tone(200 + i * 50, 50);
    delay(80);
    rgbLed.setColor(i, 0, 20, 50);
    rgbLed.show();
  }

  // Flash final
  setAllLeds(255, 255, 255);
  delay(200);
  clearLeds();
}

void countdownAnimation(int seconds) {
  for (int i = seconds; i > 0; i--) {
    Serial.print(i);
    Serial.println("...");

    // Afficher le nombre avec les LEDs
    int ledsToLight = i * 4;
    for (int led = 1; led <= 12; led++) {
      if (led <= ledsToLight) {
        rgbLed.setColor(led, 255, 255, 0);
      } else {
        rgbLed.setColor(led, 0, 0, 0);
      }
    }
    rgbLed.show();

    buzzer.tone(800, 200);
    delay(1000);
  }

  // Go!
  Serial.println("GO!");
  setAllLeds(0, 255, 0);
  buzzer.tone(1200, 500);
  delay(500);
}

void playTransitionMelody() {
  int notes[] = {523, 659, 784, 1047};  // C5, E5, G5, C6
  for (int i = 0; i < 4; i++) {
    buzzer.tone(notes[i], 150);
    delay(200);
  }
}

void finishAnimation() {
  Serial.println("Animation de fin...");

  // Arc-en-ciel rotatif
  for (int cycle = 0; cycle < 3; cycle++) {
    for (int hue = 0; hue < 256; hue += 8) {
      for (int led = 1; led <= 12; led++) {
        int ledHue = (hue + led * 20) % 256;
        uint8_t r, g, b;
        hueToRgb(ledHue, &r, &g, &b);
        rgbLed.setColor(led, r, g, b);
      }
      rgbLed.show();
      delay(20);
    }
  }

  // Flash de victoire
  for (int i = 0; i < 5; i++) {
    setAllLeds(0, 255, 0);
    buzzer.tone(1000 + i * 100, 100);
    delay(150);
    clearLeds();
    delay(100);
  }
}

void hueToRgb(int hue, uint8_t* r, uint8_t* g, uint8_t* b) {
  int section = hue / 43;
  int remainder = (hue - (section * 43)) * 6;

  switch (section) {
    case 0:
      *r = 255; *g = remainder; *b = 0;
      break;
    case 1:
      *r = 255 - remainder; *g = 255; *b = 0;
      break;
    case 2:
      *r = 0; *g = 255; *b = remainder;
      break;
    case 3:
      *r = 0; *g = 255 - remainder; *b = 255;
      break;
    case 4:
      *r = remainder; *g = 0; *b = 255;
      break;
    default:
      *r = 255; *g = 0; *b = 255 - remainder;
      break;
  }
}

// ==================== INTERRUPTIONS ENCODEURS ====================

void interruptLeft() {
  if (digitalRead(motorLeft.getPortB()) == 0) {
    motorLeft.pulsePosMinus();
  } else {
    motorLeft.pulsePosPlus();
  }
}

void interruptRight() {
  if (digitalRead(motorRight.getPortB()) == 0) {
    motorRight.pulsePosMinus();
  } else {
    motorRight.pulsePosPlus();
  }
}
