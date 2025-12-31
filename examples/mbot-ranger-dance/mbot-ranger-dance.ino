/**
 * mBot Ranger Dance
 *
 * Ce programme fait danser le mBot Ranger en combinant:
 * - Des mouvements rythmiques des moteurs
 * - Des animations LED colorées
 * - Des sons de buzzer
 *
 * Matériel: Makeblock mBot Ranger (carte Me Auriga)
 * Bibliothèque requise: MakeblockDrive
 *
 * @author Makeblock Community
 * @license MIT
 */

#include <MeAuriga.h>

// Configuration des moteurs encodeurs
MeEncoderOnBoard motorLeft(SLOT1);
MeEncoderOnBoard motorRight(SLOT2);

// Configuration des LEDs RGB intégrées (12 LEDs sur la carte Auriga)
MeRGBLed rgbLed(0, 12);

// Configuration du buzzer
MeBuzzer buzzer;

// Vitesse de base pour les mouvements (0-255)
const int SPEED_SLOW = 100;
const int SPEED_MEDIUM = 150;
const int SPEED_FAST = 200;

// Durées des mouvements en millisecondes
const int DURATION_SHORT = 200;
const int DURATION_MEDIUM = 400;
const int DURATION_LONG = 600;

// Notes musicales pour le buzzer
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523

void setup() {
  Serial.begin(115200);

  // Initialisation des LEDs RGB
  rgbLed.setpin(44);
  rgbLed.setColor(0, 0, 0, 0);
  rgbLed.show();

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

  // Animation de démarrage
  startupAnimation();

  delay(1000);
}

void loop() {
  // Séquence de danse complète
  danceSequence1();
  delay(500);

  danceSequence2();
  delay(500);

  danceSequence3();
  delay(500);

  // Pause entre les répétitions
  stopMotors();
  rainbowCycle(3);
  delay(1000);
}

// ==================== SÉQUENCES DE DANSE ====================

/**
 * Séquence 1: Le twist
 * Le robot tourne alternativement à gauche et à droite
 */
void danceSequence1() {
  playNote(NOTE_C4, 100);

  for (int i = 0; i < 4; i++) {
    // Tourner à gauche
    setLedColor(255, 0, 0);  // Rouge
    spinLeft(SPEED_MEDIUM);
    delay(DURATION_MEDIUM);
    playNote(NOTE_E4, 50);

    // Tourner à droite
    setLedColor(0, 0, 255);  // Bleu
    spinRight(SPEED_MEDIUM);
    delay(DURATION_MEDIUM);
    playNote(NOTE_G4, 50);
  }

  stopMotors();
}

/**
 * Séquence 2: Le moonwalk
 * Avancer et reculer avec des LEDs clignotantes
 */
void danceSequence2() {
  playNote(NOTE_A4, 100);

  for (int i = 0; i < 3; i++) {
    // Avancer
    setLedColor(0, 255, 0);  // Vert
    moveForward(SPEED_FAST);
    delay(DURATION_LONG);
    playNote(NOTE_C5, 50);

    // Pause avec flash
    stopMotors();
    flashLeds(255, 255, 0, 3);  // Jaune

    // Reculer
    setLedColor(255, 0, 255);  // Magenta
    moveBackward(SPEED_MEDIUM);
    delay(DURATION_LONG);
    playNote(NOTE_G4, 50);

    // Pause
    stopMotors();
    delay(DURATION_SHORT);
  }
}

/**
 * Séquence 3: Le shake
 * Mouvements rapides et saccadés
 */
void danceSequence3() {
  playNote(NOTE_F4, 100);

  for (int i = 0; i < 6; i++) {
    // Mouvement rapide à gauche
    setLedColor(0, 255, 255);  // Cyan
    spinLeft(SPEED_FAST);
    delay(DURATION_SHORT);
    playNote(NOTE_B4, 30);

    // Mouvement rapide à droite
    setLedColor(255, 165, 0);  // Orange
    spinRight(SPEED_FAST);
    delay(DURATION_SHORT);
    playNote(NOTE_D4, 30);
  }

  // Spin final
  setLedColor(255, 255, 255);  // Blanc
  spinRight(SPEED_FAST);
  delay(800);
  playNote(NOTE_C5, 200);

  stopMotors();
}

// ==================== CONTRÔLE DES MOTEURS ====================

void moveForward(int speed) {
  motorLeft.setTarPWM(speed);
  motorRight.setTarPWM(-speed);
  updateMotors();
}

void moveBackward(int speed) {
  motorLeft.setTarPWM(-speed);
  motorRight.setTarPWM(speed);
  updateMotors();
}

void spinLeft(int speed) {
  motorLeft.setTarPWM(-speed);
  motorRight.setTarPWM(-speed);
  updateMotors();
}

void spinRight(int speed) {
  motorLeft.setTarPWM(speed);
  motorRight.setTarPWM(speed);
  updateMotors();
}

void stopMotors() {
  motorLeft.setTarPWM(0);
  motorRight.setTarPWM(0);
  updateMotors();
}

void updateMotors() {
  motorLeft.loop();
  motorRight.loop();
}

// ==================== CONTRÔLE DES LEDs ====================

void setLedColor(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setColor(0, r, g, b);  // 0 = toutes les LEDs
  rgbLed.show();
}

void flashLeds(uint8_t r, uint8_t g, uint8_t b, int times) {
  for (int i = 0; i < times; i++) {
    setLedColor(r, g, b);
    delay(100);
    setLedColor(0, 0, 0);
    delay(100);
  }
}

void rainbowCycle(int cycles) {
  for (int c = 0; c < cycles; c++) {
    for (int i = 0; i < 256; i += 8) {
      for (int led = 1; led <= 12; led++) {
        int hue = (i + led * 20) % 256;
        uint8_t r, g, b;
        hueToRgb(hue, &r, &g, &b);
        rgbLed.setColor(led, r, g, b);
      }
      rgbLed.show();
      delay(20);
    }
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

void startupAnimation() {
  // Animation de démarrage avec les LEDs
  for (int led = 1; led <= 12; led++) {
    rgbLed.setColor(led, 0, 255, 0);
    rgbLed.show();
    playNote(NOTE_C4 + (led * 20), 50);
    delay(100);
  }

  delay(300);

  // Flash final
  for (int i = 0; i < 3; i++) {
    setLedColor(255, 255, 255);
    delay(100);
    setLedColor(0, 0, 0);
    delay(100);
  }
}

// ==================== CONTRÔLE DU BUZZER ====================

void playNote(int frequency, int duration) {
  buzzer.tone(frequency, duration);
  delay(duration);
}

void playMelody() {
  int melody[] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5};
  int durations[] = {200, 200, 200, 400};

  for (int i = 0; i < 4; i++) {
    playNote(melody[i], durations[i]);
    delay(50);
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
