# Evolver Desktop – MIDI spec (source de vérité d'EvoCtrl)

Source unique : **Evolver Operation Manual v3.2a (juillet 2010), Dave Smith Instruments**
(le manuel de l'Evolver *desktop* mono). Les numéros `p.NN` sont les numéros de page imprimés.
Rien dans ce fichier n'est inventé : tout ce qui n'est pas dans le manuel est marqué **⚠ AMBIGU / TODO**.

---

## 0. Écart majeur avec le cahier des charges : pas de NRPN

Le chapitre *MIDI Implementation* (p.41–62) **ne documente aucun NRPN** (aucune mention de CC 99/98/6/38).
L'Evolver desktop échange ses paramètres programme **uniquement en SysEx** :

- édition d'un paramètre : message SysEx *Program Parameter* (§3.1) ;
- émission des mouvements de potards : « If enabled Evolver will send out changes for all 128 Program
  Parameters in System Exclusive (SysEx) format » (p.17, réglage *MIDI Xmit*).

Conséquences dans EvoCtrl :

| Prévu dans le brief | Implémenté (conforme au manuel) |
|---|---|
| NRPN CC 99/98/6/38 | SysEx *Program Parameter* (9 octets), même file d'envoi coalescée / débit limité |
| Fallback « via Ableton » en NRPN | Fallback en **CC** pour les ~40 paramètres qui ont un CC documenté (p.44–45). Les autres paramètres ne passent **pas** par Ableton (le SysEx n'y transite pas). |

⚠ **AMBIGU / TODO (CC)** : le manuel liste les CC reçus mais **ne précise pas la mise à l'échelle**
0–127 → plage du paramètre (ex. Filter Frequency 0–164). EvoCtrl envoie une mise à l'échelle linéaire
`cc = round(v * 127 / max)` (à valider sur le matériel). Le mode fallback est donc marqué *expérimental*.

---

## 1. Réglages globaux requis sur l'Evolver (menu MAIN, p.16–17, p.41, p.53–54)

| Réglage (Main param #) | Valeur requise | Pourquoi |
|---|---|---|
| **MIDI Rec** (#12) | `ALL` (1) | Recevoir dumps + paramètres SysEx. `par` (3) suffit pour les paramètres, mais son effet sur les dumps n'est pas documenté ⚠. |
| **MIDI Xmit** (#13) | `ALL` (1) ou `par` (3) | Émettre les mouvements de potards en SysEx (p.17). |
| **MIDI Channel** (#14) | `ALL` (0) ou 1–16 identique à EvoCtrl | Les messages SysEx Evolver ne portent pas de canal (seul le *Device Inquiry* en a un). Le canal sert aux CC du fallback et aux Program Change. |
| **Poly Chain** (#9) | `OFF` (0) | Évite que l'Evolver ne renvoie (echo) le MIDI reçu → boucles. |

Remarque : le manuel v3.2a ne mentionne **pas** de réglage « MIDI SysEx On/Off » (ce réglage existe sur
d'autres modèles DSI). Rien à activer de plus.

Main Parameters (p.53–54), pour référence : 0 Program 0–127 · 1 Bank 0–3 · 2 Master Volume 0–100 ·
3 Master Transpose 0–72 (36 = 0) · 4 BPM 30–250 · 5 Clock Divide 0–12 · 6 Use Program Tempo 0–1 ·
7 MIDI Clock 0–6 · 8 Lock Sequence 0–1 · 9 Poly Chain 0–2 · 10 Input Gain 0–8 · 11 Master Fine Tune 0–100 ·
12 MIDI Receive 0–3 · 13 MIDI Transmit 0–3 · 14 MIDI Channel 0–16.

⚠ AMBIGU : le *Main Parameter Data Dump* annonce « 32 nibbles for 16 Main parameters » (p.49) alors que
seuls 0–14 sont listés et que « Parameter 15 not available » (p.54). EvoCtrl n'édite pas les Main params (MVP).

---

## 2. Format SysEx commun (p.47–52)

```
F0  01  20  01  <cmd> [données…]  F7
    │   │   └── File Version = 1
    │   └────── Evolver ID = 0x20
    └────────── DSI ID = 0x01
```

| cmd | Message | Données | Sens | Page |
|---|---|---|---|---|
| `0x01` | Program Parameter | `pp` (0–127), `ls nibble`, `ms nibble` | ↔ | p.47 |
| `0x08` | Sequence Parameter | `step` (0–63), `ls nibble`, `ms nibble` | ↔ | p.47 |
| `0x09` | Main Parameter | `pp` (0–14), `ls nibble`, `ms nibble` | ↔ | p.47 |
| `0x02` | Program Data Dump | `bank` 0–3, `prog` 0–127, 220 octets packés | ↔ (en réception : **écrit** à cet emplacement, p.17) | p.48 |
| `0x03` | Edit Buffer Data Dump | 220 octets packés | ↔ | p.48 |
| `0x0A` | Waveshape Data Dump | `wave` 0–127, 293 octets packés (seules 96–127 inscriptibles) | ↔ | p.48 |
| `0x11` | Program Name Data Dump | `bank`, `prog`, 16 octets ASCII 7 bits | ↔ | p.49 |
| `0x0F` | Main Parameter Data Dump | 32 nibbles | ↔ | p.49 |
| `0x12` | Start/Stop button | – | → | p.49 |
| `0x04` | Reset button | – | → | p.49 |
| `0x13` / `0x14` | Shift button On / Off | – | → | p.50 |
| `0x05` | Request Program Dump | `bank`, `prog` | → (réponse `0x02`) | p.51 |
| `0x06` | Request Edit Buffer Dump | – | → (réponse `0x03`) | p.51 |
| `0x0B` | Request Waveshape Dump | `wave` | → (réponse `0x0A`) | p.51 |
| `0x0E` | Request Main Parameter Dump | – | → (réponse `0x0F`) | p.52 |
| `0x10` | Request Program Name Dump | `bank`, `prog` | → (réponse `0x11`) | p.52 |

Valeurs « nibble » : `valeur = ls | (ms << 4)` (ex. 198 → `06 0C`).

**Device Inquiry** (universel, p.46) : `F0 7E <ch|7F> 06 01 F7` → réponse
`F0 7E <ch|7F> 06 02 01 20 00 00 00 <PIC ver> 00 <DSP ver> 00 F7`.

### 2.1 Opérations utilisées par EvoCtrl

| Fonction | Message |
|---|---|
| Test / Get from Evolver | envoie `0x0E` + `0x06`, attend `0x0F` (banque/programme courants, Main params #1/#0, p.53) + `0x03`, puis demande le nom (`0x10`) |
| Programme changé sur l'Evolver | Program Change reçu (émis si *MIDI Xmit* = ALL ou pro, p.17) → même séquence que Get. ⚠ L'émission d'un Bank Select (CC 32) par l'Evolver n'est pas documentée : la banque vient de `0x0F`. |
| Send to Evolver | envoie `0x03` (edit buffer complet) |
| Édition temps réel | `0x01` (paramètre) / `0x08` (pas de séquence) |
| Lire banque+programme | `0x05` (+ `0x10` pour le nom) → réception `0x02` (+ `0x11`) |
| Écrire banque+programme | `0x02` avec bank/prog cible (+ `0x11` pour le nom ⚠ : format documenté, effet en réception non décrit) |
| « Écriture paramètre unique » | `0x01` (pas d'autre commande d'écriture unitaire dans le manuel) |

⚠ Le nom de programme **ne fait pas partie** du dump programme/edit buffer (192 octets = 128 params + 64 pas).

---

## 3. Packing « packed MS bit » (p.50)

Paquets de 8 octets MIDI : 1 octet de bits de poids fort suivi de 7 octets de données (7 bits bas).
Octet 0 du paquet : `bit i` = bit 7 de la donnée `i` du paquet (donnée 0 = A → bit 0).

```
Entrée  A B C D E F G           Sortie  [0 G7 F7 E7 D7 C7 B7 A7] [A&7F] [B&7F] … [G&7F]
```

- Dernier paquet **partiel** : en-tête + N octets restants (déduit des tailles annoncées :
  192 → 27×8 + (1+3) = **220**, 256 → 36×8 + (1+4) = **293** ✓).
- ⚠ Typo du manuel : la ligne 6 de l'exemple indique `F7 E6 E5…` au lieu de `F7 F6 F5…`.

Données d'un dump programme / edit buffer (192 octets dépackés) :
- octets 0–127 : paramètres programme (index = numéro de paramètre ci-dessous) ;
- octets 128–191 : 64 pas de séquence (0–15 séq. 1, 16–31 séq. 2, 32–47 séq. 3, 48–63 séq. 4, cf. p.47).

---

## 4. Séquenceur 4 × 16 (p.18–19, p.47)

- Valeurs de pas : **0–100**. 24 = une octave quand routé vers un oscillateur (demi-demi-ton par pas).
- Au-delà de 100 : `rst` (fin de séquence) puis `oFF` (silence, séquence 1 seulement).
  ⚠ **AMBIGU / TODO** : les valeurs numériques de `rst`/`oFF` ne sont pas documentées (probablement 101/102,
  non vérifié). EvoCtrl affiche/édite 0–100 et préserve telles quelles les valeurs > 100 reçues de l'Evolver.
- Édition en temps réel : SysEx `0x08`. ⚠ Le manuel ne dit pas si l'Evolver *émet* `0x08` quand on tourne
  un potard en mode séquenceur ; EvoCtrl le traite s'il arrive.

---

## 5. Paramètres programme (p.54–59)

Colonne **CC** = CC reçu documenté (p.44–45), utilisé uniquement par le fallback « via Ableton ».
Colonne **Aff.** = type d'affichage dans EvoCtrl. Les **valeurs par défaut** ne figurent pas dans le manuel :
EvoCtrl utilise une valeur neutre (centre pour les bipolaires/fine tune, sinon le minimum) ; elles n'ont
aucune incidence sur le synthé tant qu'on n'envoie rien.

| # | Nom | Plage | Signification | CC | Page |
|---|---|---|---|---|---|
| 0 | Osc 1 Frequency | 0–120 | demi-tons (10 oct.) | 20 | 54 |
| 1 | Osc 1 Fine Tune | 0–100 | 50 = 0, ±50 cents | | 54 |
| 2 | Osc 1 Shape | 0–102 | 0 Saw, 1 Tri, 2 Saw/Tri, 3–102 Pulse PW 0–99 | 28 | 54 |
| 3 | Osc 1 Level | 0–100 | | 24 | 54 |
| 4 | Osc 2 Frequency | 0–120 | demi-tons | 21 | 54 |
| 5 | Osc 2 Fine Tune | 0–100 | 50 = 0 | | 54 |
| 6 | Osc 2 Shape | 0–102 | comme Osc 1 | 29 | 54 |
| 7 | Osc 2 Level | 0–100 | | 25 | 54 |
| 8 | Osc 3 Frequency | 0–120 | demi-tons | 22 | 54 |
| 9 | Osc 3 Fine Tune | 0–100 | 50 = 0 | | 54 |
| 10 | Osc 3 Shape | 0–127 | waveshape 1–128 | 30 | 54 |
| 11 | Osc 3 Level | 0–100 | | 26 | 54 |
| 12 | Osc 4 Frequency | 0–120 | demi-tons | 23 | 54 |
| 13 | Osc 4 Fine Tune | 0–100 | 50 = 0 | | 54–55 |
| 14 | Osc 4 Shape | 0–127 | waveshape 1–128 | 31 | 55 |
| 15 | Osc 4 Level | 0–100 | | 27 | 55 |
| 16 | Filter Frequency | 0–164 | demi-tons | 52 | 55 |
| 17 | Filter Env Amount | 0–198 | −99…+99 | 54 | 55 |
| 18 | Filter Env Attack | 0–110 | | 55 | 55 |
| 19 | Filter Env Decay | 0–110 | | 56 | 55 |
| 20 | Filter Env Sustain | 0–100 | | 57 | 55 |
| 21 | Filter Env Release | 0–110 | | 58 | 55 |
| 22 | Resonance | 0–100 | | 53 | 55 |
| 23 | Filter Key Amount | 0–100 | | 61 | 55 |
| 24 | VCA Level | 0–100 | | | 55 |
| 25 | VCA Env Amount | 0–100 | | | 55 |
| 26 | VCA Env Attack | 0–110 | | 75 | 55 |
| 27 | VCA Env Decay | 0–110 | | 76 | 55 |
| 28 | VCA Env Sustain | 0–100 | | 77 | 55 |
| 29 | VCA Env Release | 0–110 | | 78 | 55 |
| 30 | Output Pan | 0–6 | 0 stéréo plein … 3 mono … 6 stéréo inversé plein | | 55 |
| 31 | Program Volume | 0–100 | | | 55 |
| 32 | Feedback Frequency | 0–48 | demi-tons | 85 | 55 |
| 33 | Feedback Amount | 0–100 | | 86 | 55 |
| 34 | Grunge | 0–1 | off/on | | 55 |
| 35 | Delay 1 Time | 0–166 | | 102 | 55 |
| 36 | Delay 1 Level | 0–100 | | 105 | 55 |
| 37 | Delay Feedback → Delay | 0–100 | somme delays → entrée delay | 108 ⚠ | 55 |
| 38 | Delay Feedback → Filter | 0–100 | somme delays → entrée filtre | 109 ⚠ | 55 |
| 39 | Output Hack | 0–14 | | | 55 |
| 40 | LFO 1 Frequency | 0–160 | 0–150 libre ; 151–160 sync (÷32, ÷16, ÷8, ÷4, ÷2, 1/pas, 2/pas, 4/pas, 8/pas, 16/pas) | | 56 |
| 41 | LFO 1 Shape | 0–4 | Tri, Rev Saw, Saw, Pulse, Random | | 56 |
| 42 | LFO 1 Amount | 0–200 | > 100 = key sync | | 56 |
| 43 | LFO 1 Destination | 0–68 | table §6 | | 56 |
| 44–47 | LFO 2 Freq / Shape / Amount / Dest | idem LFO 1 | | | 56 |
| 48 | Env 3 Amount | 0–198 | −99…+99 | | 56 |
| 49 | Env 3 Destination | 0–68 | table §6 | | 56 |
| 50 | Env 3 Attack | 0–110 | | | 56 |
| 51 | Env 3 Decay | 0–110 | | | 56 |
| 52 | Env 3 Sustain | 0–100 | | | 56 |
| 53 | Env 3 Release | 0–110 | | | 56 |
| 54 | Trigger Select | 0–13 | All, Seq, MIDI, MIDI Reset, Combo, Combo Reset, Ext Gate Env, Ext Gate Env+Reset, Ext Gate Seq, Ext Gate Seq+Reset, MIDI Seq Once, MIDI Seq Once Reset, Ext Trig Seq Once, Key Step | | 56–57 |
| 55 | Key Off / Transpose | 0–73 | 0 off, 1 = −36, 37 = 0, 73 = +36 | | 57 |
| 56–59 | Seq 1–4 Destination | 0–75 | table §6 (69–75 réservés séquenceur) | | 57 |
| 60 | Noise Volume | 0–100 | | 62 | 57 |
| 61 | Ext Input Volume | 0–100 | | | 57 |
| 62 | Ext Input Mode | 0–2 ⚠ | 0 Stereo, 1 Left→both, 2 Right→both, 3 L audio/R control | | 57 |
| 63 | Input Hack | 0–14 | | | 57 |
| 64 | Glide Osc 1 | 0–200 | 0–100 ; 101–199 fingered ⚠ ; 200 osc MIDI off | | 57 |
| 65 | Sync 2→1 | 0–1 | | | 57 |
| 66 | Program Tempo | 30–250 | BPM | | 57 |
| 67 | Program Clock Divide | 0–12 | Half, Quarter, 8th, 8th ½ swing, 8th swing, 8th trip, 16th, 16th ½ swing, 16th swing, 16th trip, 32nd, 32nd trip, 64th trip | | 57 (liste p.53) |
| 68 | Glide Osc 2 | 0–200 | comme #64 | | 57 |
| 69 | Osc Slop | 0–5 | | | 57 |
| 70 | Pitch Bend Range | 0–12 | demi-tons | | 57 |
| 71 | Key Mode | 0–5 | Low, Low retrig, High, High retrig, Last, Last retrig | | 57 |
| 72 | Glide Osc 3 | 0–200 | comme #64 | | 57 |
| 73 | FM 4→3 | 0–100 | | 40 | 57 |
| 74 | Shape Mod Osc 3 | 0–4 | Off, Seq 1–4 | | 57 |
| 75 | Ring Mod 4→3 | 0–100 | | 42 | 57 |
| 76 | Glide Osc 4 | 0–200 | comme #64 | | 57 |
| 77 | FM 3→4 | 0–100 | | 41 | 57 |
| 78 | Shape Mod Osc 4 | 0–4 | Off, Seq 1–4 | | 57 |
| 79 | Ring Mod 3→4 | 0–100 | | 43 | 57 |
| 80 | 2/4 Pole | 0–1 | 0 = 2 pôles, 1 = 4 pôles | | 58 |
| 81 | Filter Env Velocity | 0–100 | | | 58 |
| 82 | Filter Audio Mod | 0–100 | | 59 | 58 |
| 83 | Filter Split | 0–100 | | 60 | 58 |
| 84 | Highpass Cutoff | 0–199 | 0–99 sur la sortie ; 100–199 = 0–99 sur l'entrée | 13 | 58 |
| 85 | Mod 1 Source | 0–24 | table §7 | | 58 |
| 86 | Mod 1 Amount | 0–198 | −99…+99 | | 58 |
| 87 | Mod 1 Destination | 0–68 | table §6 | | 58 |
| 88 | Env Shape | 0–1 | 0 exponentiel, 1 linéaire | | 58 |
| 89 | VCA Env Velocity | 0–100 | | | 58 |
| 90–92 | Mod 2 Source / Amount / Dest | idem Mod 1 | | | 58 |
| 93–95 | Mod 3 Source / Amount / Dest | idem Mod 1 | | | 58 |
| 96–98 | Mod 4 Source / Amount / Dest | idem Mod 1 | | | 58 |
| 99 | Delay 2 Time | 0–166 | | 103 | 58 |
| 100 | Delay 2 Level | 0–100 | | 106 | 58 |
| 101 | Delay 3 Time | 0–166 | | 104 | 58 |
| 102 | Delay 3 Level | 0–100 | | 107 | 58 |
| 103 | Distortion | 0–199 | 0–99 sortie ; 100–199 = 0–99 entrée | 12 | 58 |
| 104–107 | LFO 3 Freq / Shape / Amount / Dest | idem LFO 1 | | | 58 |
| 108–111 | LFO 4 Freq / Shape / Amount / Dest | idem LFO 1 | | | 58 |
| 112 | Env 3 Delay | 0–100 | | | 59 |
| 113 | Env 3 Velocity | 0–100 | | | 59 |
| 114 | Ext In Peak Amount | 0–198 | −99…+99 | | 59 |
| 115 | Ext In Peak Dest | 0–68 | | | 59 |
| 116 | Ext In Env Follower Amount | 0–198 | −99…+99 | | 59 |
| 117 | Ext In Env Follower Dest | 0–68 | | | 59 |
| 118 | Velocity Amount | 0–198 | −99…+99 | | 59 |
| 119 | Velocity Dest | 0–68 | | | 59 |
| 120 | Mod Wheel Amount | 0–198 | −99…+99 | | 59 |
| 121 | Mod Wheel Dest | 0–68 | | | 59 |
| 122 | Pressure Amount | 0–198 | −99…+99 | | 59 |
| 123 | Pressure Dest | 0–68 | | | 59 |
| 124 | Breath Amount | 0–198 | −99…+99 | | 59 |
| 125 | Breath Dest | 0–68 | | | 59 |
| 126 | Foot Amount | 0–198 | −99…+99 | | 59 |
| 127 | Foot Dest | 0–68 | | | 59 |

⚠ CC 108/109 sont nommés « Delay Feedback 1 / 2 » (p.45) : la correspondance avec #37/#38 est une
déduction (même ordre), non affirmée par le manuel.
⚠ #62 : plage annoncée 0–2 mais 4 valeurs décrites (0–3). EvoCtrl respecte la plage annoncée (0–2).
⚠ #64/68/72/76 : la face avant affiche `F02…F99` pour le mode fingered (p.20–22) alors que la table donne
101–199 ; correspondance exacte inconnue → affiché `Fing <v−100>`.

---

## 6. Destinations de modulation (p.60–61)

0 None · 1–4 Osc 1–4 Freq · 5 Osc All Freq · 6–9 Osc 1–4 Level · 10 Osc All Level · 11 Noise Level ·
12 Ext In Level · 13 Osc 1 PW · 14 Osc 2 PW · 15 Osc All PW · 16 FM 4→3 · 17 FM 3→4 · 18 RM 4→3 ·
19 RM 3→4 · 20 Filter Freq · 21 Filter Split · 22 Resonance · 23 Highpass Freq · 24 VCA Amount · 25 Pan ·
26 Feedback Freq · 27 Feedback Amount · 28–30 Delay Time 1–3 · 31 Delay Time All · 32–34 Delay Amount 1–3 ·
35 Delay Amount All · 36–37 Delay Feedback 1–2 · 38–41 LFO 1–4 Freq · 42 LFO All Freq · 43–46 LFO 1–4 Amount ·
47 LFO All Amount · 48–50 Env 1–3 Amount · 51 Env All Amount · 52–54 Env 1–3 Attack · 55 Env All Attack ·
56–58 Env 1–3 Decay · 59 Env All Decay · 60–62 Env 1–3 Release · 63 Env All Release · 64 Filter 1 (L) Cutoff ·
65 Filter 2 (R) Cutoff · 66 Filter 1 (L) Reso · 67 Filter 2 (R) Reso · 68 Distortion.

Séquenceur uniquement : 69 Clock multiplier · 70 MIDI Note Out · 71 MIDI Velocity Out · 72 MIDI Mod Wheel Out ·
73 MIDI Pressure Out · 74 MIDI Breath Out · 75 MIDI Foot Out.

## 7. Sources de modulation (p.62)

0 None · 1–4 Seq 1–4 · 5–8 LFO 1–4 · 9 Filter Env · 10 Amp Env · 11 Env 3 · 12 Ext In Peak ·
13 Ext In Env Follower · 14 Pitch Bend · 15 Mod Wheel · 16 Pressure · 17 Breath · 18 Foot · 19 Velocity ·
20 Note Number · 21 Expression · 22 Noise · 23 Osc 3 · 24 Osc 4.

---

## 8. Autres incohérences relevées dans le manuel

- p.41 : *MIDI Xmit* `par` = « Only MIDI parameters **received** » (typo ; p.17 dit « sent »).
- p.17 : *MIDI Dump* `ALL` = « all programs in all **three** banks » alors qu'il y a 4 banques (0–3, p.48).
- p.52 : *Request Main Parameter Dump* : « will respond by sending out the current **edit buffer** in the
  format described in Main Parameter Dump » (copier-coller ; on attend un `0x0F`).
