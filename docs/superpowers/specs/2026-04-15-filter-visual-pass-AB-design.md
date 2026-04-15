# Filter Visual Pass — Part A+B Design

**Date:** 2026-04-15  
**Scope:** Filter category modules — TextDisplay formatters (A) + EQ display bandwidth fix + Vocoder display + Vocoder Rnd button (B)

---

## A: TextDisplay Formatters

### New flags in `ThemeTextDisplay` (ThemeData.h)

| Flag | Formula | Modules / Params |
|------|---------|-----------------|
| `filterHz1Format` | `504 × 2^((v-64)/12)` Hz | FilterA m86/p1, FilterB m87/p1 |
| `filterHz2Format` | `330 × 2^((v-60)/12)` Hz | FilterC m50/p2, FilterD m49/p2, FilterE m51/p5, FilterF m92/p2 |
| `eqHzFormat` | `471 × 2^((v-60)/12)` Hz | EqMid m103/p1, EqShelving m104/p1 |
| `eqGainFormat` | `(v-64) × 0.28125 dB` | EqMid m103/p2, EqShelving m104/p2 |
| `eqBwFormat` | `v / 75.0 Oct` | EqMid m103/p3 |
| `vowelFormat` | array index → `["A","E","I","O","U","Y","AA","AE","OE"]` | VocalFilter m45/p1,p2,p3 |

**Verification:**
- filterHz1: v=64 → 504 Hz ✓ (user confirmed)
- filterHz2: v=60 → 330 Hz ✓ (user confirmed)
- eqGain: v=43 → (43-64)×0.28125 = -5.906 ≈ -5.9 dB ✓ (user confirmed)
- eqBw: v=69 → 69/75 = 0.92 Oct ✓ (user confirmed)

### ThemeData.cpp changes

Add to the formatter-assignment block in `parseTextDisplay()`:

```cpp
// Filter frequency Hz1: FilterA (m86), FilterB (m87) — p1
static const juce::StringArray filterHz1Modules { "m86", "m87" };
if (filterHz1Modules.contains(theme.componentId) && td.componentId == "p1")
    td.filterHz1Format = true;

// Filter frequency Hz2: FilterC(m50/p2), FilterD(m49/p2), FilterE(m51/p5), FilterF(m92/p2)
if ((theme.componentId == "m50" && td.componentId == "p2") ||
    (theme.componentId == "m49" && td.componentId == "p2") ||
    (theme.componentId == "m51" && td.componentId == "p5") ||
    (theme.componentId == "m92" && td.componentId == "p2"))
    td.filterHz2Format = true;

// EQ Hz: EqMid (m103/p1), EqShelving (m104/p1)
static const juce::StringArray eqModules { "m103", "m104" };
if (eqModules.contains(theme.componentId) && td.componentId == "p1")
    td.eqHzFormat = true;

// EQ Gain: EqMid (m103/p2), EqShelving (m104/p2)
if (eqModules.contains(theme.componentId) && td.componentId == "p2")
    td.eqGainFormat = true;

// EQ Bandwidth: EqMid (m103/p3) only
if (theme.componentId == "m103" && td.componentId == "p3")
    td.eqBwFormat = true;

// Vowels: VocalFilter (m45) p1,p2,p3
static const juce::StringArray vowelParams { "p1", "p2", "p3" };
if (theme.componentId == "m45" && vowelParams.contains(td.componentId))
    td.vowelFormat = true;
```

### paintTextDisplays changes (PatchCanvasComponent.cpp)

Add before the numeric fallback:

```cpp
else if (td.filterHz1Format)
{
    // fmtFilterHz1: 504 * 2^((val-64)/12) — FilterA, FilterB
    double hz = 504.0 * std::pow(2.0, (val - 64) / 12.0);
    if (hz < 1000.0) displayStr = juce::String(juce::roundToInt(hz)) + " Hz";
    else             displayStr = juce::String(hz / 1000.0, 2) + " kHz";
}
else if (td.filterHz2Format)
{
    // fmtFilterHz2: 330 * 2^((val-60)/12) — FilterC/D/E/F
    double hz = 330.0 * std::pow(2.0, (val - 60) / 12.0);
    if (hz < 1000.0) displayStr = juce::String(juce::roundToInt(hz)) + " Hz";
    else if (hz < 10000.0) displayStr = juce::String(hz / 1000.0, 2) + " kHz";
    else             displayStr = juce::String(hz / 1000.0, 1) + " kHz";
}
else if (td.eqHzFormat)
{
    // fmtEqHz: 471 * 2^((val-60)/12) — EqMid, EqShelving
    double hz = 471.0 * std::pow(2.0, (val - 60) / 12.0);
    if (hz < 1000.0) displayStr = juce::String(juce::roundToInt(hz)) + " Hz";
    else if (hz < 10000.0) displayStr = juce::String(hz / 1000.0, 2) + " kHz";
    else             displayStr = juce::String(hz / 1000.0, 1) + " kHz";
}
else if (td.eqGainFormat)
{
    // (val-64) * 0.28125 dB — EqMid, EqShelving gain
    double db = (val - 64) * 0.28125;
    displayStr = juce::String(db, 1) + " dB";
}
else if (td.eqBwFormat)
{
    // val / 75.0 Oct — EqMid bandwidth
    double oct = val / 75.0;
    displayStr = juce::String(oct, 2) + " Oct";
}
else if (td.vowelFormat)
{
    // VocalFilter vowel names
    static const char* vowels[] = { "A","E","I","O","U","Y","AA","AE","OE" };
    int idx = juce::jlimit(0, 8, val);
    displayStr = vowels[idx];
}
```

---

## B1: EQ Display — Bandwidth Fix

### Problem
`eq-mid-display` renders without bandwidth data — curve width is fixed at `factor=5.0f`, ignoring the BW parameter.

### ThemeCustomDisplay (ThemeData.h)
Add:
```cpp
juce::String bwComponentId;   // <bandwidth component-id="pN"> (EqMid)
```

### ThemeData.cpp
In the sub-element loop for custom displays, add:
```cpp
else if (subTag == "bandwidth")
    cd.bwComponentId = sub->getStringAttribute("component-id");
```

### paintCustomDisplays (PatchCanvasComponent.cpp)
In the `eq-mid-display` branch, add BW lookup after the freq/gain loop:
```cpp
float bw = 0.5f;  // default mid bandwidth
for (auto& p : m.getParameters()) {
    if (p.getDescriptor()->name.toLowerCase().contains("bandwidth")) {
        int range = pd->maxValue - pd->minValue;
        bw = (range > 0) ? static_cast<float>(p.getValue() - pd->minValue) / range : 0.5f;
    }
}
float bwFactor = 6.0f - bw * 4.5f;  // range 1.5..6.0 (wider BW → narrower factor)
// Replace the hardcoded 5.0f with bwFactor:
float dist = (t - freq) * bwFactor;
```

---

## B2: Vocoder Display

### ThemeCustomDisplay (ThemeData.h)
Add:
```cpp
juce::String bandIds[16];  // <band0..band15 component-id="pN">
```

### ThemeData.cpp
In the sub-element loop, add:
```cpp
else if (subTag.startsWith("band") && subTag.length() <= 6) {
    int idx = subTag.substring(4).getIntValue();
    if (idx >= 0 && idx < 16)
        cd.bandIds[idx] = sub->getStringAttribute("component-id");
}
```

### paintCustomDisplays (PatchCanvasComponent.cpp)
Add new case after the compressor/expander block:
```cpp
if (type == "vocoder-display") {
    // Draw black background
    g.setColour(activeScheme_.displayBg);
    g.fillRect(dx, dy, dw, dh);
    g.setColour(activeScheme_.displayBorder);
    g.drawRect(dx, dy, dw, dh, 1.0f);

    // Draw routing lines: for each band i, if band[i]>0, draw line from
    // output col (band[i]-1) at top to input col (i) at bottom
    float space = dw / 16.0f;
    float loffset = dx + space * 0.5f;
    float top = dy + 1.0f;
    float bot = dy + dh - 1.0f;

    g.setColour(juce::Colour(0xff00cc44));  // green routing lines
    for (int i = 0; i < 16; ++i)
    {
        auto* param = findParameter(m, cd.bandIds[i]);
        if (param != nullptr && param->getValue() > 0)
        {
            float x0 = loffset + (param->getValue() - 1) * space;
            float x1 = loffset + i * space;
            g.drawLine(x0, top, x1, bot, 1.0f);
        }
    }
}
```

---

## B3: Vocoder Rnd Button

### Problem
The XML `<call component="vocoderDisp" method="rnd" />` is not parsed or handled. Clicking the Rnd button does nothing.

### ThemeButton (ThemeData.h)
Add:
```cpp
bool isCall = false;       // button triggers a call action, not a parameter
juce::String callMethod;   // e.g. "rnd"
```

### ThemeData.cpp — parseButton()
After parsing `<btn>` children, also check for `<call>`:
```cpp
if (auto* call = elem.getChildByName("call"))
{
    tb.isCall = true;
    tb.callMethod = call->getStringAttribute("method");
}
```

### Click handler (PatchCanvasComponent.cpp)
When a button click is detected and `tb.isCall`:
```cpp
if (tb.isCall && tb.callMethod == "rnd")
{
    // Assign random band routing values (1-16) to all 16 bands of this module
    auto& params = m.getParameters();
    for (auto& p : params)
    {
        auto* pd = p.getDescriptor();
        if (pd->name.startsWith("band "))
        {
            int rndVal = juce::Random::getSystemRandom().nextInt(17); // 0-16
            p.setValue(rndVal);
            // Send parameter change via SysEx
            sendParameterChange(m, p);
        }
    }
}
```

---

## Files Changed

| File | Changes |
|------|---------|
| `source/model/ThemeData.h` | 6 flags in ThemeTextDisplay; bwComponentId, bandIds[16], isCall/callMethod |
| `source/model/ThemeData.cpp` | Formatter detection for 6 new cases; BW sub-element; band0-15 sub-elements; call sub-element |
| `source/ui/PatchCanvasComponent.cpp` | 6 format cases in paintTextDisplays; BW in eq-mid render; vocoder-display render case; Rnd call handler |

No XML changes required — classic-theme.xml already has all needed tags.
