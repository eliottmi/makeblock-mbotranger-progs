# mBot Ranger Zone Explorer

Un programme Arduino qui permet au mBot Ranger d'explorer une zone, mémoriser son parcours, puis le rejouer en accéléré.

## Description

Ce projet démontre un comportement de robot autonome en deux phases:

### Phase 1: Exploration (60 secondes)
- Le robot se déplace **lentement** dans la zone
- Utilise le **capteur ultrasonique** pour détecter les obstacles
- **Scanne** gauche/droite pour choisir la meilleure direction
- **Mémorise** chaque mouvement (type + durée)
- LEDs bleues pendant l'exploration

### Phase 2: Replay
- Le robot **rejoue** tous les mouvements mémorisés
- Vitesse **2.5x plus rapide** que l'exploration
- Durées proportionnellement réduites
- LEDs vertes pendant le replay
- Animation de fin puis recommence

## Matériel requis

- Makeblock mBot Ranger (avec carte Me Auriga)
- Capteur ultrasonique Me (connecté au port 10)
- Câble USB pour la programmation
- Batteries (6x AA ou batterie Li-Po)

## Prérequis logiciels

- [Arduino IDE](https://www.arduino.cc/en/software) (version 1.8.x ou supérieure)
- [Bibliothèque MakeblockDrive](https://github.com/Makeblock-official/Makeblock-Libraries)

## Installation

### 1. Connexion du capteur ultrasonique

Connectez le capteur ultrasonique Me au **port 10** de la carte Auriga.

### 2. Installer la bibliothèque Makeblock

1. Télécharger la bibliothèque depuis [GitHub](https://github.com/Makeblock-official/Makeblock-Libraries)
2. Dans Arduino IDE: **Sketch > Include Library > Add .ZIP Library**
3. Sélectionner le fichier ZIP téléchargé

### 3. Configurer la carte

1. Dans Arduino IDE: **Tools > Board > Arduino/Genuino Mega or Mega 2560**
2. Sélectionner le port COM correspondant: **Tools > Port**

### 4. Téléverser le programme

1. Ouvrir le fichier `mbot-ranger-zone-explorer.ino`
2. Cliquer sur le bouton **Upload**
3. Attendre la fin du téléversement

## Utilisation

1. Placer le robot dans une zone dégagée avec quelques obstacles
2. Allumer le robot
3. Le robot effectue un compte à rebours de 3 secondes
4. **Phase d'exploration**: Observer le robot explorer lentement (LEDs bleues)
5. **Phase de replay**: Le robot refait le parcours rapidement (LEDs vertes)
6. Le cycle recommence automatiquement

## Paramètres configurables

### Vitesses

```cpp
const int SPEED_EXPLORE = 80;   // Vitesse d'exploration (lente)
const int SPEED_REPLAY = 200;   // Vitesse de replay (rapide)
```

### Distances de détection

```cpp
const int OBSTACLE_DISTANCE = 25;  // Distance de détection (cm)
const int DANGER_DISTANCE = 10;    // Distance de danger (cm)
```

### Durée d'exploration

```cpp
const int EXPLORE_DURATION = 60000;  // 60 secondes par défaut
```

Pour modifier la durée, changez cette valeur (en millisecondes):
- 30000 = 30 secondes
- 60000 = 1 minute
- 120000 = 2 minutes

### Port du capteur ultrasonique

```cpp
MeUltrasonicSensor ultraSensor(PORT_10);  // Changer si autre port
```

## Fonctionnement technique

### Mémorisation des mouvements

Le programme stocke jusqu'à 500 mouvements dans un tableau:

```cpp
struct Movement {
  MoveType type;        // FORWARD, BACKWARD, TURN_LEFT, TURN_RIGHT
  unsigned long duration;  // Durée en millisecondes
};
```

### Algorithme d'évitement

1. Détection d'obstacle à moins de 25 cm
2. Scan à gauche (mesure distance)
3. Scan à droite (mesure distance)
4. Choix de la direction avec le plus d'espace
5. Rotation aléatoire (300-700ms) pour varier la couverture

### Calcul du replay

La durée de chaque mouvement en replay est calculée:
```
durée_replay = durée_originale / ratio_vitesse
```

Avec un ratio de 2.5, un mouvement de 1000ms devient 400ms.

## Indicateurs LED

| Couleur | Signification |
|---------|---------------|
| Bleu | Mode exploration |
| Orange | Obstacle détecté |
| Rouge (flash) | Danger - obstacle très proche |
| Vert | Mode replay |
| Cyan (gauche) | Rotation à gauche |
| Magenta (droite) | Rotation à droite |
| Arc-en-ciel | Animation de fin |

## Sortie série

Connectez le moniteur série (115200 baud) pour voir:
- Progression de l'exploration
- Détection des obstacles
- Distances mesurées
- Détail de chaque mouvement en replay

Exemple de sortie:
```
*** PHASE D'EXPLORATION ***
Exploration: 25% - Mouvements mémorisés: 12
Obstacle détecté - Changement de direction
Distance gauche: 45.2 cm, Distance droite: 23.1 cm

*** PHASE DE REPLAY ***
Replay mouvement 1/12: AVANCER (400ms)
Replay mouvement 2/12: TOURNER GAUCHE (200ms)
...
```

## Dépannage

### Le robot ne détecte pas les obstacles
- Vérifier la connexion du capteur ultrasonique au port 10
- Vérifier que le capteur n'est pas obstrué
- Les surfaces très absorbantes (tissus, mousse) peuvent ne pas être détectées

### Le robot tourne toujours du même côté
- Le scan gauche/droite peut échouer si les obstacles sont trop proches
- Augmenter `OBSTACLE_DISTANCE` pour détecter plus tôt

### Le replay est trop rapide/lent
- Ajuster `SPEED_REPLAY` ou `SPEED_EXPLORE`
- Le ratio est calculé automatiquement

### Mémoire insuffisante
- Le programme peut stocker 500 mouvements maximum
- Pour une exploration plus longue, réduire `MAX_MOVEMENTS` ou la durée d'exploration

## Améliorations possibles

- Ajouter un capteur de ligne pour détecter les limites de zone
- Utiliser les encodeurs pour une mémorisation basée sur la distance
- Ajouter une cartographie 2D de la zone
- Implémenter un algorithme de couverture plus efficace (spirale, zigzag)

## Licence

MIT License - Voir le fichier LICENSE pour plus de détails.

## Ressources

- [Documentation Makeblock](https://www.makeblock.com/pages/mbot-ranger)
- [Capteur ultrasonique Me](https://www.makeblock.com/project/me-ultrasonic-sensor)
- [Forum Makeblock](https://forum.makeblock.com/)
