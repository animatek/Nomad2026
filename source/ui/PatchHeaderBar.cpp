#include "PatchHeaderBar.h"

namespace
{
    // Morph colors from original Java JTNM1Context
    const juce::Colour morphColours[4] = {
        juce::Colour(0xffCB4F4F),  // Morph 1 - Red
        juce::Colour(0xff9AC889),  // Morph 2 - Green
        juce::Colour(0xff5A5FB3),  // Morph 3 - Blue
        juce::Colour(0xffE5DE45)   // Morph 4 - Yellow
    };

    // Cable colors matching SignalType order
    const juce::Colour cableColours[7] = {
        juce::Colour(0xffCB4F4F),  // Audio (Red)
        juce::Colour(0xff5A5FB3),  // Control (Blue)
        juce::Colour(0xffE5DE45),  // Logic (Yellow)
        juce::Colour(0xffA8A8A8),  // MasterSlave (Gray)
        juce::Colour(0xff9AC899),  // User1 (Green)
        juce::Colour(0xffBB00D7),  // User2 (Purple)
        juce::Colour(0xffEEEEEE)   // None (White)
    };

    // Layout constants for section widths
    constexpr int padL = 8;
    constexpr int sepGap = 14;
    constexpr int patchLblW = 44;
    constexpr int patchNameW = 150;
    constexpr int patchSecW = patchLblW + patchNameW + 4;
    constexpr int voicesLblW = 50;
    constexpr int voicesValW = 32;
    constexpr int arrowBtnW = 16;
    constexpr int voicesSecW = voicesLblW + voicesValW + arrowBtnW;
    constexpr int loadLblW = 36;
    constexpr int loadBarTotalW = 70;
    constexpr int loadSecW = loadLblW + loadBarTotalW;
}

PatchHeaderBar::PatchHeaderBar() {}

void PatchHeaderBar::setPatch(Patch* p)
{
    patch = p;
    repaint();
}

void PatchHeaderBar::resized()
{
    int x = padL;
    patchSecX_ = x;     x += patchSecW + sepGap;
    voicesSecX_ = x;    x += voicesSecW + sepGap;
    loadSecX_ = x;      x += loadSecW + sepGap;
    morphSecX_ = x;     x += 4 * (morphKnobSize + morphKnobSpacing) + sepGap;
    cableSecX_ = x;
}

// --- Layout helpers ---

juce::Rectangle<float> PatchHeaderBar::getMorphKnobBounds(int i) const
{
    float kx = static_cast<float>(morphSecX_ + i * (morphKnobSize + morphKnobSpacing));
    float ky = (static_cast<float>(getHeight()) - morphKnobSize) / 2.0f - 5.0f;
    return { kx, ky, static_cast<float>(morphKnobSize), static_cast<float>(morphKnobSize) };
}

int PatchHeaderBar::getMorphKnobAt(juce::Point<int> pos) const
{
    for (int i = 0; i < 4; i++)
        if (getMorphKnobBounds(i).expanded(2.0f).contains(pos.toFloat()))
            return i;
    return -1;
}

juce::Rectangle<float> PatchHeaderBar::getCableToggleBounds(int i) const
{
    float cx = static_cast<float>(cableSecX_ + i * (cableToggleSize + cableToggleSpacing));
    float cy = (static_cast<float>(getHeight()) - cableToggleSize) / 2.0f;
    return { cx, cy, static_cast<float>(cableToggleSize), static_cast<float>(cableToggleSize) };
}

int PatchHeaderBar::getCableToggleAt(juce::Point<int> pos) const
{
    for (int i = 0; i < numCableTypes; i++)
        if (getCableToggleBounds(i).expanded(2.0f).contains(pos.toFloat()))
            return i;
    return -1;
}

PatchHeaderBar::ArrowHit PatchHeaderBar::getVoiceArrowAt(juce::Point<int> pos) const
{
    int ax = voicesSecX_ + voicesLblW + voicesValW;
    auto area = juce::Rectangle<int>(ax, 0, arrowBtnW, getHeight());
    if (!area.contains(pos))
        return ArrowHit::None;
    return pos.y < getHeight() / 2 ? ArrowHit::Up : ArrowHit::Down;
}

bool PatchHeaderBar::getCableVisibility(int index) const
{
    if (!patch) return true;
    const auto& hdr = patch->getHeader();
    switch (index)
    {
        case 0: return hdr.cableVisRed;
        case 1: return hdr.cableVisBlue;
        case 2: return hdr.cableVisYellow;
        case 3: return hdr.cableVisGray;
        case 4: return hdr.cableVisGreen;
        case 5: return hdr.cableVisPurple;
        case 6: return hdr.cableVisWhite;
        default: return true;
    }
}

void PatchHeaderBar::toggleCableVisibility(int index)
{
    if (!patch) return;
    auto& hdr = patch->getHeader();
    switch (index)
    {
        case 0: hdr.cableVisRed    = !hdr.cableVisRed; break;
        case 1: hdr.cableVisBlue   = !hdr.cableVisBlue; break;
        case 2: hdr.cableVisYellow = !hdr.cableVisYellow; break;
        case 3: hdr.cableVisGray   = !hdr.cableVisGray; break;
        case 4: hdr.cableVisGreen  = !hdr.cableVisGreen; break;
        case 5: hdr.cableVisPurple = !hdr.cableVisPurple; break;
        case 6: hdr.cableVisWhite  = !hdr.cableVisWhite; break;
    }
    repaint();
    if (cableVisCallback) cableVisCallback();
}

// --- Drawing ---

void PatchHeaderBar::drawMorphKnob(juce::Graphics& g, float cx, float cy, float size,
                                    float normalized, const juce::String& label,
                                    juce::Colour colour)
{
    // Dark knob body
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillEllipse(cx, cy, size, size);

    // Colored ring
    g.setColour(colour);
    g.drawEllipse(cx + 1.0f, cy + 1.0f, size - 2.0f, size - 2.0f, 2.0f);

    // Grip line
    float angle = (-135.0f + normalized * 270.0f) * (juce::MathConstants<float>::pi / 180.0f);
    float centerX = cx + size * 0.5f;
    float centerY = cy + size * 0.5f;
    float sinA = std::sin(angle);
    float cosA = std::cos(angle);

    g.setColour(colour.brighter(0.4f));
    g.drawLine(centerX + sinA * size * 0.15f, centerY - cosA * size * 0.15f,
               centerX + sinA * size * 0.4f, centerY - cosA * size * 0.4f, 2.0f);

    // Label below in morph color
    g.setColour(colour.withAlpha(0.85f));
    g.setFont(juce::FontOptions(9.0f));
    g.drawText(label, static_cast<int>(cx - 2), static_cast<int>(cy + size + 1),
               static_cast<int>(size + 4), 10, juce::Justification::centred, false);
}

void PatchHeaderBar::drawLoadBar(juce::Graphics& g, int x, int y, int w, int h,
                                  float percent, const juce::String& label)
{
    // Inner label (PVA: / E:)
    int lblW = 26;
    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(juce::FontOptions(9.0f));
    g.drawText(label, x, y, lblW, h, juce::Justification::centredLeft);

    int barX = x + lblW;
    int barW = w - lblW;

    // Bar background
    g.setColour(juce::Colour(0xff252545));
    g.fillRect(barX, y, barW, h);
    g.setColour(juce::Colour(0xff444466));
    g.drawRect(barX, y, barW, h, 1);

    if (percent >= 0.0f)
    {
        int fillW = juce::jlimit(0, barW - 2, static_cast<int>(percent * (barW - 2)));
        juce::Colour barCol = percent < 0.6f  ? juce::Colour(0xff5a9a5a)
                            : percent < 0.85f ? juce::Colour(0xffb0a030)
                                              : juce::Colour(0xffcb4f4f);
        g.setColour(barCol);
        g.fillRect(barX + 1, y + 1, fillW, h - 2);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(8.0f));
        g.drawText(juce::String(static_cast<int>(percent * 100)) + "%",
                   barX, y, barW, h, juce::Justification::centred);
    }
    else
    {
        g.setColour(juce::Colour(0xff888888));
        g.setFont(juce::FontOptions(8.0f));
        g.drawText("--%", barX, y, barW, h, juce::Justification::centred);
    }
}

void PatchHeaderBar::paint(juce::Graphics& g)
{
    int h = getHeight();

    // Background
    g.fillAll(juce::Colour(0xff1a1a2e));

    // Bottom border
    g.setColour(juce::Colour(0xff333355));
    g.drawLine(0.0f, static_cast<float>(h) - 0.5f,
               static_cast<float>(getWidth()), static_cast<float>(h) - 0.5f, 1.0f);

    // Separators between sections
    auto drawSep = [&](int sectionX)
    {
        float sx = static_cast<float>(sectionX) - sepGap * 0.5f;
        g.setColour(juce::Colour(0xff333355));
        g.drawLine(sx, 4.0f, sx, static_cast<float>(h - 4), 1.0f);
    };
    drawSep(voicesSecX_);
    drawSep(loadSecX_);
    drawSep(morphSecX_);
    drawSep(cableSecX_);

    // --- Patch Name ---
    int x = patchSecX_;
    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("Patch:", x, 0, patchLblW, h, juce::Justification::centredLeft);
    x += patchLblW;

    auto nameRect = juce::Rectangle<int>(x, 6, patchNameW, h - 12);
    g.setColour(juce::Colour(0xff252545));
    g.fillRoundedRectangle(nameRect.toFloat(), 3.0f);
    g.setColour(juce::Colour(0xff444466));
    g.drawRoundedRectangle(nameRect.toFloat(), 3.0f, 1.0f);

    juce::String patchName = patch ? patch->getName() : "No Patch";
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(patchName, nameRect.reduced(6, 0), juce::Justification::centredLeft, true);

    // --- Voices ---
    x = voicesSecX_;
    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("Voices:", x, 0, voicesLblW, h, juce::Justification::centredLeft);
    x += voicesLblW;

    int voices = patch ? patch->getHeader().voices : 0;
    g.setColour(juce::Colours::white);
    g.drawText(juce::String(voices) + "/-", x, 0, voicesValW, h, juce::Justification::centredLeft);
    x += voicesValW;

    // Up/Down arrows
    {
        float arrowMidX = static_cast<float>(x) + arrowBtnW * 0.5f;
        int midY = h / 2;

        g.setColour(juce::Colour(0xffaaaaaa));
        juce::Path up;
        up.addTriangle(arrowMidX, static_cast<float>(midY - 7),
                       arrowMidX - 4.0f, static_cast<float>(midY - 1),
                       arrowMidX + 4.0f, static_cast<float>(midY - 1));
        g.fillPath(up);

        juce::Path down;
        down.addTriangle(arrowMidX - 4.0f, static_cast<float>(midY + 1),
                         arrowMidX + 4.0f, static_cast<float>(midY + 1),
                         arrowMidX, static_cast<float>(midY + 7));
        g.fillPath(down);
    }

    // --- Load ---
    x = loadSecX_;
    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("Load:", x, 0, loadLblW, h, juce::Justification::centredLeft);

    int barX = loadSecX_ + loadLblW;
    int barH = 10;
    int topBarY = h / 2 - barH - 1;
    int botBarY = h / 2 + 1;
    drawLoadBar(g, barX, topBarY, loadBarTotalW, barH, loadPva, "PVA:");
    drawLoadBar(g, barX, botBarY, loadBarTotalW, barH, loadE, "E:");

    // --- Morph Knobs ---
    for (int i = 0; i < 4; i++)
    {
        auto kb = getMorphKnobBounds(i);
        float norm = 0.0f;
        if (patch)
            norm = static_cast<float>(patch->morphValues[static_cast<size_t>(i)]) / 127.0f;

        drawMorphKnob(g, kb.getX(), kb.getY(), kb.getWidth(), norm,
                      "M" + juce::String(i + 1), morphColours[i]);
    }

    // --- Cable Visibility Toggles ---
    for (int i = 0; i < numCableTypes; i++)
    {
        auto tb = getCableToggleBounds(i);
        bool vis = getCableVisibility(i);

        if (vis)
        {
            g.setColour(cableColours[i]);
            g.fillEllipse(tb);
            g.setColour(cableColours[i].darker(0.3f));
            g.drawEllipse(tb.reduced(0.5f), 1.0f);
        }
        else
        {
            g.setColour(cableColours[i].withAlpha(0.3f));
            g.drawEllipse(tb.reduced(0.5f), 1.5f);
        }
    }
}

// --- Mouse interaction ---

void PatchHeaderBar::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

    // Morph knob drag
    int morphIdx = getMorphKnobAt(pos);
    if (morphIdx >= 0 && patch)
    {
        dragState.morphIndex = morphIdx;
        dragState.startValue = patch->morphValues[static_cast<size_t>(morphIdx)];
        dragState.startPos = pos;
        dragState.lastSentValue = -1;
        dragState.lastSendTime = 0;
        return;
    }

    // Voice arrows
    auto arrow = getVoiceArrowAt(pos);
    if (arrow != ArrowHit::None && patch)
    {
        auto& hdr = patch->getHeader();
        if (arrow == ArrowHit::Up)
            hdr.voices = juce::jmin(32, hdr.voices + 1);
        else
            hdr.voices = juce::jmax(1, hdr.voices - 1);
        repaint();
        if (voiceChangeCallback)
            voiceChangeCallback(hdr.voices);
        return;
    }

    // Cable toggles
    int cableIdx = getCableToggleAt(pos);
    if (cableIdx >= 0)
    {
        toggleCableVisibility(cableIdx);
        return;
    }
}

void PatchHeaderBar::mouseDrag(const juce::MouseEvent& e)
{
    if (dragState.morphIndex < 0 || patch == nullptr)
        return;

    int deltaY = dragState.startPos.y - e.getPosition().y;
    int newVal = juce::jlimit(0, 127, dragState.startValue + static_cast<int>(deltaY * 0.5f));
    patch->morphValues[static_cast<size_t>(dragState.morphIndex)] = newVal;
    repaint();

    if (morphChangeCallback && newVal != dragState.lastSentValue)
    {
        auto now = juce::Time::getMillisecondCounter();
        if (now - dragState.lastSendTime >= paramSendIntervalMs)
        {
            morphChangeCallback(dragState.morphIndex, newVal);
            dragState.lastSentValue = newVal;
            dragState.lastSendTime = now;
        }
    }
}

void PatchHeaderBar::mouseUp(const juce::MouseEvent&)
{
    if (dragState.morphIndex >= 0 && patch != nullptr && morphChangeCallback)
    {
        int finalVal = patch->morphValues[static_cast<size_t>(dragState.morphIndex)];
        if (finalVal != dragState.lastSentValue)
            morphChangeCallback(dragState.morphIndex, finalVal);
    }
    dragState = DragState();
}
