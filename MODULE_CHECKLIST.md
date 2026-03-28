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
| 1 | Keyboard | [ ] | [ ] | [ ] | [ ] | |
| 2 | AudioIn | [ ] | [ ] | [ ] | [ ] | Has VU meter input |
| 3 | 4Output | [ ] | [ ] | [ ] | [ ] | Has VU meters |
| 4 | 2Output | [ ] | [ ] | [ ] | [ ] | Has VU meters |
| 5 | 1Output | [ ] | [ ] | [ ] | [ ] | Has VU meter |
| 63 | KeyboardPatch | [ ] | [ ] | [ ] | [ ] | |
| 65 | MIDIGlobal | [ ] | [ ] | [ ] | [ ] | |
| 67 | NoteDetect | [ ] | [ ] | [ ] | [ ] | |
| 100 | KeybSplit | [ ] | [ ] | [ ] | [ ] | |
| 127 | PolyAreaIn | [ ] | [ ] | [ ] | [ ] | |

## Oscillator (16)

| # | Module | Layout | Graphics | LEDs/Meters | Buttons | Notes |
|---|--------|--------|----------|-------------|---------|-------|
| 7 | OscA | [ ] | [ ] | [ ] | [ ] | Waveform selector buttons, custom param |
| 8 | OscB | [ ] | [ ] | [ ] | [ ] | Waveform selector buttons, custom param |
| 9 | OscC | [ ] | [ ] | [ ] | [ ] | Sine oscillator |
| 10 | OscSlvB | [ ] | [ ] | [ ] | [ ] | Square/Pulse slave, custom param |
| 11 | OscSlvC | [ ] | [ ] | [ ] | [ ] | Sawtooth slave, custom param |
| 12 | OscSlvD | [ ] | [ ] | [ ] | [ ] | Triangle slave, custom param |
| 13 | OscSlvE | [ ] | [ ] | [ ] | [ ] | Sine slave, custom param |
| 14 | OscSlvA | [ ] | [ ] | [ ] | [ ] | Multiple slave, custom param |
| 31 | Noise | [ ] | [ ] | [ ] | [ ] | Noise generator |
| 58 | DrumSynth | [ ] | [ ] | [ ] | [ ] | |
| 85 | OscSlvFM | [ ] | [ ] | [ ] | [ ] | FM slave, custom param |
| 95 | PercOsc | [ ] | [ ] | [ ] | [ ] | Custom param |
| 96 | FormantOsc | [ ] | [ ] | [ ] | [ ] | Custom param, needs formant display? |
| 97 | MasterOsc | [ ] | [ ] | [ ] | [ ] | Custom param |
| 106 | OscSineBank | [ ] | [ ] | [ ] | [ ] | |
| 107 | SpectralOsc | [ ] | [ ] | [ ] | [ ] | Custom param, needs spectral display? |

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
- [ ] All LEDs render as static dark circles (off state only)
- [ ] No green/yellow/red color based on signal type
- [ ] No brightness modulation from signal values
- [ ] LED arrays not rendered as arrays

### Meters
- [ ] VU meters render as static dark rectangles
- [ ] No level animation or peak hold

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
