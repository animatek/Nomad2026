# Module Visual & Functional Checklist

Status guide:
- `[ ]` Not reviewed
- `[~]` Reviewed, has issues (details noted)
- `[x]` Reviewed, looks correct

For each module, check:
- **Layout**: Correct size, connectors/knobs/sliders in right positions
- **Graphics**: Custom displays rendering correctly (envelopes, waveforms, filters, etc.)
- **Buttons**: Waveform selectors and radio buttons showing correct icons
- **LEDs**: Signal indicator LEDs lighting up (green/yellow/red based on signal)
- **Meters**: VU/level meters showing activity
- **Labels**: Text labels and value displays correct
- **Parameters**: All knobs/sliders/buttons interactive and sending correct values

---

## In/Out (10)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 1 | Keyboard | [x] | [x] | [x] | [x] | Outputs=cuadrados, inputs=círculos, label Rel/vel multilínea centrado |
| 2 | AudioIn | [x] | [x] | [x] | [x] | VU meters animados (verde/amarillo/rojo), LEDs clip rojos activados por ledOnValue=127 |
| 3 | 4Output | [x] | [x] | [x] | [x] | OK |
| 4 | 2Output | [x] | [x] | [x] | [x] | Botones Destination y Mute con bevel OK |
| 5 | 1Output | [x] | [x] | [x] | [x] | Botones Dest y Mute con bevel OK |
| 63 | KeyboardPatch | [x] | [x] | [x] | [x] | Etiquetas multilínea posición corregida |
| 65 | MIDIGlobal | [x] | [x] | [x] | [x] | OK |
| 67 | NoteDetect | [x] | [x] | [x] | [x] | Display muestra nombre de nota (C4, D#3…) |
| 100 | KeybSplit | [x] | [x] | [x] | [x] | Lower y Upper muestran nombre de nota |
| 127 | PolyAreaIn | [x] | [x] | [x] | [x] | Labels L/R corregidas, escala dB entre VU en negro |

## Oscillator (16)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 7 | OscA | [x] | [x] | [x] | [x] | Knobs, waveform selector (icons), TextDisplay nota, mute, static icon, groupbox |
| 8 | OscB | [x] | [x] | [x] | [x] | Igual que OscA |
| 9 | OscC | [x] | [x] | [x] | [x] | Sine solo, groupbox ajustado, static icon, labels alineados |
| 10 | OscSlvB | [x] | [x] | [x] | [x] | Partial display 1:1, flechas ◄►, static icon wf_square |
| 11 | OscSlvC | [x] | [x] | [x] | [x] | Partial display, flechas, static icon wf_saw |
| 12 | OscSlvD | [x] | [x] | [x] | [x] | Partial display, flechas, static icon wf_tri |
| 13 | OscSlvE | [x] | [x] | [x] | [x] | Partial display, flechas, static icon wf_sine |
| 14 | OscSlvA | [x] | [x] | [x] | [x] | Partial display, flechas, waveform radio selector |
| 31 | Noise | [x] | [x] | [x] | [x] | Static icon wf_noise añadido (x=207 y=5) |
| 58 | DrumSynth | [x] | [x] | [x] | [x] | |
| 85 | OscSlvFM | [x] | [x] | [x] | [x] | Partial display, flechas, static icon wf_sine |
| 95 | PercOsc | [~] | [~] | [ ] | [ ] | Static icon wf_percosc añadido; Punch button reubicado (x=213 y=23 26×13) |
| 96 | FormantOsc | [~] | [~] | [ ] | [ ] | Static icon wf_formant añadido (x=238 y=2) |
| 97 | MasterOsc | [ ] | [ ] | [ ] | [ ] | Custom param |
| 106 | OscSineBank | [ ] | [ ] | [ ] | [ ] | |
| 107 | SpectralOsc | [~] | [~] | [ ] | [ ] | Static icon wf_spectral añadido; Partials button reducido (x=207 y=35 38×13) |

## LFO (14)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 24 | LFOA | [ ] | [ ] | [ ] | [ ] | LFO display implemented |
| 25 | LFOB | [ ] | [ ] | [ ] | [ ] | Square/Pulse |
| 26 | LFOC | [ ] | [ ] | [ ] | [ ] | Multiple LFO |
| 27 | LFOSlvB | [ ] | [ ] | [ ] | [ ] | Sawtooth slave, custom param |
| 28 | LFOSlvC | [ ] | [ ] | [ ] | [ ] | Sine slave, custom param |
| 29 | LFOSlvD | [ ] | [ ] | [ ] | [ ] | Square slave, custom param |
| 30 | LFOSlvE | [ ] | [ ] | [ ] | [ ] | Triangle slave, custom param |
| 33 | ClkRndGen | [ ] | [ ] | [ ] | [ ] | |
| 34 | RndStepGen | [ ] | [ ] | [ ] | [ ] | Custom param |
| 35 | RndPulsGen | [ ] | [ ] | [ ] | [ ] | |
| 68 | ClkGen | [ ] | [ ] | [ ] | [ ] | |
| 80 | LFOSlvA | [ ] | [ ] | [ ] | [ ] | Multiple slave, custom param |
| 99 | PatternGen | [ ] | [ ] | [ ] | [ ] | Step pattern display? |
| 110 | RandomGen | [ ] | [ ] | [ ] | [ ] | Custom param |

## Envelope (6)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 20 | ADSR | [ ] | [ ] | [ ] | [ ] | ADSR display implemented |
| 23 | Mod-Env | [ ] | [ ] | [ ] | [ ] | Modulated envelope |
| 46 | AHD | [ ] | [ ] | [ ] | [ ] | AHD display implemented |
| 52 | Multi-Env | [ ] | [ ] | [ ] | [ ] | Multi-env display implemented |
| 71 | EnvFollower | [ ] | [ ] | [ ] | [ ] | |
| 84 | AD-Env | [ ] | [ ] | [ ] | [ ] | AD display implemented |

## Filter (11)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 32 | FilterBank | [ ] | [ ] | [ ] | [ ] | 14-band, complex UI |
| 45 | VocalFilter | [ ] | [ ] | [ ] | [ ] | |
| 49 | FilterD | [ ] | [ ] | [ ] | [ ] | Dynamic multimode, custom param |
| 50 | FilterC | [ ] | [ ] | [ ] | [ ] | Static multimode, custom param |
| 51 | FilterE | [ ] | [ ] | [ ] | [ ] | Filter display implemented, custom param |
| 86 | FilterA | [ ] | [ ] | [ ] | [ ] | 6dB LP |
| 87 | FilterB | [ ] | [ ] | [ ] | [ ] | 6dB HP |
| 92 | FilterF | [ ] | [ ] | [ ] | [ ] | Filter display implemented, custom param |
| 103 | EqMid | [ ] | [ ] | [ ] | [ ] | EQ display implemented |
| 104 | EqShelving | [ ] | [ ] | [ ] | [ ] | EQ display implemented |
| 108 | Vocoder | [ ] | [ ] | [ ] | [ ] | 16-band, needs vocoder display? |

## Mixer (13)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 18 | X-Fade | [ ] | [ ] | [ ] | [ ] | |
| 19 | Mixer (3) | [ ] | [ ] | [ ] | [ ] | |
| 40 | Mixer (8) | [ ] | [ ] | [ ] | [ ] | |
| 44 | GainControl | [ ] | [ ] | [ ] | [ ] | LED trigger indicator |
| 47 | Pan | [ ] | [ ] | [ ] | [ ] | |
| 76 | OnOff | [ ] | [ ] | [ ] | [ ] | LED on/off state |
| 79 | 4-1Switch | [ ] | [ ] | [ ] | [ ] | |
| 81 | Amplifier | [ ] | [ ] | [ ] | [ ] | LED trigger indicator |
| 88 | 1-4Switch | [ ] | [ ] | [ ] | [ ] | |
| 111 | LevMult | [ ] | [ ] | [ ] | [ ] | |
| 112 | LevAdd | [ ] | [ ] | [ ] | [ ] | |
| 113 | 1to2Fade | [ ] | [ ] | [ ] | [ ] | |
| 114 | 2to1Fade | [ ] | [ ] | [ ] | [ ] | |

## Audio (15)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 21 | Compressor | [ ] | [ ] | [ ] | [ ] | Compressor display implemented |
| 53 | Sample&Hold | [ ] | [ ] | [ ] | [ ] | |
| 54 | Quantizer | [ ] | [ ] | [ ] | [ ] | |
| 57 | InvLevShift | [ ] | [ ] | [ ] | [ ] | |
| 61 | Clip | [ ] | [ ] | [ ] | [ ] | Clip display implemented |
| 62 | Overdrive | [ ] | [ ] | [ ] | [ ] | Overdrive display implemented |
| 74 | WaveWrap | [ ] | [ ] | [ ] | [ ] | WaveWrap display implemented |
| 78 | Delay | [ ] | [ ] | [ ] | [ ] | |
| 82 | Diode | [ ] | [ ] | [ ] | [ ] | |
| 83 | Shaper | [ ] | [ ] | [ ] | [ ] | |
| 94 | StereoChorus | [ ] | [ ] | [ ] | [ ] | |
| 102 | Phaser | [ ] | [ ] | [ ] | [ ] | Phaser display implemented |
| 105 | Expander | [ ] | [ ] | [ ] | [ ] | Expander display implemented |
| 117 | RingMod | [ ] | [ ] | [ ] | [ ] | |
| 118 | Digitizer | [ ] | [ ] | [ ] | [ ] | |

## Control (10)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 16 | PortamentoB | [ ] | [ ] | [ ] | [ ] | |
| 22 | PartialGen | [ ] | [ ] | [ ] | [ ] | |
| 39 | Smooth | [ ] | [ ] | [ ] | [ ] | |
| 43 | Constant | [ ] | [ ] | [ ] | [ ] | |
| 48 | PortamentoA | [ ] | [ ] | [ ] | [ ] | |
| 66 | ControlMixer | [ ] | [ ] | [ ] | [ ] | |
| 72 | NoteScaler | [ ] | [ ] | [ ] | [ ] | |
| 75 | NoteQuant | [ ] | [ ] | [ ] | [ ] | |
| 98 | KeyQuant | [ ] | [ ] | [ ] | [ ] | |
| 115 | NoteVelScal | [ ] | [ ] | [ ] | [ ] | |

## Logic (10)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 36 | PosEdgeDelay | [ ] | [ ] | [ ] | [ ] | |
| 37 | LogicDelay | [ ] | [ ] | [ ] | [ ] | |
| 38 | Pulse | [ ] | [ ] | [ ] | [ ] | |
| 59 | CompareLev | [ ] | [ ] | [ ] | [ ] | |
| 64 | NegEdgeDelay | [ ] | [ ] | [ ] | [ ] | |
| 69 | ClkDiv | [ ] | [ ] | [ ] | [ ] | |
| 70 | LogicInv | [ ] | [ ] | [ ] | [ ] | |
| 73 | LogicProc | [ ] | [ ] | [ ] | [ ] | |
| 77 | ClkDivFix | [ ] | [ ] | [ ] | [ ] | |
| 89 | CompareAB | [ ] | [ ] | [ ] | [ ] | |

## Sequencer (4)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 15 | NoteSeqA | [ ] | [ ] | [ ] | [ ] | Step display? |
| 17 | EventSeq | [ ] | [ ] | [ ] | [ ] | |
| 90 | NoteSeqB | [ ] | [ ] | [ ] | [ ] | Custom params (zoom, slider pos) |
| 91 | CtrlSeq | [ ] | [ ] | [ ] | [ ] | |

## Morph (1)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 0 | Morph | [ ] | [ ] | [ ] | [ ] | |

---

## Known Global Issues

### LEDs
- [x] LEDs driven by globalLightValues (NMInfo 0x39) — clip LEDs by ledOnValue
- [x] Green/yellow/red color based on signal level (meters)
- [ ] LED arrays not rendered as arrays

### Meters
- [x] VU meters animados verde/amarillo/rojo en tiempo real (NMInfo 0x3A)
- [x] Escala dB (0 a -30) entre barras cuando hay espacio

### Waveform Buttons
- [ ] Some oscillator/LFO waveform selector buttons may not show waveform icons
- [ ] Need to verify each waveform button renders its shape correctly

### Custom Displays Already Implemented
- [x] ADSR, AD, AHD, Multi-Env envelope displays
- [x] LFO waveform display (sine, triangle, saw, inv-saw, square)
- [x] Overdrive, Clip, WaveWrap distortion displays
- [x] FilterE, FilterF filter response displays
- [x] EqMid, EqShelving EQ displays
- [x] Compressor, Expander dynamics displays
- [x] Phaser display

### Custom Displays Missing
- [ ] Vocoder 16-band display
- [ ] Spectral Oscillator display
- [ ] Formant Oscillator display
- [ ] Step sequencer position indicators
