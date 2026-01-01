# mBot Ranger Guardian (Gardien)

Robot de surveillance qui patrouille une zone et déclenche une alarme en cas d'intrusion.

## Description

Le Guardian offre trois modes de surveillance:

### Mode Patrouille
- Se déplace automatiquement dans la zone
- Scanne continuellement pour détecter les intrus
- Suit un parcours en carré avec variations
- Évite les obstacles

### Mode Sentinelle
- Reste statique à un point fixe
- Surveille une direction précise
- Consomme moins d'énergie
- Idéal pour surveiller une entrée

### Mode Tracking
- Suit un objet/personne détectée
- Maintient une distance idéale (30-60 cm)
- S'arrête si la cible est trop loin

## Matériel requis

- Makeblock mBot Ranger (carte Me Auriga)
- Capteur ultrasonique Me (port 10)
- Câble USB
- Batteries

## Installation

1. Connecter le capteur ultrasonique au **port 10**
2. Ouvrir `mbot-ranger-guardian.ino` dans Arduino IDE
3. Sélectionner **Tools > Board > Arduino Mega 2560**
4. Téléverser le programme
5. Ouvrir le moniteur série (115200 baud)

## Commandes

| Cmd | Mode | Description |
|-----|------|-------------|
| `p` | Patrouille | Déplacement + surveillance active |
| `s` | Sentinelle | Statique + surveillance |
| `t` | Tracking | Suit les mouvements détectés |
| `c` | Calibrer | Mesure la distance de référence |
| `q` | Arrêt | Stop tous les modes |
| `h` | Aide | Affiche les commandes |

## Indicateurs LED

| Couleur | Signification |
|---------|---------------|
| Vert faible | Veille (idle) |
| Bleu pulsé | Patrouille en cours |
| Cyan | Mode sentinelle |
| Magenta | Mode tracking |
| Jaune | Calibration |
| Rouge/Bleu clignotant | ALARME! |

## Fonctionnement

### Détection d'intrusion

1. **Calibration**: Le robot mesure la distance "normale" devant lui
2. **Surveillance**: Compare en continu avec la distance de référence
3. **Détection**: Si un objet apparaît plus proche que prévu (différence > 20 cm)
4. **Confirmation**: 3 mesures consécutives pour éviter les faux positifs
5. **Alarme**: Sirène + LEDs clignotantes pendant 5 secondes

### Paramètres

```cpp
const float DETECTION_DISTANCE = 50.0;   // Distance max de détection
const float BASELINE_TOLERANCE = 20.0;   // Tolérance de changement
const int DETECTION_SAMPLES = 3;         // Échantillons pour confirmer
const unsigned long ALARM_DURATION = 5000; // Durée de l'alarme (ms)
```

## Exemples d'utilisation

### Surveillance d'une entrée
1. Placer le robot face à la porte
2. Envoyer `s` pour mode sentinelle
3. Le robot calibre et surveille
4. Alarme si quelqu'un passe devant

### Patrouille d'une pièce
1. Placer le robot au centre de la zone
2. Envoyer `p` pour mode patrouille
3. Le robot parcourt la zone en carré
4. Détecte les intrus pendant le déplacement

### Suivre une personne
1. Se placer devant le robot (< 1.5m)
2. Envoyer `t` pour mode tracking
3. Le robot maintient une distance de 30-60 cm
4. S'arrête si vous vous éloignez trop

## Personnalisation

### Ajuster la sensibilité

```cpp
const float BASELINE_TOLERANCE = 20.0;  // Diminuer = plus sensible
const int DETECTION_SAMPLES = 3;        // Augmenter = moins de faux positifs
```

### Modifier le parcours de patrouille

Dans `nextPatrolState()`, modifiez la séquence:
```cpp
switch (patrolStep % 8) {
  case 0: patrolState = PATROL_FORWARD; break;
  case 1: patrolState = PATROL_TURN_RIGHT; break;
  // etc.
}
```

### Changer la durée de l'alarme

```cpp
const unsigned long ALARM_DURATION = 5000;  // 5 secondes
```

## Dépannage

### Faux positifs fréquents
- Augmenter `BASELINE_TOLERANCE`
- Augmenter `DETECTION_SAMPLES`
- Éviter les surfaces réfléchissantes

### Ne détecte pas les intrus
- Vérifier la connexion du capteur
- Diminuer `DETECTION_DISTANCE`
- Recalibrer avec `c`

### Le robot tourne en rond
- Vérifier les batteries
- Surface trop glissante

## Licence

MIT License
