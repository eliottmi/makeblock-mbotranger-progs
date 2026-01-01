# mBot Ranger Mapper (Cartographe)

Un programme Arduino qui permet au mBot Ranger de cartographier son environnement et d'afficher une carte 2D.

## Description

Ce projet utilise l'odométrie et le capteur ultrasonique pour construire une carte de l'environnement:

1. **Odométrie**: Utilise les encodeurs des moteurs pour estimer la position du robot (X, Y, angle)
2. **Scan**: Mesure les distances aux obstacles avec le capteur ultrasonique
3. **Cartographie**: Construit une grille 2D représentant l'environnement
4. **Visualisation**: Affiche la carte sur le moniteur série

## Matériel requis

- Makeblock mBot Ranger (avec carte Me Auriga)
- Capteur ultrasonique Me (connecté au port 10)
- Câble USB pour la programmation et visualisation
- Batteries (6x AA ou batterie Li-Po)

## Installation

### 1. Connexion du capteur

Connectez le capteur ultrasonique au **port 10** de la carte Auriga.

### 2. Téléverser le programme

1. Ouvrir `mbot-ranger-mapper.ino` dans Arduino IDE
2. Sélectionner **Tools > Board > Arduino Mega 2560**
3. Téléverser le programme

### 3. Moniteur série

1. Ouvrir **Tools > Serial Monitor**
2. Régler le baud rate sur **115200**

## Utilisation

### Commandes

| Commande | Action |
|----------|--------|
| `s` | **Start** - Démarrer la cartographie |
| `p` | **Print** - Afficher la carte actuelle |
| `r` | **Reset** - Réinitialiser la carte |
| `q` | **Quit** - Arrêter la cartographie |

### Fonctionnement

1. Placer le robot au centre d'une zone à explorer
2. Ouvrir le moniteur série (115200 baud)
3. Envoyer `s` pour démarrer
4. Le robot va:
   - Scanner à 360° pour détecter les obstacles
   - Avancer si la voie est libre
   - Tourner s'il y a un obstacle
   - Répéter jusqu'à 20 étapes d'exploration
5. La carte s'affiche automatiquement à la fin

### Lecture de la carte

```
========== CARTE ==========
Position robot: (150.00, 150.00) angle: 45.00°

..............................
..............................
..........###.................
..........#  #................
..........#   #...............
..........#    >..............
..........#   #...............
..........#  #................
..........###.................
..............................

Légende: . = inconnu, espace = libre, # = obstacle
         > ^ v < = robot (direction)
```

## Paramètres configurables

### Taille de la carte

```cpp
const int MAP_WIDTH = 30;      // Largeur en cellules
const int MAP_HEIGHT = 30;     // Hauteur en cellules
const float CELL_SIZE = 10.0;  // Taille d'une cellule en cm
```

Carte par défaut: 30x30 cellules = 300x300 cm = 3x3 mètres

### Dimensions du robot

```cpp
const float WHEEL_DIAMETER = 6.4;  // Diamètre roues (cm)
const float WHEEL_BASE = 13.0;     // Empattement (cm)
```

### Vitesses

```cpp
const int SPEED_MOVE = 120;  // Vitesse de déplacement
const int SPEED_TURN = 100;  // Vitesse de rotation
```

## Fonctionnement technique

### Odométrie

Le robot estime sa position grâce aux encodeurs des moteurs:

```
ΔS = (ΔL + ΔR) / 2     // Distance parcourue
Δθ = (ΔR - ΔL) / L     // Changement d'angle
X += ΔS × cos(θ)       // Nouvelle position X
Y += ΔS × sin(θ)       // Nouvelle position Y
```

### Algorithme de cartographie

1. **Scan**: À chaque position, le robot mesure les distances dans 8 directions (0°, 45°, 90°, etc.)

2. **Ray casting**: Pour chaque mesure, l'algorithme de Bresenham marque les cellules traversées comme "libres"

3. **Obstacles**: La cellule finale (où l'obstacle est détecté) est marquée comme "obstacle"

4. **Exploration**: Le robot choisit d'avancer si possible, sinon tourne

### Types de cellules

| Valeur | Symbole | Signification |
|--------|---------|---------------|
| 0 | `.` | Inconnu (non exploré) |
| 1 | ` ` | Libre (pas d'obstacle) |
| 2 | `#` | Obstacle détecté |

## Indicateurs LED

| Couleur | Signification |
|---------|---------------|
| Bleu | Scan en cours |
| Vert | Déplacement |
| Jaune | Rotation |
| Rouge | Arrêté |

## Limitations

- **Précision de l'odométrie**: L'estimation de position dérive avec le temps (glissement des roues)
- **Capteur fixe**: Le capteur ultrasonique ne peut scanner qu'en tournant le robot entier
- **Mémoire**: La carte 30x30 utilise 900 octets de RAM

## Améliorations possibles

- Ajouter un servo pour scanner sans tourner le robot
- Implémenter SLAM (Simultaneous Localization and Mapping)
- Ajouter la détection de boucle pour corriger la dérive
- Exporter la carte vers un fichier
- Navigation autonome vers un point cible

## Dépannage

### La carte est décalée
- L'odométrie dérive avec le temps
- Calibrer les paramètres du robot (diamètre roues, empattement)

### Le robot ne détecte pas les obstacles
- Vérifier la connexion du capteur au port 10
- Certaines surfaces absorbent les ultrasons

### Mémoire insuffisante
- Réduire MAP_WIDTH et MAP_HEIGHT
- Augmenter CELL_SIZE

## Licence

MIT License
