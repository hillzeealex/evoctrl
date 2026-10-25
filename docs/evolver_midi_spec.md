# Evolver desktop MIDI specification (EvoCtrl's reference)

Single source: **Evolver Operation Manual v3.2a (July 2010), Dave Smith Instruments**, the manual for the mono
*desktop* Evolver. `p.NN` refers to the printed page numbers. Nothing in this file is made up: anything the manual
does not state is marked **⚠ UNCLEAR / TODO**.

---

## 0. No NRPN

The *MIDI Implementation* chapter (p.41-62) **documents no NRPN at all** (no mention of CC 99/98/6/38). The desktop
Evolver exchanges its program parameters **in SysEx only**:

- editing one parameter: the *Program Parameter* SysEx message (§2);
- sending knob moves: "If enabled Evolver will send out changes for all 128 Program Parameters in System Exclusive
  (SysEx) format" (p.17, *MIDI Xmit* setting).

What this means for EvoCtrl:

| Originally planned | Implemented (as per the manual) |
|---|---|
| NRPN CC 99/98/6/38 | *Program Parameter* SysEx (9 bytes), through the same coalesced, rate-limited send queue |
| "Via Ableton" fallback in NRPN | Fallback in **CC** for the ~40 parameters with a documented CC (p.44-45). Other parameters do **not** go through Ableton, as Live does not pass the SysEx on. |

⚠ **UNCLEAR / TODO (CC)**: the manual lists the CCs it receives but **does not give the scaling** from 0-127 to the
parameter's range (Filter Frequency is 0-164, for instance). EvoCtrl scales linearly, `cc = round(v * 127 / max)`,
which still needs checking on the hardware. The fallback mode is therefore marked *experimental*.

---

## 1. Global settings required on the Evolver (MAIN menu, p.16-17, p.41, p.53-54)

| Setting (Main param #) | Required value | Why |
|---|---|---|
| **MIDI Rec** (#12) | `ALL` (1) | To receive dumps and SysEx parameters. `par` (3) is enough for parameters, but its effect on dumps is not documented ⚠. |
| **MIDI Xmit** (#13) | `ALL` (1) or `par` (3) | To send knob moves as SysEx (p.17). |
| **MIDI Channel** (#14) | `ALL` (0), or 1-16 matching EvoCtrl | Evolver SysEx messages carry no channel (only *Device Inquiry* does). The channel is used for the fallback CCs and for Program Change. |
| **Poly Chain** (#9) | `OFF` (0) | Otherwise the Evolver echoes the MIDI it receives, which causes loops. |

The v3.2a manual does **not** mention a "MIDI SysEx On/Off" setting (other DSI instruments have one). There is
nothing else to switch on.

Main Parameters (p.53-54), for reference: 0 Program 0-127 · 1 Bank 0-3 · 2 Master Volume 0-100 ·
3 Master Transpose 0-72 (36 = 0) · 4 BPM 30-250 · 5 Clock Divide 0-12 · 6 Use Program Tempo 0-1 ·
7 MIDI Clock 0-6 · 8 Lock Sequence 0-1 · 9 Poly Chain 0-2 · 10 Input Gain 0-8 · 11 Master Fine Tune 0-100 ·
12 MIDI Receive 0-3 · 13 MIDI Transmit 0-3 · 14 MIDI Channel 0-16.

⚠ UNCLEAR: the *Main Parameter Data Dump* is described as "32 nibbles for 16 Main parameters" (p.49), yet only 0-14
are listed and "Parameter 15 not available" (p.54). EvoCtrl reads and writes 0-14 only.

---

## 2. Common SysEx format (p.47-52)

```
F0  01  20  01  <cmd> [data...]  F7
    │   │   └── File Version = 1
    │   └────── Evolver ID = 0x20
    └────────── DSI ID = 0x01
```

| cmd | Message | Data | Direction | Page |
|---|---|---|---|---|
| `0x01` | Program Parameter | `pp` (0-127), `ls nibble`, `ms nibble` | ↔ | p.47 |
| `0x08` | Sequence Parameter | `step` (0-63), `ls nibble`, `ms nibble` | ↔ | p.47 |
| `0x09` | Main Parameter | `pp` (0-14), `ls nibble`, `ms nibble` | ↔ | p.47 |
| `0x02` | Program Data Dump | `bank` 0-3, `prog` 0-127, 220 packed bytes | ↔ (on receipt it is **written** to that slot, p.17) | p.48 |
| `0x03` | Edit Buffer Data Dump | 220 packed bytes | ↔ | p.48 |
| `0x0A` | Waveshape Data Dump | `wave` 0-127, 293 packed bytes (only 96-127 are writable) | ↔ | p.48 |
| `0x11` | Program Name Data Dump | `bank`, `prog`, 16 bytes of 7-bit ASCII | ↔ | p.49 |
| `0x0F` | Main Parameter Data Dump | 32 nibbles | ↔ | p.49 |
| `0x12` | Start/Stop button | none | → | p.49 |
| `0x04` | Reset button | none | → | p.49 |
| `0x13` / `0x14` | Shift button On / Off | none | → | p.50 |
| `0x05` | Request Program Dump | `bank`, `prog` | → (reply `0x02`) | p.51 |
| `0x06` | Request Edit Buffer Dump | none | → (reply `0x03`) | p.51 |
| `0x0B` | Request Waveshape Dump | `wave` | → (reply `0x0A`) | p.51 |
| `0x0E` | Request Main Parameter Dump | none | → (reply `0x0F`) | p.52 |
| `0x10` | Request Program Name Dump | `bank`, `prog` | → (reply `0x11`) | p.52 |

Nibble values: `value = ls | (ms << 4)` (198 is sent as `06 0C`).

**Device Inquiry** (universal, p.46): `F0 7E <ch|7F> 06 01 F7`, answered with
`F0 7E <ch|7F> 06 02 01 20 00 00 00 <PIC ver> 00 <DSP ver> 00 F7`.

### 2.1 What EvoCtrl uses

| Feature | Messages |
|---|---|
| Test / Get from Evolver | sends `0x0E` and `0x06`, waits for `0x0F` (current bank and program, Main params #1 and #0, p.53) and `0x03`, then asks for the name (`0x10`) |
| Program changed on the Evolver | a Program Change arrives (sent when *MIDI Xmit* is ALL or pro, p.17) and triggers the same sequence as Get. ⚠ Whether the Evolver sends a Bank Select (CC 32) is not documented, so the bank comes from `0x0F`. |
| Send to Evolver | sends `0x03` (the whole edit buffer) |
| Real-time editing | `0x01` (parameter) and `0x08` (sequencer step) |
| Read a bank slot | `0x05` (plus `0x10` for the name), answered with `0x02` (plus `0x11`) |
| Write a bank slot | `0x02` with the target bank and program (plus `0x11` for the name ⚠: the format is documented, but not what the Evolver does with it on receipt) |
| Switch program | Main params 1 (bank) then 0 (program) with `0x09`, then Get (verified on hardware) |
| Globals (SETUP page) | `0x0E` to read, `0x09` to write |
| Waveshapes (WAVES page) | `0x0B` per wave, answered with `0x0A`; `0x0A` to write wave 97-128 |
| Sequencer transport | `0x12` (Start/Stop) and `0x04` (Reset) |

⚠ The program name is **not part** of the program or edit buffer dump (192 bytes = 128 parameters + 64 steps).

---

## 3. Packed MS bit format (p.50)

Groups of 8 MIDI bytes: one byte holding the top bits, followed by 7 data bytes (low 7 bits). Byte 0 of a group:
`bit i` is bit 7 of data byte `i` in that group (data byte 0 = A goes to bit 0).

```
Input  A B C D E F G           Output  [0 G7 F7 E7 D7 C7 B7 A7] [A&7F] [B&7F] ... [G&7F]
```

- The last group may be **partial**: header plus the N remaining bytes (worked out from the stated sizes:
  192 → 27×8 + (1+3) = **220**, 256 → 36×8 + (1+4) = **293** ✓).
- ⚠ Typo in the manual: line 6 of the example reads `F7 E6 E5...` instead of `F7 F6 F5...`.

Contents of a program or edit buffer dump (192 bytes once unpacked):
- bytes 0-127: program parameters (index = parameter number below);
- bytes 128-191: 64 sequencer steps (0-15 seq 1, 16-31 seq 2, 32-47 seq 3, 48-63 seq 4, see p.47).

A waveshape dump unpacks to 256 bytes: 128 signed 16-bit points, least significant byte first.

---

## 4. 4 × 16 sequencer (p.18-19, p.47)

- Step values: **0-100**. 24 is one octave when the sequence is routed to an oscillator (a quarter tone per step).
- Above 100: `rst` (end of sequence), then `oFF` (silence, sequence 1 only).
  ⚠ **UNCLEAR / TODO**: the numeric values of `rst` and `oFF` are not documented (probably 101 and 102, not
  verified). EvoCtrl shows and edits 0-100 and keeps any value above 100 received from the Evolver as it is.
- Real-time editing: SysEx `0x08`. ⚠ The manual does not say whether the Evolver *sends* `0x08` when a knob is
  turned in sequencer mode; EvoCtrl handles it if it arrives.

---

## 5. Program parameters (p.54-59)

The **CC** column is the documented received CC (p.44-45), used only by the "via Ableton" fallback. The manual gives
no **default values**: EvoCtrl uses a neutral value (centre for bipolar and fine tune parameters, otherwise the
minimum). They have no effect on the synth until something is sent.

| # | Name | Range | Meaning | CC | Page |
|---|---|---|---|---|---|
| 0 | Osc 1 Frequency | 0-120 | semitones (10 octaves) | 20 | 54 |
| 1 | Osc 1 Fine Tune | 0-100 | 50 = 0, ±50 cents | | 54 |
| 2 | Osc 1 Shape | 0-102 | 0 Saw, 1 Tri, 2 Saw/Tri, 3-102 Pulse PW 0-99 | 28 | 54 |
| 3 | Osc 1 Level | 0-100 | | 24 | 54 |
| 4 | Osc 2 Frequency | 0-120 | semitones | 21 | 54 |
| 5 | Osc 2 Fine Tune | 0-100 | 50 = 0 | | 54 |
| 6 | Osc 2 Shape | 0-102 | as Osc 1 | 29 | 54 |
| 7 | Osc 2 Level | 0-100 | | 25 | 54 |
| 8 | Osc 3 Frequency | 0-120 | semitones | 22 | 54 |
| 9 | Osc 3 Fine Tune | 0-100 | 50 = 0 | | 54 |
| 10 | Osc 3 Shape | 0-127 | waveshape 1-128 | 30 | 54 |
| 11 | Osc 3 Level | 0-100 | | 26 | 54 |
| 12 | Osc 4 Frequency | 0-120 | semitones | 23 | 54 |
| 13 | Osc 4 Fine Tune | 0-100 | 50 = 0 | | 54-55 |
| 14 | Osc 4 Shape | 0-127 | waveshape 1-128 | 31 | 55 |
| 15 | Osc 4 Level | 0-100 | | 27 | 55 |
| 16 | Filter Frequency | 0-164 | semitones | 52 | 55 |
| 17 | Filter Env Amount | 0-198 | -99 to +99 | 54 | 55 |
| 18 | Filter Env Attack | 0-110 | | 55 | 55 |
| 19 | Filter Env Decay | 0-110 | | 56 | 55 |
| 20 | Filter Env Sustain | 0-100 | | 57 | 55 |
| 21 | Filter Env Release | 0-110 | | 58 | 55 |
| 22 | Resonance | 0-100 | | 53 | 55 |
| 23 | Filter Key Amount | 0-100 | | 61 | 55 |
| 24 | VCA Level | 0-100 | | | 55 |
| 25 | VCA Env Amount | 0-100 | | | 55 |
| 26 | VCA Env Attack | 0-110 | | 75 | 55 |
| 27 | VCA Env Decay | 0-110 | | 76 | 55 |
| 28 | VCA Env Sustain | 0-100 | | 77 | 55 |
| 29 | VCA Env Release | 0-110 | | 78 | 55 |
| 30 | Output Pan | 0-6 | 0 full stereo, 3 mono, 6 full reversed stereo | | 55 |
| 31 | Program Volume | 0-100 | | | 55 |
| 32 | Feedback Frequency | 0-48 | semitones | 85 | 55 |
| 33 | Feedback Amount | 0-100 | | 86 | 55 |
| 34 | Grunge | 0-1 | off/on | | 55 |
| 35 | Delay 1 Time | 0-166 | | 102 | 55 |
| 36 | Delay 1 Level | 0-100 | | 105 | 55 |
| 37 | Delay Feedback → Delay | 0-100 | sum of delays into the delay input | 108 ⚠ | 55 |
| 38 | Delay Feedback → Filter | 0-100 | sum of delays into the filter input | 109 ⚠ | 55 |
| 39 | Output Hack | 0-14 | | | 55 |
| 40 | LFO 1 Frequency | 0-160 | 0-150 free; 151-160 synced (÷32, ÷16, ÷8, ÷4, ÷2, 1/step, 2/step, 4/step, 8/step, 16/step) | | 56 |
| 41 | LFO 1 Shape | 0-4 | Tri, Rev Saw, Saw, Pulse, Random | | 56 |
| 42 | LFO 1 Amount | 0-200 | above 100 = key sync | | 56 |
| 43 | LFO 1 Destination | 0-68 | table §6 | | 56 |
| 44-47 | LFO 2 Freq / Shape / Amount / Dest | as LFO 1 | | | 56 |
| 48 | Env 3 Amount | 0-198 | -99 to +99 | | 56 |
| 49 | Env 3 Destination | 0-68 | table §6 | | 56 |
| 50 | Env 3 Attack | 0-110 | | | 56 |
| 51 | Env 3 Decay | 0-110 | | | 56 |
| 52 | Env 3 Sustain | 0-100 | | | 56 |
| 53 | Env 3 Release | 0-110 | | | 56 |
| 54 | Trigger Select | 0-13 | All, Seq, MIDI, MIDI Reset, Combo, Combo Reset, Ext Gate Env, Ext Gate Env+Reset, Ext Gate Seq, Ext Gate Seq+Reset, MIDI Seq Once, MIDI Seq Once Reset, Ext Trig Seq Once, Key Step | | 56-57 |
| 55 | Key Off / Transpose | 0-73 | 0 off, 1 = -36, 37 = 0, 73 = +36 | | 57 |
| 56-59 | Seq 1-4 Destination | 0-75 | table §6 (69-75 sequencer only) | | 57 |
| 60 | Noise Volume | 0-100 | | 62 | 57 |
| 61 | Ext Input Volume | 0-100 | | | 57 |
| 62 | Ext Input Mode | 0-2 ⚠ | 0 Stereo, 1 Left to both, 2 Right to both, 3 L audio / R control | | 57 |
| 63 | Input Hack | 0-14 | | | 57 |
| 64 | Glide Osc 1 | 0-200 | 0-100; 101-199 fingered ⚠; 200 osc MIDI off | | 57 |
| 65 | Sync 2→1 | 0-1 | | | 57 |
| 66 | Program Tempo | 30-250 | BPM | | 57 |
| 67 | Program Clock Divide | 0-12 | Half, Quarter, 8th, 8th half swing, 8th swing, 8th triplet, 16th, 16th half swing, 16th swing, 16th triplet, 32nd, 32nd triplet, 64th triplet | | 57 (list on p.53) |
| 68 | Glide Osc 2 | 0-200 | as #64 | | 57 |
| 69 | Osc Slop | 0-5 | | | 57 |
| 70 | Pitch Bend Range | 0-12 | semitones | | 57 |
| 71 | Key Mode | 0-5 | Low, Low retrig, High, High retrig, Last, Last retrig | | 57 |
| 72 | Glide Osc 3 | 0-200 | as #64 | | 57 |
| 73 | FM 4→3 | 0-100 | | 40 | 57 |
| 74 | Shape Mod Osc 3 | 0-4 | Off, Seq 1-4 | | 57 |
| 75 | Ring Mod 4→3 | 0-100 | | 42 | 57 |
| 76 | Glide Osc 4 | 0-200 | as #64 | | 57 |
| 77 | FM 3→4 | 0-100 | | 41 | 57 |
| 78 | Shape Mod Osc 4 | 0-4 | Off, Seq 1-4 | | 57 |
| 79 | Ring Mod 3→4 | 0-100 | | 43 | 57 |
| 80 | 2/4 Pole | 0-1 | 0 = 2 pole, 1 = 4 pole | | 58 |
| 81 | Filter Env Velocity | 0-100 | | | 58 |
| 82 | Filter Audio Mod | 0-100 | | 59 | 58 |
| 83 | Filter Split | 0-100 | | 60 | 58 |
| 84 | Highpass Cutoff | 0-199 | 0-99 on the output; 100-199 = 0-99 on the input | 13 | 58 |
| 85 | Mod 1 Source | 0-24 | table §7 | | 58 |
| 86 | Mod 1 Amount | 0-198 | -99 to +99 | | 58 |
| 87 | Mod 1 Destination | 0-68 | table §6 | | 58 |
| 88 | Env Shape | 0-1 | 0 exponential, 1 linear | | 58 |
| 89 | VCA Env Velocity | 0-100 | | | 58 |
| 90-92 | Mod 2 Source / Amount / Dest | as Mod 1 | | | 58 |
| 93-95 | Mod 3 Source / Amount / Dest | as Mod 1 | | | 58 |
| 96-98 | Mod 4 Source / Amount / Dest | as Mod 1 | | | 58 |
| 99 | Delay 2 Time | 0-166 | | 103 | 58 |
| 100 | Delay 2 Level | 0-100 | | 106 | 58 |
| 101 | Delay 3 Time | 0-166 | | 104 | 58 |
| 102 | Delay 3 Level | 0-100 | | 107 | 58 |
| 103 | Distortion | 0-199 | 0-99 output; 100-199 = 0-99 input | 12 | 58 |
| 104-107 | LFO 3 Freq / Shape / Amount / Dest | as LFO 1 | | | 58 |
| 108-111 | LFO 4 Freq / Shape / Amount / Dest | as LFO 1 | | | 58 |
| 112 | Env 3 Delay | 0-100 | | | 59 |
| 113 | Env 3 Velocity | 0-100 | | | 59 |
| 114 | Ext In Peak Amount | 0-198 | -99 to +99 | | 59 |
| 115 | Ext In Peak Dest | 0-68 | | | 59 |
| 116 | Ext In Env Follower Amount | 0-198 | -99 to +99 | | 59 |
| 117 | Ext In Env Follower Dest | 0-68 | | | 59 |
| 118 | Velocity Amount | 0-198 | -99 to +99 | | 59 |
| 119 | Velocity Dest | 0-68 | | | 59 |
| 120 | Mod Wheel Amount | 0-198 | -99 to +99 | | 59 |
| 121 | Mod Wheel Dest | 0-68 | | | 59 |
| 122 | Pressure Amount | 0-198 | -99 to +99 | | 59 |
| 123 | Pressure Dest | 0-68 | | | 59 |
| 124 | Breath Amount | 0-198 | -99 to +99 | | 59 |
| 125 | Breath Dest | 0-68 | | | 59 |
| 126 | Foot Amount | 0-198 | -99 to +99 | | 59 |
| 127 | Foot Dest | 0-68 | | | 59 |

⚠ CC 108/109 are named "Delay Feedback 1 / 2" (p.45). Mapping them to #37/#38 is an inference from the order; the
manual does not say so.
⚠ #62: the stated range is 0-2 but four values are described (0-3). EvoCtrl keeps to the stated range (0-2).
⚠ #64/68/72/76: the front panel shows `F02...F99` for fingered mode (p.20-22) while the table gives 101-199. The exact
mapping is unknown, so EvoCtrl shows `Fing <v-100>`.

---

## 6. Modulation destinations (p.60-61)

0 None · 1-4 Osc 1-4 Freq · 5 Osc All Freq · 6-9 Osc 1-4 Level · 10 Osc All Level · 11 Noise Level ·
12 Ext In Level · 13 Osc 1 PW · 14 Osc 2 PW · 15 Osc All PW · 16 FM 4→3 · 17 FM 3→4 · 18 RM 4→3 ·
19 RM 3→4 · 20 Filter Freq · 21 Filter Split · 22 Resonance · 23 Highpass Freq · 24 VCA Amount · 25 Pan ·
26 Feedback Freq · 27 Feedback Amount · 28-30 Delay Time 1-3 · 31 Delay Time All · 32-34 Delay Amount 1-3 ·
35 Delay Amount All · 36-37 Delay Feedback 1-2 · 38-41 LFO 1-4 Freq · 42 LFO All Freq · 43-46 LFO 1-4 Amount ·
47 LFO All Amount · 48-50 Env 1-3 Amount · 51 Env All Amount · 52-54 Env 1-3 Attack · 55 Env All Attack ·
56-58 Env 1-3 Decay · 59 Env All Decay · 60-62 Env 1-3 Release · 63 Env All Release · 64 Filter 1 (L) Cutoff ·
65 Filter 2 (R) Cutoff · 66 Filter 1 (L) Reso · 67 Filter 2 (R) Reso · 68 Distortion.

Sequencer only: 69 Clock multiplier · 70 MIDI Note Out · 71 MIDI Velocity Out · 72 MIDI Mod Wheel Out ·
73 MIDI Pressure Out · 74 MIDI Breath Out · 75 MIDI Foot Out.

## 7. Modulation sources (p.62)

0 None · 1-4 Seq 1-4 · 5-8 LFO 1-4 · 9 Filter Env · 10 Amp Env · 11 Env 3 · 12 Ext In Peak ·
13 Ext In Env Follower · 14 Pitch Bend · 15 Mod Wheel · 16 Pressure · 17 Breath · 18 Foot · 19 Velocity ·
20 Note Number · 21 Expression · 22 Noise · 23 Osc 3 · 24 Osc 4.

---

## 8. Hardware findings and errors in the manual

Checked on the hardware (Evolver behind a MOTU UltraLite-mk4, macOS):

- A SysEx split across several CoreMIDI packets is misread, and the parameter takes a wrong value (always 17 for
  the cutoff). JUCE 8 splits messages like this because it sends through the MIDI 2.0 (UMP) API, 6 bytes per packet.
  Sent as a single packet, the same message is read correctly, so on macOS EvoCtrl sends its SysEx as one packet
  through the classic CoreMIDI API.
- Messages less than about 4 ms apart are sometimes dropped (1 or 2 per burst at 1 ms, none at 4 ms).
- The Evolver echoes every Program and Sequence Parameter it receives.
- With **Poly Chain** not set to OFF, the Evolver repeats everything it receives on MIDI OUT, mixed into its own
  dumps, which then arrive truncated.

Errors in the manual:

- p.41: *MIDI Xmit* `par` = "Only MIDI parameters **received**" (typo; p.17 says "sent").
- p.17: *MIDI Dump* `ALL` = "all programs in all **three** banks", although there are four banks (0-3, p.48).
- p.52: *Request Main Parameter Dump*: "will respond by sending out the current **edit buffer** in the format
  described in Main Parameter Dump" (copy and paste; the reply is a `0x0F`).
