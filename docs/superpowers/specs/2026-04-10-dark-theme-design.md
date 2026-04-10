# Dark Theme System — Design Spec
**Date:** 2026-04-10  
**Status:** Approved, pending implementation

---

## Overview

Replace the 131 hardcoded color literals in `PatchCanvasComponent.cpp` with a `ColorScheme` struct. Ship two themes: **Classic** (current colors, dev reference) and **Dark** (new carbón palette). Selectable at runtime via View menu. Default: Dark.

Classic theme is a development safety net — once Dark is stable it will be removed in a separate commit.

---

## 1. Data Layer — `ColorScheme`

### Files
- `source/ui/ColorScheme.h` — struct definition + two `extern const` declarations
- `source/ui/ColorScheme.cpp` — `kClassicTheme` and `kDarkTheme` definitions

### Struct fields (~49 total)

```cpp
struct ColorScheme
{
    // Canvas
    juce::Colour gridBackground;      // Dark: #111111

    // Module panels
    juce::Colour moduleBg;            // Dark: #2D3033
    juce::Colour moduleBorder;        // Dark: #3a3d42
    juce::Colour moduleText;          // Dark: #ffffff
    juce::Colour moduleTextDim;       // Dark: #55585C
    juce::Colour groupBoxBorder;      // Dark: #3a3d42

    // Knobs (semi-flat gradient, option B)
    juce::Colour knobHighlight;       // Dark: #e8e8e8  (gradient top)
    juce::Colour knobMid;             // Dark: #d0d0d0  (gradient mid)
    juce::Colour knobBase;            // Dark: #b8b8b8  (gradient bottom)
    juce::Colour knobBorder;          // Dark: #55585C
    juce::Colour knobIndicator;       // Dark: #3a3a3a  (position line)
    juce::Colour knobRangeMark;       // Dark: #55585C  (two bottom delimiters)
    juce::Colour knobValueArc;        // Dark: #F37F15  (orange value ring)
    juce::Colour knobRangeArc;        // Dark: #3a3d42  (background arc)

    // Morph / Macro slots (4)
    juce::Colour morphColor[4];       // Dark: existing morph colors extracted

    // Text displays
    juce::Colour displayBg;           // Dark: #1a1a1a
    juce::Colour displayBorder;       // Dark: #55585C
    juce::Colour displayText;         // Dark: #F37F15
    juce::Colour displayTextAlt;      // Dark: #ffffff  (note names etc)

    // Buttons
    juce::Colour buttonBg;            // Dark: #3a3d42
    juce::Colour buttonBgActive;      // Dark: #55585C
    juce::Colour buttonText;          // Dark: #aaaaaa
    juce::Colour buttonTextActive;    // Dark: #ffffff
    juce::Colour buttonBorder;        // Dark: #55585C

    // Reset buttons
    juce::Colour resetBg;             // Dark: #2a2a2a
    juce::Colour resetBorder;         // Dark: #444444

    // Cables / Connectors (signal type colors — same both themes)
    juce::Colour cableAudio;          // #EE3B54
    juce::Colour cableControl;        // #EAB308
    juce::Colour cableSlave;          // #A8A8A8
    juce::Colour cableLogic;          // #2DDCA3
    juce::Colour cablePhase;          // #A855F7
    juce::Colour cableMasterSlave;    // #A8A8A8

    // Custom displays (envelopes, LFO, filter, etc.)
    juce::Colour displayCurvePrimary;   // Dark: #2DDCA3  (env, LFO)
    juce::Colour displayCurveSecondary; // Dark: #55aaff  (filter, LFO alt)
    juce::Colour displayCurveWarm;      // Dark: #ff8844  (overdrive, clip)
    juce::Colour displayCurveCool;      // Dark: #aa55cc  (phaser)
    juce::Colour displayCurveGreen;     // Dark: #88eeaa  (spectral)
    juce::Colour displayGrid;           // Dark: #333355  (reference lines)
    juce::Colour displayBgCustom;       // Dark: #1a1a2e  (custom display bg)
    juce::Colour displayBorderCustom;   // Dark: #444466

    // Static waveform icons (wf_* boxes)
    juce::Colour iconBg;              // Dark: rgba(0,0,0,0.55)
    juce::Colour iconBorder;          // Dark: rgba(255,255,255,0.2)
    juce::Colour iconFg;              // Dark: #ffffff

    // LEDs
    juce::Colour ledOn;               // Dark: #22cc44
    juce::Colour ledOff;              // Dark: #1a2a22
    juce::Colour ledAudioOn;          // Dark: #EE3B54
    juce::Colour ledYellow;           // Dark: #ffffdd44

    // Misc
    juce::Colour lockIcon;            // Dark: #C0A020
    juce::Colour selectionRect;       // Dark: #4444ff (rubber band)
    juce::Colour snapHighlight;       // Dark: #E5DE45 (parameter snap)
};

extern const ColorScheme kClassicTheme;
extern const ColorScheme kDarkTheme;
```

### Knob gradient — Dark (option B, semi-flat)
Gradient center: cx=45%, cy=38%. Color stops: `#e8e8e8` → `#d0d0d0` → `#b8b8b8`. Narrower range than Classic, less 3D bump.

---

## 2. Integration in PatchCanvas

### Changes to `PatchCanvas`
- Add member: `ColorScheme activeScheme_ = kDarkTheme;`
- Add public method: `void setTheme(const ColorScheme& cs)` — assigns and calls `repaint()`
- All `paint*` functions replace `juce::Colour(0xffXXXXXX)` literals with `activeScheme_.fieldName`
- No function signature changes

### Knob gradient change
`paintKnobs()` currently uses hardcoded radial gradient parameters. In Dark theme the `ColorScheme` provides the three color stops; the gradient center shifts to (45%, 38%) for the semi-flat look. Classic theme restores the original values.

---

## 3. Menu — View → Theme

In the existing `MenuBarModel`:
- Add submenu **View → Theme** with two items: `Classic` and `Dark`
- Active theme shows a checkmark
- On selection: `mainLayout->getPatchCanvas()->setTheme(kClassicTheme / kDarkTheme)`
- No persistence — always starts Dark on launch

---

## 4. Cross-platform notes

- `juce::Colour` is portable across Linux, macOS, Raspberry Pi — no platform-specific code
- `ColorScheme` is a plain struct with no heap allocation
- `kClassicTheme` and `kDarkTheme` are `const` globals — safe, no initialization order issues

---

## 5. Out of scope (this iteration)

- Persisting theme selection to `settings.json`
- User-defined / custom themes from external files
- Removing Classic theme (separate commit once Dark is stable)
- Theming the main window chrome / title bar

---

## 6. Migration strategy

1. Create `ColorScheme.h/.cpp` with Classic = exact current colors → app looks identical
2. Wire all 131 color literals to `activeScheme_` → verify Classic is pixel-perfect
3. Define Dark palette and add menu → switch and validate
4. (Later) remove Classic
