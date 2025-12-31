# mBot Ranger Dance

Un programme Arduino qui fait danser le mBot Ranger de Makeblock avec des mouvements rythmiques, des animations LED colorées et des sons.

## Description

Ce projet exemple montre comment programmer le mBot Ranger pour exécuter une chorégraphie de danse composée de trois séquences différentes:

1. **Le Twist**: Le robot tourne alternativement à gauche et à droite avec des LEDs rouge et bleue
2. **Le Moonwalk**: Mouvements avant/arrière avec des transitions fluides et des flashs lumineux
3. **Le Shake**: Mouvements rapides et saccadés avec un spin final

## Matériel requis

- Makeblock mBot Ranger (avec carte Me Auriga)
- Câble USB pour la programmation
- Batteries (6x AA ou batterie Li-Po)

## Prérequis logiciels

- [Arduino IDE](https://www.arduino.cc/en/software) (version 1.8.x ou supérieure)
- [Bibliothèque MakeblockDrive](https://github.com/Makeblock-official/Makeblock-Libraries)

## Installation

### 1. Installer la bibliothèque Makeblock

1. Télécharger la bibliothèque depuis [GitHub](https://github.com/Makeblock-official/Makeblock-Libraries)
2. Dans Arduino IDE: **Sketch > Include Library > Add .ZIP Library**
3. Sélectionner le fichier ZIP téléchargé

### 2. Configurer la carte

1. Dans Arduino IDE: **Tools > Board > Arduino/Genuino Mega or Mega 2560**
2. Sélectionner le port COM correspondant: **Tools > Port**

### 3. Téléverser le programme

1. Ouvrir le fichier `mbot-ranger-dance.ino`
2. Cliquer sur le bouton **Upload** (flèche vers la droite)
3. Attendre la fin du téléversement

## Utilisation

Une fois le programme téléversé:

1. Débrancher le câble USB
2. Placer le robot sur une surface plane et dégagée
3. Allumer le robot avec l'interrupteur
4. Le robot exécutera automatiquement sa danse en boucle

## Personnalisation

### Modifier les vitesses

```cpp
const int SPEED_SLOW = 100;    // Vitesse lente
const int SPEED_MEDIUM = 150;  // Vitesse moyenne
const int SPEED_FAST = 200;    // Vitesse rapide
```

### Modifier les durées

```cpp
const int DURATION_SHORT = 200;   // 200ms
const int DURATION_MEDIUM = 400;  // 400ms
const int DURATION_LONG = 600;    // 600ms
```

### Ajouter une nouvelle séquence

Créez une nouvelle fonction de séquence en suivant ce modèle:

```cpp
void maNouvelleDanse() {
  // Définir la couleur des LEDs
  setLedColor(255, 0, 0);  // Rouge

  // Exécuter un mouvement
  spinLeft(SPEED_MEDIUM);
  delay(500);

  // Jouer un son
  playNote(NOTE_C4, 100);

  // Arrêter les moteurs
  stopMotors();
}
```

Puis appelez-la dans la fonction `loop()`.

## Fonctions disponibles

### Mouvements

| Fonction | Description |
|----------|-------------|
| `moveForward(speed)` | Avancer |
| `moveBackward(speed)` | Reculer |
| `spinLeft(speed)` | Tourner sur place à gauche |
| `spinRight(speed)` | Tourner sur place à droite |
| `stopMotors()` | Arrêter les moteurs |

### LEDs

| Fonction | Description |
|----------|-------------|
| `setLedColor(r, g, b)` | Définir la couleur de toutes les LEDs |
| `flashLeds(r, g, b, times)` | Faire clignoter les LEDs |
| `rainbowCycle(cycles)` | Animation arc-en-ciel |

### Son

| Fonction | Description |
|----------|-------------|
| `playNote(frequency, duration)` | Jouer une note |
| `playMelody()` | Jouer une mélodie prédéfinie |

## Dépannage

### Le robot ne démarre pas
- Vérifier que les batteries sont chargées
- Vérifier que l'interrupteur est sur ON

### Les mouvements sont erratiques
- Placer le robot sur une surface plane
- Vérifier que les roues ne sont pas bloquées

### Pas de son
- Le buzzer est intégré à la carte, vérifier qu'il n'est pas endommagé

## Licence

MIT License - Voir le fichier LICENSE pour plus de détails.

## Ressources

- [Documentation Makeblock](https://www.makeblock.com/pages/mbot-ranger)
- [Forum Makeblock](https://forum.makeblock.com/)
- [GitHub Makeblock Libraries](https://github.com/Makeblock-official/Makeblock-Libraries)
