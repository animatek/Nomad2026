# Filter Visual Pass A+B Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Hz/dB/Oct/vowel formatters to filter and EQ text displays, fix EQ bandwidth curve, implement Vocoder routing matrix display, and wire up the Vocoder Rnd button.

**Architecture:** All changes follow the existing pattern: ThemeData.h defines data flags/fields, ThemeData.cpp detects them during XML parsing, PatchCanvasComponent.cpp uses them at render/click time. No new files needed.

**Tech Stack:** JUCE/C++17, CMake. Build: `cmake --build build -j$(nproc)`. Run: `./build/Nomad2026_artefacts/Debug/Nomad2026`

---

## File Map

| File | What changes |
|------|-------------|
| `source/model/ThemeData.h` | Add 6 bool flags to `ThemeTextDisplay`; add `bwComponentId`+`bandIds[16]` to `ThemeCustomDisplay`; add `isCall`+`callMethod` to `ThemeButton` |
| `source/model/ThemeData.cpp` | Add formatter detection for 6 new TextDisplay cases; parse `<bandwidth>`, `<band0-15>`, `<call>` sub-elements |
| `source/ui/PatchCanvasComponent.cpp` | Add 6 format cases in `paintTextDisplays`; use BW in `eq-mid-display` render; add `vocoder-display` render case; handle Rnd call in click handler |

---

### Task 1: Add flags to ThemeTextDisplay and ThemeButton

**Files:**
- Modify: `source/model/ThemeData.h`

- [ ] **Step 1: Add 6 formatter flags to `ThemeTextDisplay` (lines 63-66, after `envReleaseFormat`)**

```cpp
    bool envReleaseFormat = false;  // true → fmtEnvelopeRelease: lookup 128 entries
    bool filterHz1Format = false;   // true → 504*2^((v-64)/12) Hz (FilterA, FilterB)
    bool filterHz2Format = false;   // true → 330*2^((v-60)/12) Hz (FilterC/D/E/F)
    bool eqHzFormat = false;        // true → 471*2^((v-60)/12) Hz (EqMid, EqShelving)
    bool eqGainFormat = false;      // true → (v-64)*0.28125 dB (EqMid, EqShelving)
    bool eqBwFormat = false;        // true → v/75.0 Oct (EqMid bandwidth)
    bool vowelFormat = false;       // true → vowel name: A/E/I/O/U/Y/AA/AE/OE (VocalFilter)
```

- [ ] **Step 2: Add `bwComponentId` to `ThemeCustomDisplay` (after `curveComponentId` line ~110)**

```cpp
    juce::String curveComponentId;   // <curve component-id="pN"> (curve type)
    juce::String bwComponentId;      // <bandwidth component-id="pN"> (EqMid)
    juce::String bandIds[16];        // <band0..band15 component-id="pN"> (Vocoder)
```

- [ ] **Step 3: Add `isCall`+`callMethod` to `ThemeButton` (after `imageRefs` line ~40)**

```cpp
    std::vector<juce::String> imageRefs;  // image href per btn index (e.g. "wf_sine", "wf_saw")
    bool isCall = false;       // button triggers a call action, not a parameter change
    juce::String callMethod;   // e.g. "rnd" (Vocoder Rnd button)
```

- [ ] **Step 4: Build to confirm no compile errors**

```bash
cmake --build /mnt/SPEED/CODE/Nomad2026/build -j$(nproc) 2>&1 | grep -E "error:|warning:.*error" | head -20
```
Expected: no errors.

- [ ] **Step 5: Commit**

```bash
cd /mnt/SPEED/CODE/Nomad2026
git add source/model/ThemeData.h
git commit -m "feat(theme): add filter/eq/vowel format flags and vocoder/call fields"
```

---

### Task 2: ThemeData.cpp — detect formatter types in parseTextDisplay

**Files:**
- Modify: `source/model/ThemeData.cpp` (around line 363, after the `envReleaseFormat` block)

- [ ] **Step 1: Locate the insertion point**

The block ends at approximately:
```cpp
        if (theme.componentId == "m71")
        {
            if (td.componentId == "p1") td.envAttackFormat  = true;
            if (td.componentId == "p2") td.envReleaseFormat = true;
        }
```
Add the new blocks immediately after this.

- [ ] **Step 2: Add the 6 formatter detection blocks**

```cpp
        // fmtFilterHz1: 504*2^((v-64)/12) — FilterA (m86/p1), FilterB (m87/p1)
        static const juce::StringArray filterHz1Modules { "m86", "m87" };
        if (filterHz1Modules.contains(theme.componentId) && td.componentId == "p1")
            td.filterHz1Format = true;

        // fmtFilterHz2: 330*2^((v-60)/12) — FilterC(m50/p2), FilterD(m49/p2),
        //                                     FilterE(m51/p5), FilterF(m92/p2)
        if ((theme.componentId == "m50" && td.componentId == "p2") ||
            (theme.componentId == "m49" && td.componentId == "p2") ||
            (theme.componentId == "m51" && td.componentId == "p5") ||
            (theme.componentId == "m92" && td.componentId == "p2"))
            td.filterHz2Format = true;

        // fmtEqHz: 471*2^((v-60)/12) — EqMid (m103/p1), EqShelving (m104/p1)
        static const juce::StringArray eqModules { "m103", "m104" };
        if (eqModules.contains(theme.componentId) && td.componentId == "p1")
            td.eqHzFormat = true;

        // EqGain: (v-64)*0.28125 dB — EqMid (m103/p2), EqShelving (m104/p2)
        if (eqModules.contains(theme.componentId) && td.componentId == "p2")
            td.eqGainFormat = true;

        // EqBandwidth: v/75.0 Oct — EqMid only (m103/p3)
        if (theme.componentId == "m103" && td.componentId == "p3")
            td.eqBwFormat = true;

        // Vowels: VocalFilter (m45) p1=left, p2=middle, p3=right
        static const juce::StringArray vowelParams { "p1", "p2", "p3" };
        if (theme.componentId == "m45" && vowelParams.contains(td.componentId))
            td.vowelFormat = true;
```

- [ ] **Step 3: Build to confirm no compile errors**

```bash
cmake --build /mnt/SPEED/CODE/Nomad2026/build -j$(nproc) 2>&1 | grep -E "error:" | head -10
```
Expected: no errors.

- [ ] **Step 4: Commit**

```bash
cd /mnt/SPEED/CODE/Nomad2026
git add source/model/ThemeData.cpp
git commit -m "feat(theme): detect filter/eq/vowel formatter types in parseTextDisplay"
```

---

### Task 3: ThemeData.cpp — parse bandwidth, band0-15, call sub-elements

**Files:**
- Modify: `source/model/ThemeData.cpp` (the sub-element loop inside the custom display parser, around line 119-152)

- [ ] **Step 1: Locate the custom display sub-element loop**

The loop looks like:
```cpp
            for (auto* sub = child->getFirstChildElement(); sub != nullptr; sub = sub->getNextElement())
            {
                auto subTag = sub->getTagName();
                if (subTag == "phase")
                    cd.phaseComponentId = sub->getStringAttribute("component-id");
                // ...
                else if (subTag == "curve")
                    cd.curveComponentId = sub->getStringAttribute("component-id");
            }
```

- [ ] **Step 2: Add bandwidth and band0-15 parsing after the `curve` case**

```cpp
                else if (subTag == "bandwidth")
                    cd.bwComponentId = sub->getStringAttribute("component-id");
                else if (subTag.startsWith("band") && subTag.length() <= 6)
                {
                    int idx = subTag.substring(4).getIntValue();
                    if (idx >= 0 && idx < 16)
                        cd.bandIds[idx] = sub->getStringAttribute("component-id");
                }
```

- [ ] **Step 3: Locate parseButton() and add `<call>` detection**

Find `parseButton` in ThemeData.cpp. The existing button label loop looks like:
```cpp
    for (auto* btn = elem.getFirstChildElement(); btn != nullptr;
         btn = btn->getNextElement())
    {
        if (btn->getTagName() == "btn")
        {
            // ... existing label/imageRef handling
        }
    }
```

Add after the loop (before `theme.buttons.push_back(tb)`):
```cpp
    // Detect <call component="..." method="rnd"> (Vocoder Rnd button)
    if (auto* call = elem.getChildByName("call"))
    {
        tb.isCall    = true;
        tb.callMethod = call->getStringAttribute("method");
    }
```

- [ ] **Step 4: Build**

```bash
cmake --build /mnt/SPEED/CODE/Nomad2026/build -j$(nproc) 2>&1 | grep -E "error:" | head -10
```
Expected: no errors.

- [ ] **Step 5: Commit**

```bash
cd /mnt/SPEED/CODE/Nomad2026
git add source/model/ThemeData.cpp
git commit -m "feat(theme): parse bandwidth, vocoder band0-15, and call sub-elements"
```

---

### Task 4: paintTextDisplays — add 6 format cases

**Files:**
- Modify: `source/ui/PatchCanvasComponent.cpp` (inside `paintTextDisplays`, before the numeric fallback `else { displayStr = juce::String(val); }`)

- [ ] **Step 1: Locate the insertion point**

Find the block that ends with:
```cpp
            else if (td.envReleaseFormat)
            {
                // ... table lookup
                displayStr = tbl[idx];
            }
            else
            {
                displayStr = juce::String(val);
            }
```

Insert the 6 new cases between `envReleaseFormat` and the final `else`.

- [ ] **Step 2: Add filterHz1Format case**

```cpp
            else if (td.filterHz1Format)
            {
                // fmtFilterHz1: 504 * 2^((val-64)/12) — FilterA (6dB LP), FilterB (6dB HP)
                double hz = 504.0 * std::pow(2.0, (val - 64) / 12.0);
                if (hz < 1000.0)
                    displayStr = juce::String(juce::roundToInt(hz)) + " Hz";
                else if (hz < 10000.0)
                    displayStr = juce::String(hz / 1000.0, 2) + " kHz";
                else
                    displayStr = juce::String(hz / 1000.0, 1) + " kHz";
            }
```

- [ ] **Step 3: Add filterHz2Format case**

```cpp
            else if (td.filterHz2Format)
            {
                // fmtFilterHz2: 330 * 2^((val-60)/12) — FilterC/D/E/F
                double hz = 330.0 * std::pow(2.0, (val - 60) / 12.0);
                if (hz < 1000.0)
                    displayStr = juce::String(juce::roundToInt(hz)) + " Hz";
                else if (hz < 10000.0)
                    displayStr = juce::String(hz / 1000.0, 2) + " kHz";
                else
                    displayStr = juce::String(hz / 1000.0, 1) + " kHz";
            }
```

- [ ] **Step 4: Add eqHzFormat, eqGainFormat, eqBwFormat, vowelFormat cases**

```cpp
            else if (td.eqHzFormat)
            {
                // fmtEqHz: 471 * 2^((val-60)/12) — EqMid, EqShelving frequency
                double hz = 471.0 * std::pow(2.0, (val - 60) / 12.0);
                if (hz < 1000.0)
                    displayStr = juce::String(juce::roundToInt(hz)) + " Hz";
                else if (hz < 10000.0)
                    displayStr = juce::String(hz / 1000.0, 2) + " kHz";
                else
                    displayStr = juce::String(hz / 1000.0, 1) + " kHz";
            }
            else if (td.eqGainFormat)
            {
                // (val-64) * 0.28125 dB — EqMid, EqShelving gain
                // Verified: val=43 → -5.9 dB, val=64 → 0.0 dB, val=127 → +17.7 dB
                double db = (val - 64) * 0.28125;
                displayStr = juce::String(db, 1) + " dB";
            }
            else if (td.eqBwFormat)
            {
                // val / 75.0 Oct — EqMid bandwidth
                // Verified: val=69 → 0.92 Oct
                double oct = val / 75.0;
                displayStr = juce::String(oct, 2) + " Oct";
            }
            else if (td.vowelFormat)
            {
                // VocalFilter vowel names (DATA_VOWELS from nmformat.js)
                static const char* vowels[] = { "A","E","I","O","U","Y","AA","AE","OE" };
                int idx = juce::jlimit(0, 8, val);
                displayStr = vowels[idx];
            }
```

- [ ] **Step 5: Build and visual test**

```bash
cmake --build /mnt/SPEED/CODE/Nomad2026/build -j$(nproc) 2>&1 | grep -E "error:" | head -10
```
Then run the app and verify:
- FilterA knob at val=64 → textDisplay shows "504 Hz"
- FilterA knob at val=76 → textDisplay shows "1.01 kHz"
- EqMid freq at val=60 → "471 Hz"
- EqMid gain at val=43 → "-5.9 dB"
- EqMid gain at val=64 → "0.0 dB"
- EqMid BW at val=69 → "0.92 Oct"
- VocalFilter p1 at val=0 → "A", val=1 → "E", val=4 → "U"

- [ ] **Step 6: Commit**

```bash
cd /mnt/SPEED/CODE/Nomad2026
git add source/ui/PatchCanvasComponent.cpp
git commit -m "feat(ui): add filterHz1/2, eqHz/gain/bw, vowel formatters to textDisplays"
```

---

### Task 5: eq-mid-display — use bandwidth in curve rendering

**Files:**
- Modify: `source/ui/PatchCanvasComponent.cpp` (inside `paintCustomDisplays`, the `eq-mid-display` branch around line 2350)

- [ ] **Step 1: Locate the EqMid render block**

Find:
```cpp
        if (type == "eq-mid-display" || type == "eq-shelving-display")
        {
            float freq = 0.5f, gain = 0.5f;
            for (auto& p : m.getParameters())
            {
                // ... reads freq and gain by name
            }
```

- [ ] **Step 2: Add BW readout to the parameter scan loop**

Replace the parameter loop with:
```cpp
            float freq = 0.5f, gain = 0.5f, bw = 0.5f;
            for (auto& p : m.getParameters())
            {
                auto name = p.getDescriptor()->name.toLowerCase();
                auto* pd = p.getDescriptor();
                int range = pd->maxValue - pd->minValue;
                float norm = (range > 0) ? static_cast<float>(p.getValue() - pd->minValue) / static_cast<float>(range) : 0.5f;

                if (name.contains("freq"))           freq = norm;
                else if (name.contains("gain"))      gain = norm;
                else if (name.contains("bandwidth")) bw   = norm;
            }
```

- [ ] **Step 3: Use BW to modulate curve width in the eq-mid branch**

Find the parametric peak calculation inside the `else` (not shelving) branch:
```cpp
                else
                {
                    // Parametric peak
                    float dist = (t - freq) * 5.0f;
                    response = 0.5f + gainAmt * 0.4f * std::exp(-dist * dist);
                }
```

Replace the `5.0f` with a BW-derived factor:
```cpp
                else
                {
                    // Parametric peak — BW=0→wide (factor 1.5), BW=1→narrow (factor 6.0)
                    float bwFactor = 1.5f + bw * 4.5f;
                    float dist = (t - freq) * bwFactor;
                    response = 0.5f + gainAmt * 0.4f * std::exp(-dist * dist);
                }
```

- [ ] **Step 4: Build and visual test**

```bash
cmake --build /mnt/SPEED/CODE/Nomad2026/build -j$(nproc) 2>&1 | grep -E "error:" | head -10
```
Run app → load EqMid → adjust BW knob → confirm curve narrows/widens in the display box.

- [ ] **Step 5: Commit**

```bash
cd /mnt/SPEED/CODE/Nomad2026
git add source/ui/PatchCanvasComponent.cpp
git commit -m "feat(ui): use bandwidth parameter in eq-mid-display curve width"
```

---

### Task 6: vocoder-display — render routing lines

**Files:**
- Modify: `source/ui/PatchCanvasComponent.cpp` (inside `paintCustomDisplays`, add new case after compressor/expander block)

- [ ] **Step 1: Locate insertion point**

Find the end of the compressor/expander display block:
```cpp
        // --- Compressor / Expander displays ---
        if (type == "compressor-display" || type == "expander-display")
        {
            // ...
        }
```
Add the vocoder case immediately after this block.

- [ ] **Step 2: Add vocoder-display render case**

```cpp
        // --- Vocoder routing display ---
        if (type == "vocoder-display")
        {
            // Black background with border
            g.setColour(activeScheme_.displayBg);
            g.fillRect(dx, dy, dw, dh);
            g.setColour(activeScheme_.displayBorder);
            g.drawRect(dx, dy, dw, dh, 1.0f);

            // Draw routing lines: band[i] = output band index (1-based, 0=off)
            // Line goes from: top row at column (band[i]-1) → bottom row at column i
            constexpr int kBands = 16;
            float space   = dw / static_cast<float>(kBands);
            float loffset = dx + space * 0.5f;
            float top     = dy + 1.0f;
            float bot     = dy + dh - 1.0f;

            g.setColour(juce::Colour(0xff00cc44));  // green routing lines (matches original)
            for (int i = 0; i < kBands; ++i)
            {
                if (cd.bandIds[i].isEmpty()) continue;
                auto* param = findParameter(m, cd.bandIds[i]);
                if (param == nullptr) continue;
                int bandVal = param->getValue();
                if (bandVal <= 0) continue;                       // 0 = off, no line

                float x0 = loffset + static_cast<float>(bandVal - 1) * space;  // output column
                float x1 = loffset + static_cast<float>(i) * space;            // input column
                g.drawLine(x0, top, x1, bot, 1.0f);
            }
            continue;
        }
```

- [ ] **Step 3: Build and visual test**

```bash
cmake --build /mnt/SPEED/CODE/Nomad2026/build -j$(nproc) 2>&1 | grep -E "error:" | head -10
```
Run app → load a Vocoder module → confirm:
- Black display area at x=26 y=25 width=192 height=50
- With default params (band 1→1, 2→2, ... 16→16): 16 vertical green lines
- Adjusting band increment buttons changes line angles

- [ ] **Step 4: Commit**

```bash
cd /mnt/SPEED/CODE/Nomad2026
git add source/ui/PatchCanvasComponent.cpp
git commit -m "feat(ui): render vocoder routing matrix display with green lines"
```

---

### Task 7: Vocoder Rnd button — wire up call action

**Files:**
- Modify: `source/ui/PatchCanvasComponent.cpp` (the button click handler)

- [ ] **Step 1: Find the button click handler**

Search for where button clicks trigger `param->setValue` or similar. The click handler processes `ThemeButton` and calls some parameter change. Look for:
```cpp
// Button click handling
```
or the mouse-down / hitTest area for buttons. Find where `tb.isIncrement` is handled as a special case — the Rnd call follows the same pattern.

- [ ] **Step 2: Add `isCall` branch to the button click handler**

In the button click dispatch, add before or after the `isIncrement` check:
```cpp
            // Call action (e.g. Vocoder Rnd)
            if (tb.isCall)
            {
                if (tb.callMethod == "rnd")
                {
                    // Assign random routing values (0-16) to all vocoder band parameters
                    // Band params are named "band N" in modules.xml (p1..p16, maxValue=16)
                    for (auto& p : m->getParameters())
                    {
                        auto* pd = p.getDescriptor();
                        if (pd->name.startsWith("band "))
                        {
                            int rndVal = juce::Random::getSystemRandom().nextInt(pd->maxValue + 1);
                            p.setValue(rndVal);
                            sendParameterChange(*m, p);
                        }
                    }
                }
                return;   // consumed
            }
```

Note: `sendParameterChange` is already used elsewhere in the click handler for regular parameter buttons — use the same call pattern.

- [ ] **Step 3: Build and visual test**

```bash
cmake --build /mnt/SPEED/CODE/Nomad2026/build -j$(nproc) 2>&1 | grep -E "error:" | head -10
```
Run app → load Vocoder → click Rnd button → confirm routing lines change randomly in the display.

- [ ] **Step 4: Commit**

```bash
cd /mnt/SPEED/CODE/Nomad2026
git add source/ui/PatchCanvasComponent.cpp
git commit -m "feat(ui): wire Vocoder Rnd button to randomise band routing via call action"
```

---

## Spec Coverage Check

| Spec requirement | Task |
|-----------------|------|
| filterHz1Format (FilterA/B) | Task 1, 2, 4 |
| filterHz2Format (FilterC/D/E/F) | Task 1, 2, 4 |
| eqHzFormat (EqMid/EqShelving freq) | Task 1, 2, 4 |
| eqGainFormat (EqMid/EqShelving gain dB) | Task 1, 2, 4 |
| eqBwFormat (EqMid BW Oct) | Task 1, 2, 4 |
| vowelFormat (VocalFilter) | Task 1, 2, 4 |
| EQ display bandwidth fix | Task 3, 5 |
| Vocoder display routing lines | Task 3, 6 |
| Vocoder Rnd button | Task 3, 7 |
