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
// Sur Auriga: Moteur gauche = SLOT1, Moteur droit = SLOT2
MeEncoderOnBoard motorLeft(SLOT1);
MeEncoderOnBoard motorRight(SLOT2);

// Configuration des LEDs RGB intégrées (12 LEDs sur la carte Auriga, pin 44)
MeRGBLed rgbLed(0, 12);

// Configuration du buzzer (pin 45 sur Auriga)
MeBuzzer buzzer;
#define BUZZER_PIN 45

// Vitesse de base pour les mouvements (0-255)
const int SPEED_SLOW = 150;
const int SPEED_MEDIUM = 220;
const int SPEED_FAST = 255;

// Durées des mouvements en millisecondes
const int DURATION_SHORT = 300;
const int DURATION_MEDIUM = 500;
const int DURATION_LONG = 800;

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

  // Initialisation du buzzer sur pin 45
  buzzer.setpin(BUZZER_PIN);

  // Initialisation des LEDs RGB sur pin 44
  rgbLed.setpin(44);
  rgbLed.setColor(0, 0, 0, 0);
  rgbLed.show();

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
  playNote(NOTE_C4, 150);

  // Série de rotations alternées
  for (int i = 0; i < 3; i++) {
    // Tourner à gauche
    setLedColor(255, 0, 0);  // Rouge
    spinLeft(SPEED_FAST);
    delay(DURATION_MEDIUM);
    stopMotors();
    playNote(NOTE_E4, 80);
    delay(100);

    // Tourner à droite
    setLedColor(0, 0, 255);  // Bleu
    spinRight(SPEED_FAST);
    delay(DURATION_MEDIUM);
    stopMotors();
    playNote(NOTE_G4, 80);
    delay(100);
  }

  // Finir avec un tour complet
  setLedColor(255, 255, 0);  // Jaune
  spinRight(SPEED_FAST);
  delay(1200);
  stopMotors();
  playNote(NOTE_C5, 200);
}

/**
 * Séquence 2: Le moonwalk
 * Avancer et reculer avec courbes
 */
void danceSequence2() {
  playNote(NOTE_A4, 150);

  // Avancer en zigzag
  for (int i = 0; i < 2; i++) {
    // Avancer + courbe gauche
    setLedColor(0, 255, 0);  // Vert
    motorLeft.setMotorPwm(-SPEED_SLOW);
    motorRight.setMotorPwm(SPEED_FAST);
    delay(DURATION_LONG);
    playNote(NOTE_D4, 50);

    // Avancer + courbe droite
    setLedColor(0, 255, 128);  // Vert clair
    motorLeft.setMotorPwm(-SPEED_FAST);
    motorRight.setMotorPwm(SPEED_SLOW);
    delay(DURATION_LONG);
    playNote(NOTE_F4, 50);
  }

  stopMotors();
  flashLeds(255, 255, 0, 2);

  // Reculer droit
  setLedColor(255, 0, 255);  // Magenta
  moveBackward(SPEED_FAST);
  delay(1000);
  stopMotors();
  playNote(NOTE_A4, 150);
}

/**
 * Séquence 3: Le shake
 * Mouvements courts et énergiques
 */
void danceSequence3() {
  playNote(NOTE_F4, 150);

  // Vibrations rapides
  for (int i = 0; i < 8; i++) {
    setLedColor(0, 255, 255);  // Cyan
    spinLeft(SPEED_FAST);
    delay(150);

    setLedColor(255, 165, 0);  // Orange
    spinRight(SPEED_FAST);
    delay(150);

    playNote(NOTE_B4 + i * 20, 30);
  }

  stopMotors();
  delay(200);

  // Avancer-reculer rapide
  for (int i = 0; i < 3; i++) {
    setLedColor(255, 0, 0);
    moveForward(SPEED_FAST);
    delay(250);

    setLedColor(0, 0, 255);
    moveBackward(SPEED_FAST);
    delay(250);

    playNote(NOTE_C5, 50);
  }

  // Spin final spectaculaire
  setLedColor(255, 255, 255);  // Blanc
  spinLeft(SPEED_FAST);
  delay(1500);
  stopMotors();

  flashLeds(0, 255, 0, 5);
  playNote(NOTE_C5, 300);
}

// ==================== CONTRÔLE DES MOTEURS ====================
// Note: setMotorPwm() = contrôle PWM direct (plus fiable)
// Sur mBot Ranger: SLOT1=gauche, SLOT2=droite
// Les moteurs sont montés en miroir, donc signes opposés pour avancer

void moveForward(int speed) {
  motorLeft.setMotorPwm(-speed);   // Gauche: négatif pour avancer
  motorRight.setMotorPwm(speed);   // Droite: positif pour avancer
}

void moveBackward(int speed) {
  motorLeft.setMotorPwm(speed);    // Gauche: positif pour reculer
  motorRight.setMotorPwm(-speed);  // Droite: négatif pour reculer
}

void spinLeft(int speed) {
  motorLeft.setMotorPwm(speed);    // Gauche: recule
  motorRight.setMotorPwm(speed);   // Droite: avance
}

void spinRight(int speed) {
  motorLeft.setMotorPwm(-speed);   // Gauche: avance
  motorRight.setMotorPwm(-speed);  // Droite: recule
}

void stopMotors() {
  motorLeft.setMotorPwm(0);
  motorRight.setMotorPwm(0);
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

