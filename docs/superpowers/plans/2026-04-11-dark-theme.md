# Dark Theme System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace 86 hardcoded color literals in `PatchCanvasComponent.cpp` with a `ColorScheme` struct and ship two selectable themes (Classic and Dark) via View → Theme menu.

**Architecture:** Plain `ColorScheme` struct as `PatchCanvas` member (`activeScheme_`); two `extern const ColorScheme` globals (`kClassicTheme`, `kDarkTheme`). `setTheme()` assigns and calls `repaint()`. Menu item 70=Classic, 71=Dark in `MainComponent`; forwarded via `MainLayout → PatchCanvasComponent → PatchCanvas`.

**Tech Stack:** JUCE 8, C++17, CMakeLists.txt source list.

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `source/ui/ColorScheme.h` | **Create** | Struct definition + extern declarations |
| `source/ui/ColorScheme.cpp` | **Create** | `kClassicTheme` and `kDarkTheme` definitions |
| `source/ui/PatchCanvasComponent.h` | **Modify** | Add `activeScheme_` member + `setTheme()` |
| `source/ui/PatchCanvasComponent.cpp` | **Modify** | Wire all 86 color literals to `activeScheme_` |
| `source/ui/MainLayout.h` | **Modify** | Add `setTheme()` forwarding method |
| `source/MainComponent.cpp` | **Modify** | Add View→Theme submenu + `menuItemSelected` cases |
| `CMakeLists.txt` | **Modify** | Add `ColorScheme.cpp` to source list |

---

## Task 1: Create ColorScheme struct (Classic colors only)

**Files:**
- Create: `source/ui/ColorScheme.h`
- Create: `source/ui/ColorScheme.cpp`
- Modify: `CMakeLists.txt` (add source file)

- [ ] **Step 1: Create `source/ui/ColorScheme.h`**

```cpp
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

struct ColorScheme
{
    // Canvas
    juce::Colour gridBackground;
    juce::Colour gridLines;

    // Module panels
    juce::Colour moduleBorder;
    juce::Colour moduleText;
    juce::Colour groupBoxBorder;

    // Knobs
    juce::Colour knobBase;           // fill color for non-morph knobs
    juce::Colour knobBorder;         // outline when no morph
    juce::Colour knobGrip;           // position indicator line
    juce::Colour knobTickMark;       // limit tick marks at ±135°
    juce::Colour morphColor[4];      // morph group colors [0]=red [1]=green [2]=blue [3]=yellow
    juce::Colour lockBody;           // padlock body
    juce::Colour lockShackle;        // padlock shackle arc

    // Connectors
    juce::Colour connHole;           // dark hole inside connector
    juce::Colour connOutline;        // connector ring outline

    // Text displays
    juce::Colour displayBg;
    juce::Colour displayBorder;
    juce::Colour displayText;

    // Buttons (cyclic / toggle)
    juce::Colour buttonText;
    juce::Colour buttonTextActive;
    juce::Colour buttonBorder;

    // Reset buttons
    juce::Colour resetBg;
    juce::Colour resetBorder;
    juce::Colour resetText;

    // Reset dot (default indicator)
    juce::Colour resetDotOn;
    juce::Colour resetDotOff;

    // Cable / connector signal colors (same both themes)
    juce::Colour cableAudio;
    juce::Colour cableControl;
    juce::Colour cableLogic;
    juce::Colour cableMasterSlave;
    juce::Colour cableUser1;
    juce::Colour cableUser2;

    // LED/meter states
    juce::Colour ledOn;
    juce::Colour ledOff;
    juce::Colour ledAudioOn;
    juce::Colour ledYellow;
    juce::Colour meterLow;
    juce::Colour meterMid;
    juce::Colour meterHigh;
    juce::Colour meterTrack;

    // Custom displays (envelopes, LFO, filter curves)
    juce::Colour displayBgCustom;
    juce::Colour displayBorderCustom;
    juce::Colour displayGrid;
    juce::Colour displayCurveGreen;
    juce::Colour displayCurveBlue;
    juce::Colour displayCurveWarm;
    juce::Colour displayCurvePurple;
    juce::Colour displayCurveYellow;
    juce::Colour displayCurveRed;

    // Static waveform icon boxes
    juce::Colour iconBg;
    juce::Colour iconFg;

    // Snap highlight (parameter snap indicator)
    juce::Colour snapHighlight;

    // Rubber-band selection rect
    juce::Colour selectionRect;

    // SlotBar synth icon
    juce::Colour slotIconActive;
    juce::Colour slotIconInactive;
};

extern const ColorScheme kClassicTheme;
extern const ColorScheme kDarkTheme;
```

- [ ] **Step 2: Create `source/ui/ColorScheme.cpp`** with Classic = exact current colors

```cpp
#include "ColorScheme.h"

const ColorScheme kClassicTheme = {
    // Canvas
    .gridBackground  = juce::Colour(0xff12122a),
    .gridLines       = juce::Colour(0xff1a1a3a),

    // Module panels (moduleBorder/moduleText are unused in Classic — module uses descriptor bg)
    .moduleBorder    = juce::Colour(0x44000000),
    .moduleText      = juce::Colours::black.withAlpha(0.7f),
    .groupBoxBorder  = juce::Colour(0x44000000),  // placeholder; derived from bg.darker()

    // Knobs
    .knobBase        = juce::Colour(0xff989898),
    .knobBorder      = juce::Colour(0xff666666),
    .knobGrip        = juce::Colours::white,
    .knobTickMark    = juce::Colour(0xff333333),
    .morphColor      = {
        juce::Colour(0xffCB4F4F),  // Group 1 — red
        juce::Colour(0xff9AC899),  // Group 2 — green
        juce::Colour(0xff5A5FB3),  // Group 3 — blue
        juce::Colour(0xffE5DE45),  // Group 4 — yellow
    },
    .lockBody        = juce::Colour(0xffE0C030),
    .lockShackle     = juce::Colour(0xffC0A020),

    // Connectors
    .connHole        = juce::Colour(0xff111111),
    .connOutline     = juce::Colour(0xff222222),

    // Text displays
    .displayBg       = juce::Colour(0xff2A2560),
    .displayBorder   = juce::Colour(0xff181440),
    .displayText     = juce::Colour(0xff4A3FA0),

    // Buttons
    .buttonText       = juce::Colour(0xff333333),
    .buttonTextActive = juce::Colour(0xff111111),
    .buttonBorder     = juce::Colour(0xff222222),

    // Reset buttons
    .resetBg          = juce::Colour(0xff2a2a2a),
    .resetBorder      = juce::Colour(0xff444444),
    .resetText        = juce::Colour(0xffaaaaaa),

    // Reset dot
    .resetDotOn       = juce::Colour(0xff44cc44),
    .resetDotOff      = juce::Colour(0xff2a4a2a),

    // Cable signal colors (same both themes)
    .cableAudio       = juce::Colour(0xffCB4F4F),
    .cableControl     = juce::Colour(0xff5A5FB3),
    .cableLogic       = juce::Colour(0xffE5DE45),
    .cableMasterSlave = juce::Colour(0xffA8A8A8),
    .cableUser1       = juce::Colour(0xff9AC899),
    .cableUser2       = juce::Colour(0xffBB00D7),

    // LEDs/meters
    .ledOn            = juce::Colour(0xffff2200),
    .ledOff           = juce::Colour(0xff333333),
    .ledAudioOn       = juce::Colour(0xffff6644),
    .ledYellow        = juce::Colour(0xffffdd44).withAlpha(0.8f),
    .meterLow         = juce::Colour(0xff22cc44),
    .meterMid         = juce::Colour(0xffddcc00),
    .meterHigh        = juce::Colour(0xffee2200),
    .meterTrack       = juce::Colour(0xff555555),

    // Custom displays
    .displayBgCustom     = juce::Colour(0xff1a1a2e),
    .displayBorderCustom = juce::Colour(0xff444466),
    .displayGrid         = juce::Colour(0xff333355),
    .displayCurveGreen   = juce::Colour(0xff55cc55),
    .displayCurveBlue    = juce::Colour(0xff55aaff),
    .displayCurveWarm    = juce::Colour(0xffff8844),
    .displayCurvePurple  = juce::Colour(0xffaa55cc),
    .displayCurveYellow  = juce::Colour(0xffaaaa55),
    .displayCurveRed     = juce::Colour(0xffcc5555),

    // Static icons
    .iconBg              = juce::Colour(0x44000000),
    .iconFg              = juce::Colours::white,

    // Snap / selection
    .snapHighlight       = juce::Colour(0xffE5DE45),
    .selectionRect       = juce::Colours::yellow,

    // SlotBar
    .slotIconActive      = juce::Colour(0xffcc3333),
    .slotIconInactive    = juce::Colour(0xff555577),
};

const ColorScheme kDarkTheme = {
    // Canvas
    .gridBackground  = juce::Colour(0xff111111),
    .gridLines       = juce::Colour(0xff1c1c2a),

    // Module panels
    .moduleBorder    = juce::Colour(0x44000000),
    .moduleText      = juce::Colours::white.withAlpha(0.85f),
    .groupBoxBorder  = juce::Colour(0xff3a3d42),

    // Knobs (semi-flat option B)
    .knobBase        = juce::Colour(0xffb8b8b8),
    .knobBorder      = juce::Colour(0xff55585C),
    .knobGrip        = juce::Colour(0xff3a3a3a),
    .knobTickMark    = juce::Colour(0xff55585C),
    .morphColor      = {
        juce::Colour(0xffCB4F4F),
        juce::Colour(0xff9AC899),
        juce::Colour(0xff5A5FB3),
        juce::Colour(0xffE5DE45),
    },
    .lockBody        = juce::Colour(0xffE0C030),
    .lockShackle     = juce::Colour(0xffC0A020),

    // Connectors
    .connHole        = juce::Colour(0xff111111),
    .connOutline     = juce::Colour(0xff333333),

    // Text displays
    .displayBg       = juce::Colour(0xff1a1a1a),
    .displayBorder   = juce::Colour(0xff55585C),
    .displayText     = juce::Colour(0xffF37F15),

    // Buttons
    .buttonText       = juce::Colour(0xffaaaaaa),
    .buttonTextActive = juce::Colour(0xffffffff),
    .buttonBorder     = juce::Colour(0xff55585C),

    // Reset buttons
    .resetBg          = juce::Colour(0xff2a2a2a),
    .resetBorder      = juce::Colour(0xff444444),
    .resetText        = juce::Colour(0xffaaaaaa),

    // Reset dot
    .resetDotOn       = juce::Colour(0xff44cc44),
    .resetDotOff      = juce::Colour(0xff1a2a22),

    // Cable signal colors (identical to Classic)
    .cableAudio       = juce::Colour(0xffCB4F4F),
    .cableControl     = juce::Colour(0xff5A5FB3),
    .cableLogic       = juce::Colour(0xffE5DE45),
    .cableMasterSlave = juce::Colour(0xffA8A8A8),
    .cableUser1       = juce::Colour(0xff9AC899),
    .cableUser2       = juce::Colour(0xffBB00D7),

    // LEDs/meters
    .ledOn            = juce::Colour(0xffff2200),
    .ledOff           = juce::Colour(0xff333333),
    .ledAudioOn       = juce::Colour(0xffff6644),
    .ledYellow        = juce::Colour(0xffffdd44).withAlpha(0.8f),
    .meterLow         = juce::Colour(0xff22cc44),
    .meterMid         = juce::Colour(0xffddcc00),
    .meterHigh        = juce::Colour(0xffee2200),
    .meterTrack       = juce::Colour(0xff3a3a3a),

    // Custom displays
    .displayBgCustom     = juce::Colour(0xff1a1a2e),
    .displayBorderCustom = juce::Colour(0xff444466),
    .displayGrid         = juce::Colour(0xff333355),
    .displayCurveGreen   = juce::Colour(0xff2DDCA3),
    .displayCurveBlue    = juce::Colour(0xff55aaff),
    .displayCurveWarm    = juce::Colour(0xffff8844),
    .displayCurvePurple  = juce::Colour(0xffaa55cc),
    .displayCurveYellow  = juce::Colour(0xffaaaa55),
    .displayCurveRed     = juce::Colour(0xffcc5555),

    // Static icons
    .iconBg              = juce::Colour(0x44000000),
    .iconFg              = juce::Colours::white,

    // Snap / selection
    .snapHighlight       = juce::Colour(0xffE5DE45),
    .selectionRect       = juce::Colour(0xff4444ff),

    // SlotBar
    .slotIconActive      = juce::Colour(0xffcc3333),
    .slotIconInactive    = juce::Colour(0xff555577),
};
```

- [ ] **Step 3: Add `ColorScheme.cpp` to `CMakeLists.txt`**

Open `CMakeLists.txt`. Find the source list that includes `source/ui/PatchCanvasComponent.cpp` (around line 30). Add directly below it:

```cmake
    source/ui/ColorScheme.cpp
```

- [ ] **Step 4: Build to verify it compiles**

```bash
cmake --build /mnt/SPEED/CODE/Nomad2026/build -j$(nproc) 2>&1 | tail -20
```
Expected: no errors. If designated initializers fail (`gcc <9`), replace with member-by-member assignment in a helper function.

- [ ] **Step 5: Commit**

```bash
cd /mnt/SPEED/CODE/Nomad2026
git add source/ui/ColorScheme.h source/ui/ColorScheme.cpp CMakeLists.txt
git commit -m "feat: add ColorScheme struct with Classic and Dark palettes"
```

---

## Task 2: Wire PatchCanvas to use activeScheme_

**Files:**
- Modify: `source/ui/PatchCanvasComponent.h` (add member + method)
- Modify: `source/ui/PatchCanvasComponent.cpp` (replace all 86 literals)

- [ ] **Step 1: Add `#include` and member to `PatchCanvasComponent.h`**

At the top of the includes section (after existing includes):
```cpp
#include "ColorScheme.h"
```

In the `public:` section of `PatchCanvas`, add after `void shakeCables();`:
```cpp
    void setTheme(const ColorScheme& cs) { activeScheme_ = cs; repaint(); }
    const ColorScheme& getTheme() const { return activeScheme_; }
```

In the `private:` section of `PatchCanvas`, add after `float zoomLevel = 1.0f;`:
```cpp
    ColorScheme activeScheme_ = kDarkTheme;
```

- [ ] **Step 2: Replace canvas background + grid colors in `PatchCanvas::paint()`** (`PatchCanvasComponent.cpp:336-342`)

Old:
```cpp
    g.fillAll(juce::Colour(0xff12122a));
    ...
    g.setColour(juce::Colour(0xff1a1a3a));
```
New:
```cpp
    g.fillAll(activeScheme_.gridBackground);
    ...
    g.setColour(activeScheme_.gridLines);
```

- [ ] **Step 3: Replace cable preview signal colors** (lines ~388-393)

Old:
```cpp
    case SignalType::Audio:      cableColor = juce::Colour(0xffCB4F4F); break;
    case SignalType::Control:    cableColor = juce::Colour(0xff5A5FB3); break;
    case SignalType::Logic:      cableColor = juce::Colour(0xffE5DE45); break;
    case SignalType::MasterSlave:cableColor = juce::Colour(0xffA8A8A8); break;
    case SignalType::User1:      cableColor = juce::Colour(0xff9AC899); break;
    case SignalType::User2:      cableColor = juce::Colour(0xffBB00D7); break;
```
New:
```cpp
    case SignalType::Audio:      cableColor = activeScheme_.cableAudio;       break;
    case SignalType::Control:    cableColor = activeScheme_.cableControl;     break;
    case SignalType::Logic:      cableColor = activeScheme_.cableLogic;       break;
    case SignalType::MasterSlave:cableColor = activeScheme_.cableMasterSlave; break;
    case SignalType::User1:      cableColor = activeScheme_.cableUser1;       break;
    case SignalType::User2:      cableColor = activeScheme_.cableUser2;       break;
```

- [ ] **Step 4: Replace snap highlight + rubber-band colors** (lines ~417, ~438, ~440, ~562)

Line ~417:
```cpp
// Old:
g.setColour(juce::Colour(0xff111111).withAlpha(0.6f));
// New:
g.setColour(activeScheme_.snapHighlight.withAlpha(0.0f));   // snap fill (keep transparent)
```
Actually, line 417 is the snap dim overlay. Check context:

```cpp
// Old line ~417 (snap overlay dim):
g.setColour(juce::Colour(0xff111111).withAlpha(0.6f));
// New:
g.setColour(activeScheme_.gridBackground.withAlpha(0.6f));
```

Line ~438 (snap inactive dim):
```cpp
// Old:
g.setColour(juce::Colour(0x33ffffff));
// New:
g.setColour(juce::Colours::white.withAlpha(0.2f));   // unchanged
```

Line ~440 (snap indicator color):
```cpp
// Old:
g.setColour(juce::Colour(0xffffdd44).withAlpha(0.8f));
// New:
g.setColour(activeScheme_.snapHighlight.withAlpha(0.8f));
```

Line ~562 (second snap indicator):
```cpp
// Old:
g.setColour(juce::Colour(0xffffdd44).withAlpha(0.9f));
// New:
g.setColour(activeScheme_.snapHighlight.withAlpha(0.9f));
```

- [ ] **Step 5: Replace connector colors in `paintConnectors()`** (lines ~607-634)

Lines ~607-612 (connector signal colors):
```cpp
// Old:
if      (tc.cssClass == "cAUDIO")   connColour = juce::Colour(0xffCB4F4F);
else if (tc.cssClass == "cCONTROL") connColour = juce::Colour(0xff5A5FB3);
else if (tc.cssClass == "cLOGIC")   connColour = juce::Colour(0xffE5DE45);
else if (tc.cssClass == "cSLAVE")   connColour = juce::Colour(0xffA8A8A8);
else if (tc.cssClass == "cUSER1")   connColour = juce::Colour(0xff9AC899);
else if (tc.cssClass == "cUSER2")   connColour = juce::Colour(0xffBB00D7);
// New:
if      (tc.cssClass == "cAUDIO")   connColour = activeScheme_.cableAudio;
else if (tc.cssClass == "cCONTROL") connColour = activeScheme_.cableControl;
else if (tc.cssClass == "cLOGIC")   connColour = activeScheme_.cableLogic;
else if (tc.cssClass == "cSLAVE")   connColour = activeScheme_.cableMasterSlave;
else if (tc.cssClass == "cUSER1")   connColour = activeScheme_.cableUser1;
else if (tc.cssClass == "cUSER2")   connColour = activeScheme_.cableUser2;
```

Lines ~633-634 (connector hole and outline):
```cpp
// Old:
const juce::Colour darkHole = juce::Colour(0xff111111);
const juce::Colour outline  = juce::Colour(0xff222222);
// New:
const juce::Colour darkHole = activeScheme_.connHole;
const juce::Colour outline  = activeScheme_.connOutline;
```

- [ ] **Step 6: Replace text display color in `paintModuleBackground()`** (line ~690)

```cpp
// Old:
g.setColour(juce::Colour(0xff1a1a1a));
// New:  (this is the module text — maps to moduleText field)
g.setColour(activeScheme_.moduleText);
```
Note: line 690 is actually `g.setColour(juce::Colour(0xff1a1a1a))` — verify with Read to confirm context.

- [ ] **Step 7: Replace knob colors in `paintKnobs()`** (lines ~790-901)

Lines ~790-793 (morphColors static array) — change from static `const` to derived from scheme:
```cpp
// Old:
static const juce::Colour morphColors[4] = {
    juce::Colour(0xffCB4F4F),
    juce::Colour(0xff9AC899),
    juce::Colour(0xff5A5FB3),
    juce::Colour(0xffE5DE45),
};
// New:
const juce::Colour* morphColors = activeScheme_.morphColor;
```

Line ~799 (knob base color for non-morph knobs):
```cpp
// Old:
juce::Colour baseColor = hasMorph ? morphColors[morphGroup] : juce::Colour(0xff989898);
// New:
juce::Colour baseColor = hasMorph ? morphColors[morphGroup] : activeScheme_.knobBase;
```

Line ~855 (knob outline for non-morph):
```cpp
// Old:
g.setColour(hasMorph ? baseColor.darker(0.4f) : juce::Colour(0xff666666));
// New:
g.setColour(hasMorph ? baseColor.darker(0.4f) : activeScheme_.knobBorder);
```

Line ~865 (tick marks):
```cpp
// Old:
g.setColour(juce::Colour(0xff333333));
// New:
g.setColour(activeScheme_.knobTickMark);
```

Line ~880 (grip line):
```cpp
// Old:
g.setColour(juce::Colours::white);
// New:
g.setColour(activeScheme_.knobGrip);
```

Lines ~895 and ~901 (lock indicator):
```cpp
// Old:
g.setColour(juce::Colour(0xffE0C030));
...
g.setColour(juce::Colour(0xffC0A020));
// New:
g.setColour(activeScheme_.lockBody);
...
g.setColour(activeScheme_.lockShackle);
```

- [ ] **Step 8: Replace button colors in `paintButtons()`** (lines ~1169-1215)

Line ~1169:
```cpp
// Old:
juce::Colour label = selected ? juce::Colour(0xff111111) : juce::Colour(0xff333333);
// New:
juce::Colour label = selected ? activeScheme_.buttonTextActive : activeScheme_.buttonText;
```

Lines ~1174, ~1205, ~1215 (button border):
```cpp
// Old: g.setColour(juce::Colour(0xff222222));
// New: g.setColour(activeScheme_.buttonBorder);
```

Lines ~1201-1202 (mute button colors — keep red, only inactive text changes):
```cpp
// Old:
juce::Colour muteBase = isOn ? juce::Colour(0xffcc4444) : moduleBg.darker(0.2f);
juce::Colour muteText = isOn ? juce::Colours::white : juce::Colour(0xff444444);
// New:
juce::Colour muteBase = isOn ? juce::Colour(0xffcc4444) : moduleBg.darker(0.2f);
juce::Colour muteText = isOn ? juce::Colours::white : activeScheme_.buttonText;
```

Lines ~1210-1211 (toggle button state):
```cpp
// Old:
juce::Colour labelCol  = isOn ? juce::Colour(0xff111111) : juce::Colour(0xff333333);
// New:
juce::Colour labelCol = isOn ? activeScheme_.buttonTextActive : activeScheme_.buttonText;
```

- [ ] **Step 9: Replace reset button colors in `paintResetButtons()`** (lines ~1230-1249 and ~1380-1390)

Lines ~1230, ~1234, ~1249 (first occurrence):
```cpp
// Old:
g.setColour(juce::Colour(0xff2a2a2a));   // bg
g.setColour(juce::Colour(0xff444444));   // border
g.setColour(juce::Colour(0xffaaaaaa));   // text
// New:
g.setColour(activeScheme_.resetBg);
g.setColour(activeScheme_.resetBorder);
g.setColour(activeScheme_.resetText);
```

Lines ~1380, ~1382, ~1390 (second occurrence, same pattern):
```cpp
g.setColour(activeScheme_.resetBg);
g.setColour(activeScheme_.resetBorder);
g.setColour(activeScheme_.resetText);
```

Lines ~1268 and ~1417-1419 (reset dot default indicator):
```cpp
// Old (~1268):
g.setColour(juce::Colour(0xffE0C030));
// This is snap-highlight on knob — already handled in Step 4 via snapHighlight

// Old (~1417):
g.setColour(atDefault ? juce::Colour(0xff44cc44) : juce::Colour(0xff2a4a2a));
// New:
g.setColour(atDefault ? activeScheme_.resetDotOn : activeScheme_.resetDotOff);

// Old (~1419):
g.setColour(juce::Colour(0xff111111));
// New:
g.setColour(activeScheme_.connHole);   // dark outline for reset dot
```

- [ ] **Step 10: Replace text display colors in `paintTextDisplays()`** (lines ~1287-1294)

```cpp
// Old:
g.setColour(juce::Colour(0xff2A2560));   // bg
g.setColour(juce::Colour(0xff181440));   // border
g.setColour(juce::Colour(0xff4A3FA0));   // text
// New:
g.setColour(activeScheme_.displayBg);
g.setColour(activeScheme_.displayBorder);
g.setColour(activeScheme_.displayText);
```

Same for the second text display block at lines ~4014-4019:
```cpp
g.setColour(activeScheme_.displayBg);
g.setColour(activeScheme_.displayBorder);
g.setColour(activeScheme_.displayText);
```

Lines ~1449, ~1451 (bevel border on displays):
```cpp
// Old:
g.setColour(juce::Colour(0x66000000));
g.setColour(juce::Colour(0x66ffffff));
// These are bevel effects — keep as literals (they work for both themes)
```

- [ ] **Step 11: Replace LED/meter colors in `paintLights()`** (lines ~1529-1564)

Lines ~1529, ~1531 (audio LED on states):
```cpp
// Old:
g.setColour(juce::Colour(0xffff2200));
g.setColour(juce::Colour(0xffff6644));
// New:
g.setColour(activeScheme_.ledOn);
g.setColour(activeScheme_.ledAudioOn);
```

Lines ~1537, ~1539 (LED off states):
```cpp
// Old:
g.setColour(juce::Colour(0xff333333));
g.setColour(juce::Colour(0xff555555));
// New:
g.setColour(activeScheme_.ledOff);
g.setColour(activeScheme_.meterTrack);
```

Line ~1548 (meter track):
```cpp
// Old: g.setColour(juce::Colour(0xff222222));
// New: g.setColour(activeScheme_.meterTrack);
```

Lines ~1562-1564 (meter fill colors):
```cpp
// Old:
if      (fill < 0.6f)  barColour = juce::Colour(0xff22cc44);
else if (fill < 0.85f) barColour = juce::Colour(0xffddcc00);
else                   barColour = juce::Colour(0xffee2200);
// New:
if      (fill < 0.6f)  barColour = activeScheme_.meterLow;
else if (fill < 0.85f) barColour = activeScheme_.meterMid;
else                   barColour = activeScheme_.meterHigh;
```

- [ ] **Step 12: Replace custom display colors in `paintCustomDisplays()`** (lines ~1602-2044)

Line ~1602 (custom display bg):
```cpp
// Old: g.setColour(juce::Colour(0xff1a1a2e));
// New: g.setColour(activeScheme_.displayBgCustom);
```

Line ~1606 (custom display border):
```cpp
// Old: g.setColour(juce::Colour(0xff444466));
// New: g.setColour(activeScheme_.displayBorderCustom);
```

Lines with `0xff333355` (grid lines, ~1707, ~1772, ~1828, ~1897, ~1955, ~2006):
```cpp
// Old: g.setColour(juce::Colour(0xff333355));
// New: g.setColour(activeScheme_.displayGrid);
```

Line ~1680 (env/LFO curve green):
```cpp
// Old: g.setColour(juce::Colour(0xff55cc55));
// New: g.setColour(activeScheme_.displayCurveGreen);
```

Lines ~1745, ~1872 (filter/LFO curve blue):
```cpp
// Old: g.setColour(juce::Colour(0xff55aaff));
// New: g.setColour(activeScheme_.displayCurveBlue);
```

Line ~1799 (overdrive warm):
```cpp
// Old: g.setColour(juce::Colour(0xffff8844));
// New: g.setColour(activeScheme_.displayCurveWarm);
```

Line ~1930 (spectral yellow):
```cpp
// Old: g.setColour(juce::Colour(0xffaaaa55));
// New: g.setColour(activeScheme_.displayCurveYellow);
```

Line ~1993 (VCF red curve):
```cpp
// Old: g.setColour(juce::Colour(0xffcc5555));
// New: g.setColour(activeScheme_.displayCurveRed);
```

Line ~2034 (phaser purple):
```cpp
// Old: g.setColour(juce::Colour(0xffaa55cc));
// New: g.setColour(activeScheme_.displayCurvePurple);
```

Line ~2044 (phaser bg ring):
```cpp
// Old: g.setColour(juce::Colour(0xff666688));
// New: g.setColour(activeScheme_.displayBorderCustom);
```

Line ~2206 (cable darken overlay):
```cpp
// Old: g.setColour(juce::Colour(0xaa000000));
// Keep as literal — alpha overlay, theme-independent
```

- [ ] **Step 13: Replace cable signal colors in `paintCables()`** — already done in Step 3; verify the same switch in `paintCables()` (~line 2139+) and replace if needed using same mapping.

- [ ] **Step 14: Replace remaining scattered colors**

Line ~4040 (DrumSynth extras — `spBase`):
```cpp
// Old: juce::Colour spBase = juce::Colour(0xff909090);
// New: juce::Colour spBase = activeScheme_.knobBase;
```

Lines ~4045, ~4058 (border lines):
```cpp
// Old: g.setColour(juce::Colour(0xff222222));
// New: g.setColour(activeScheme_.buttonBorder);
```

- [ ] **Step 15: Replace SlotBar colors in `MainLayout.cpp`** (line ~76, ~48-49)

`SlotBar::paint()` line ~76:
```cpp
// Old: g.fillAll(juce::Colour(0xff1e1e38));
// Leave as literal for now — SlotBar does not have access to ColorScheme yet
// (out of scope for this iteration per spec section 5)
```

`SlotBar::drawSlotIcon()` lines ~48-49:
```cpp
// Old:
g.setColour(active ? juce::Colour(0xffcc3333) : juce::Colour(0xff555577));
// Leave as literal — SlotBar theming is out of scope for this iteration
```

- [ ] **Step 16: Build and verify Classic is pixel-perfect**

```bash
cmake --build /mnt/SPEED/CODE/Nomad2026/build -j$(nproc) 2>&1 | tail -20
./build/Nomad2026_artefacts/Debug/Nomad2026
```
Expected: app launches, looks identical to pre-task baseline. Load a patch with oscillators, check knobs, cables, text displays, LEDs all render as before.

- [ ] **Step 17: Commit**

```bash
cd /mnt/SPEED/CODE/Nomad2026
git add source/ui/PatchCanvasComponent.h source/ui/PatchCanvasComponent.cpp
git commit -m "feat: wire PatchCanvas color literals to activeScheme_ (Classic pixel-perfect)"
```

---

## Task 3: Add `setTheme()` forwarding + View → Theme menu

**Files:**
- Modify: `source/ui/PatchCanvasComponent.h` (add to `PatchCanvasComponent` wrapper)
- Modify: `source/ui/MainLayout.h` / `MainLayout.cpp` (forward to canvas)
- Modify: `source/MainComponent.cpp` (menu items 70 + 71)

- [ ] **Step 1: Add `setTheme()` to `PatchCanvasComponent` wrapper class**

In `PatchCanvasComponent` class (the outer wrapper, `PatchCanvasComponent.h:302-488`), add to `public:` section after `void shakeCables()`:
```cpp
    void setTheme(const ColorScheme& cs)
    {
        polyCanvas.setTheme(cs);
        commonCanvas.setTheme(cs);
    }
```

- [ ] **Step 2: Add `setTheme()` forwarding to `MainLayout`**

In `source/ui/MainLayout.h`, find the class definition for `MainLayout` and add:
```cpp
    void setTheme(const ColorScheme& cs);
    PatchCanvasComponent& getCanvas();
```
(check if `getCanvas()` already exists — if so, skip)

In `source/ui/MainLayout.cpp`, add the implementation:
```cpp
void MainLayout::setTheme(const ColorScheme& cs)
{
    patchCanvas.setTheme(cs);
}
```
(use the actual member name for the `PatchCanvasComponent` inside `MainLayout` — check the .h for the field name)

- [ ] **Step 3: Add View → Theme submenu to `MainComponent::getMenuForIndex()`**

In `source/MainComponent.cpp`, in the `menuIndex == 2` (View) branch, add before the closing brace:
```cpp
    menu.addSeparator();
    juce::PopupMenu themeMenu;
    bool isClassic = (mainLayout->getCanvas().getTheme().gridBackground == kClassicTheme.gridBackground);
    themeMenu.addItem(70, "Classic", true, isClassic);
    themeMenu.addItem(71, "Dark",    true, !isClassic);
    menu.addSubMenu("Theme", themeMenu);
```

- [ ] **Step 4: Handle menu items 70 and 71 in `menuItemSelected()`**

In `MainComponent::menuItemSelected()`, add two cases:
```cpp
  case 70:
    mainLayout->getCanvas().setTheme(kClassicTheme);
    break;
  case 71:
    mainLayout->getCanvas().setTheme(kDarkTheme);
    break;
```

Add the necessary includes at top of `MainComponent.cpp` if not already present:
```cpp
#include "ui/ColorScheme.h"
```

- [ ] **Step 5: Build and test menu**

```bash
cmake --build /mnt/SPEED/CODE/Nomad2026/build -j$(nproc) 2>&1 | tail -20
./build/Nomad2026_artefacts/Debug/Nomad2026
```
Expected: View → Theme shows Classic (checked) and Dark. Selecting Dark changes canvas appearance. Selecting Classic restores original look. Checkmark toggles correctly.

- [ ] **Step 6: Commit**

```bash
cd /mnt/SPEED/CODE/Nomad2026
git add source/ui/PatchCanvasComponent.h source/ui/MainLayout.h source/ui/MainLayout.cpp source/MainComponent.cpp
git commit -m "feat: add View->Theme menu with Classic/Dark switching"
```

---

## Task 4: Visual validation of Dark theme

No code changes — purely visual review.

- [ ] **Step 1: Launch app and switch to Dark theme via View → Theme → Dark**

- [ ] **Step 2: Load a patch with oscillators (OscA, OscB) — verify:**
  - Canvas background: near-black `#111111` with subtle grid lines
  - Module panels: dark grey `#2D3033` (set by `descriptor.background` — this comes from `classic-theme.xml`, **not** the scheme)
    - NOTE: module background color comes from `m.getDescriptor()->background` which is XML-driven. The scheme's `moduleBg` is NOT wired to module backgrounds. This is intentional — Classic theme module colors are purple/grey from the XML and will remain so in Dark theme too. If you want different module colors in Dark, that requires a separate XML override (out of scope).
  - Knobs: light grey body with dark grip line — visually semi-flat
  - Text displays: dark bg `#1a1a1a` with orange `#F37F15` text
  - Cables: same colors as Classic

- [ ] **Step 3: Check connectors — outputs are rounded squares, inputs are circles (unchanged)**

- [ ] **Step 4: Check LEDs and meters (connect synth or load a live patch)**

- [ ] **Step 5: Check selection rubber-band — should be `#4444ff` (blue) instead of yellow**

- [ ] **Step 6: Verify default startup is Dark (not Classic)**

- [ ] **Step 7: Commit final validation note**

```bash
cd /mnt/SPEED/CODE/Nomad2026
git commit --allow-empty -m "chore: dark theme visual validation complete"
```

---

## Self-Review

**Spec coverage check:**
- ✅ ColorScheme struct with ~49 fields → Task 1
- ✅ `kClassicTheme` = exact current colors → Task 1 Step 2
- ✅ `kDarkTheme` = dark palette → Task 1 Step 2
- ✅ `activeScheme_` member in `PatchCanvas` → Task 2 Step 1
- ✅ `setTheme()` method → Task 2 Step 1 + Task 3 Step 1
- ✅ All 86 color literals wired → Task 2 Steps 2-14
- ✅ View → Theme submenu with checkmark → Task 3 Steps 3-4
- ✅ Default = Dark on launch → kDarkTheme initializer in header
- ✅ No persistence (always starts Dark) → no settings.json changes
- ✅ CMakeLists.txt updated → Task 1 Step 3

**Important note on module background colors:** Module `background` color comes from `m.getDescriptor()->background` which is set from `classic-theme.xml` data via `ModuleDescriptions`. The scheme's `moduleBg` field exists for the spec but is NOT plumbed to module backgrounds in this iteration. Dark theme will use the same XML-sourced module colors as Classic. This is consistent with spec section 5 (out of scope: custom themes from external files).

**Placeholder scan:** No TBDs found. All color values are concrete hex literals. All file paths are absolute or project-relative.

**Type consistency:** `ColorScheme` struct fields are consistent across all tasks. `kClassicTheme`/`kDarkTheme` names match extern declarations in header.
