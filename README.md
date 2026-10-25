# EvoCtrl

Éditeur / contrôleur pour **Dave Smith Instruments Evolver Desktop** (mono), en plugin VST3 / AU + application
Standalone (JUCE 8, C++20). Les 128 paramètres programme sont exposés à l'hôte : automatisables, mappables
(MIDI Map, Configure) et enregistrés dans le set Ableton.

Spécification MIDI (source de vérité, avec pages du manuel) : [docs/evolver_midi_spec.md](docs/evolver_midi_spec.md).

> **Écart avec le brief initial : pas de NRPN.** Le manuel de l'Evolver desktop (v3.2a) ne documente aucun NRPN.
> Tout passe en **SysEx** (paramètre unique, dumps, séquenceur). Le fallback « via Ableton » utilise les **CC**
> documentés (~40 paramètres). Détails : spec §0.

## Build

Prérequis : CMake ≥ 3.22, un compilateur C++20 et Git (JUCE 8.0.12 est téléchargé par CMake).

### macOS (Xcode ou Command Line Tools)

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # ou -G Xcode
cmake --build build
ctest --test-dir build                               # tests SysEx / table de paramètres
```

Le build est **universel (arm64 + x86_64)**. C'est forcé dans `CMakeLists.txt` : un `cmake` Homebrew Intel tournant
sous Rosetta produirait sinon un plugin x86_64 que Live natif sur Apple Silicon ne charge pas.

### Windows (Visual Studio 2022)

```bat
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release
```

Artefacts : `build/EvoCtrl_artefacts/Release/{VST3,AU,Standalone}/`.

## Installation

| Format | macOS | Windows |
|---|---|---|
| VST3 | `~/Library/Audio/Plug-Ins/VST3/EvoCtrl.vst3` | `C:\Program Files\Common Files\VST3\EvoCtrl.vst3` |
| AU | `~/Library/Audio/Plug-Ins/Components/EvoCtrl.component` | n/a |

```sh
cp -R build/EvoCtrl_artefacts/Release/VST3/EvoCtrl.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -R build/EvoCtrl_artefacts/Release/AU/EvoCtrl.component ~/Library/Audio/Plug-Ins/Components/
killall -9 AudioComponentRegistrar   # force macOS à rescanner les AU
```

Dans Live : Préférences > Plug-Ins > activer VST3 / AU, puis *Rescan*. Le plugin est signé ad hoc (pas notarisé) :
c'est suffisant pour un usage local.

Au premier lancement, macOS demande l'accès au dossier **Documents** : la bibliothèque de patchs est dans
`~/Documents/EvoCtrl/Library` (un `.syx` par patch + `library.json` pour les tags et les favoris). Il faut accepter.

## Réglages sur l'Evolver (menu Main, p.16–17 du manuel)

| Réglage | Valeur | Pourquoi |
|---|---|---|
| MIDI Rec | `ALL` | reçoit dumps + paramètres SysEx |
| MIDI Xmit | `ALL` | envoie les mouvements de potards en SysEx (synchro bidirectionnelle) |
| MIDI Channel | `ALL`, ou le canal choisi dans EvoCtrl | ne sert qu'au fallback CC (le SysEx Evolver n'a pas de canal) |
| Poly Chain | `OFF` | évite l'écho MIDI (boucles) |

Le manuel v3.2a ne mentionne pas de réglage « SysEx On/Off » : il n'y a rien d'autre à activer.

## Les deux chemins MIDI

1. **Ports directs (par défaut, recommandé).** EvoCtrl ouvre lui-même l'entrée et la sortie MIDI de l'Evolver
   (barre du haut : *In*, *Out*). Ça couvre tout : paramètres, dumps, lecture/écriture de programmes, séquenceur,
   retour des potards. Raison : Live ne transmet pas de façon fiable le SysEx émis par un plugin.
   Les envois passent par une file coalescée (un paramètre bougé 100 fois n'est envoyé qu'avec sa dernière valeur),
   avec un délai réglable entre messages (Setup > *Min. interval*, 1 ms par défaut).
2. **Via Ableton (expérimental).** Les mouvements partent en **CC** sur la sortie MIDI du plugin. À router depuis une
   autre piste MIDI : *MIDI From* = piste EvoCtrl > EvoCtrl, *MIDI To* = port de l'Evolver. Seuls les paramètres qui
   ont un CC documenté passent. La mise à l'échelle 0–127 est linéaire et **pas encore vérifiée** sur le matériel.
   Les dumps et Get/Send exigent quand même les ports directs.

Anti-boucle : une valeur reçue de l'Evolver met à jour le paramètre hôte sans être renvoyée au synthé.

## Réglages Ableton Live 12

Préférences > Link, Tempo & MIDI, lignes des ports de l'Evolver :

- **Remote : OFF** (entrée et sortie). Ça évite que des mappings MIDI de Live pilotent l'Evolver en parallèle
  d'EvoCtrl (double routage).
- **macOS :** *Track* peut rester **ON** sur la sortie pour jouer les notes depuis une piste MIDI (CoreMIDI partage
  les ports). Mettre *Track* **OFF** sur l'entrée si tu ne veux pas que les mouvements de potards (SysEx) arrivent
  aussi dans Live.
- **Windows :** un port MIDI ne peut être ouvert que par une seule application. Si EvoCtrl affiche
  « Cannot open MIDI … », désactive *Track* et *Remote* pour ce port (entrée **et** sortie), puis *Rescan* dans
  EvoCtrl. Pour jouer les notes en même temps, il faut une interface à plusieurs ports, un driver multi-client
  ou le mode *Via Ableton*.

Pour jouer les notes : une piste MIDI séparée (ou un *External Instrument*) vers le port de l'Evolver. EvoCtrl ne
gère que le son.

## Utilisation

- **Barre du haut :** ports, canal, *Test* (demande l'edit buffer et vérifie la réponse, sans rien modifier),
  *Get from Evolver* (remplace le patch de l'éditeur), *Send to Evolver* (envoie tout le patch dans l'edit buffer).
  La pastille donne l'état : gris = inactif, jaune = en attente, vert = OK, rouge = erreur (survol = message complet).
- **Synth :** toutes les sections. Double-clic = valeur par défaut, Cmd/Ctrl-glisser = réglage fin, molette OK.
  Le survol d'un potard affiche le numéro de paramètre, la plage brute et le CC éventuel.
- **Sequencer :** grille 4 × 16 (0–100) avec destinations, tempo, division et trigger. Les valeurs > 100 reçues
  (rst / off, codes non documentés) sont conservées et affichées en gris.
- **Patches :** nom (16 caractères ASCII), lecture/écriture banque + programme (l'écriture demande confirmation),
  import/export `.syx`, bibliothèque locale avec recherche, tags et favoris.
- **Setup :** chemin MIDI, intervalle d'envoi, option *renvoyer le patch au chargement du set*.
- **MIDI monitor :** log hexadécimal des messages envoyés et reçus sur les ports directs (à activer ; clock et
  active sensing masqués).

## Tests et validation

- `ctest` : pack/unpack (tailles du manuel 192→220 et 256→293, aller-retour, disposition des bits), encodage des
  messages, parse des dumps, rejet des messages étrangers ou tronqués, découpage `.syx`, et pour **chaque**
  paramètre : défaut dans la plage + texte affiché valide sur toute la plage.
- pluginval niveau 5 (VST3 et AU installé) :
  ```sh
  pluginval --strictness-level 5 --validate-in-process ~/Library/Audio/Plug-Ins/VST3/EvoCtrl.vst3
  ```

## Limites connues

- Pas de NRPN (absent du manuel) : SysEx en direct, CC en fallback.
- Fallback CC : mise à l'échelle non vérifiée, ~40 paramètres seulement.
- Nom de programme écrit via la commande `0x11` : format documenté, mais le manuel ne dit pas si l'Evolver le
  stocke à la réception.
- Codes numériques de `rst` / `off` du séquenceur non documentés (préservés tels quels).
- Glide « fingered » (101–199) : mapping exact incertain (spec §5).
- Main parameters (globaux de l'Evolver) non édités par le plugin (MVP).
- Les valeurs par défaut des paramètres sont des neutres choisis par EvoCtrl, pas des valeurs du manuel.
- V2 préparée mais pas codée : `evo::Patch` sert de type valeur pour undo/redo, A/B, morphing et randomize ;
  `EvoCtrlProcessor::applyPatch` / `capturePatch` en sont les points d'entrée.
