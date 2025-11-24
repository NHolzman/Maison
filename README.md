# Système d’Alarme Domestique Connectée – Documentation complète

Projet réalisé avec :
- 2 × Particle Photon 2
- 2 × ESP32 (style Arduino)
- MQTT + BLE
- Capteurs reed, RFID, HC-SR04 sur servo, keypad 4×4, verrou électrique, buzzer, ampoule

## 1. Vue d’ensemble du système

![Schéma d’architecture complet](attachments/smarthome_architecture_final.svg)

> **Légende**  
> - **Orange** = communication MQTT (tout passe par le broker)  
> - **Bleu** = communication BLE (seulement entre les deux ESP32)

### Composants et rôles

| Module                  | Composants                                                                 | Communication       | Rôle principal |
|-------------------------|-----------------------------------------------------------------------------|---------------------|---------------|
| **Photon 2 #1**         | 2× reed switch (fenêtres gauche & droite), lecteur RFID                    | MQTT                | Surveillance fenêtres + désarmement par badge |
| **Photon 2 #2**         | Buzzer + ampoule (état armé)                                                | MQTT                | Alarme sonore & visuelle |
| **ESP32 #1**            | HC-SR04 monté sur servo (balayage)                                          | BLE uniquement      | Détection de présence dans l’entrée |
| **ESP32 #2**            | Keypad 4×4, LCD 16×2, verrou électrique, reed switch porte                 | BLE + MQTT          | Interface porte + gestion accès |

## 2. Schéma d’architecture détaillé (SVG)

→ [Télécharger le schéma haute qualité](attachments/smarthome_architecture_final.svg)

(Le fichier SVG ci-dessus est entièrement vectoriel, modifiable et avec icônes réalistes)

## 3. Diagrammes de cas d’usage (Use Case)

### Use Case 1 – Alarme déclenchée par ouverture de fenêtre (système armé)

![Use Case Alarme Fenêtre](attachments/usecase_alarme_fenetre.svg)

### Use Case 2 – Déverrouillage porte avec le bon code

![Use Case Déverrouillage](attachments/usecase_deverrouillage_porte.svg)

## 4. Flux détaillés

### Déclenchement d’alarme (fenêtre ouverte + système armé + porte verrouillée)
1. Photon 2 #1 détecte ouverture reed switch
2. Publie `window/left` ou `window/right` → `OPEN` sur MQTT
3. Serveur (Node-RED/Home Assistant) reçoit l’événement
4. Vérifie : système armé ? + porte verrouillée ?
5. Si oui → publie `alarm/trigger` → `ON`
6. Photon 2 #2 reçoit → buzzer en continu + ampoule allumée

### Déverrouillage avec code correct
1. Utilisateur tape code sur keypad (ESP32 #2)
2. ESP32 #2 affiche code sur LCD et publie `door/code` → `1234` sur MQTT
3. Serveur compare avec code autorisé
4. Si bon → publie `door/unlock` → `true`
5. ESP32 #2 reçoit → active le verrou électrique 5 secondes

### Détection de présence (bonus)
1. ESP32 #1 fait tourner le servo + mesure distance
2. Objet proche (< 50 cm) → envoie statut via BLE à ESP32 #2
3. ESP32 #2 publie `presence/detected` → `true` sur MQTT
4. Photon 2 #2 → buzzer court (bip unique)

## 5. Fichiers à télécharger

| Fichier                                    | Description                                 |
|--------------------------------------------|---------------------------------------------|
| [smarthome_architecture_final.svg](attachments/smarthome_architecture_final.svg)       | Schéma complet avec icônes réalistes        |
| [usecase_alarme_fenetre.svg](attachments/usecase_alarme_fenetre.svg)                   | Use case alarme fenêtre                     |
| [usecase_deverrouillage_porte.svg](attachments/usecase_deverrouillage_porte.svg)       | Use case déverrouillage code                |

## 6. Prochaines étapes possibles

- [ ] Ajout sirène extérieure
- [ ] Notifications push (Telegram / Home Assistant)
- [ ] Journal d’événements dans base de données
- [ ] Caméra déclenchée par mouvement
- [ ] Mode vacances / mode nuit

---
**Projet terminé et 100 % fonctionnel – Décembre 2025**

Tout est open-source, modifiable et prêt à être déployé !
