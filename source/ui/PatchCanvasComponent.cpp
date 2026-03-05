#include "PatchCanvasComponent.h"
#include <cmath>
#include <set>

// --- PatchCanvas (inner scrollable surface) ---

PatchCanvas::PatchCanvas()
{
    setSize(canvasWidth, canvasHeight);
}

void PatchCanvas::setPatch(Patch* p, const ModuleDescriptions* md, const ThemeData* td)
{
    patch = p;
    moduleDescs = md;
    themeData = td;
    repaint();
}

juce::Rectangle<int> PatchCanvas::getModuleBounds(const Module& m, int yOffset) const
{
    auto pos = m.getPosition();
    int x = pos.x * gridX;
    int y = yOffset + pos.y * gridY;
    int h = m.getDescriptor()->height * gridY;
    return { x, y, gridX, h };
}

const Parameter* PatchCanvas::findParameter(const Module& m, const juce::String& componentId) const
{
    if (componentId.isEmpty())
        return nullptr;

    for (auto& p : m.getParameters())
    {
        if (p.getDescriptor()->componentId == componentId)
            return &p;
    }

    // Log miss only once per module type + componentId (avoid flooding)
    static std::set<juce::String> logged;
    auto key = m.getDescriptor()->componentId + ":" + componentId;
    if (logged.find(key) == logged.end())
    {
        logged.insert(key);
        DBG("findParameter MISS: module=" + m.getDescriptor()->componentId
            + " (" + m.getTitle() + ") param=" + componentId
            + " [has " + juce::String(m.getParameters().size()) + " params: "
            + [&]() {
                juce::String ids;
                for (auto& p : m.getParameters())
                    ids += p.getDescriptor()->componentId + " ";
                return ids;
            }() + "]");
    }
    return nullptr;
}

const ThemeConnector* PatchCanvas::findThemeConnector(const ModuleTheme& theme, const juce::String& componentId) const
{
    for (auto& tc : theme.connectors)
    {
        if (tc.componentId == componentId)
            return &tc;
    }
    return nullptr;
}

juce::Point<int> PatchCanvas::getConnectorPosition(const Module& m, const Connector& conn, int yOffset) const
{
    auto bounds = getModuleBounds(m, yOffset);

    // Try theme-based position first
    if (themeData != nullptr)
    {
        auto* theme = themeData->getModuleTheme(m.getDescriptor()->componentId);
        if (theme != nullptr)
        {
            auto* tc = findThemeConnector(*theme, conn.getDescriptor()->componentId);
            if (tc != nullptr)
                return { bounds.getX() + tc->x + tc->size / 2,
                         bounds.getY() + tc->y + tc->size / 2 };
        }
    }

    // Fallback: geometric distribution
    auto* desc = conn.getDescriptor();

    int outputIdx = 0, inputIdx = 0;
    int thisOutputIdx = -1, thisInputIdx = -1;
    for (auto& c : m.getConnectors())
    {
        if (c.getDescriptor()->isOutput)
        {
            if (&c == &conn) thisOutputIdx = outputIdx;
            outputIdx++;
        }
        else
        {
            if (&c == &conn) thisInputIdx = inputIdx;
            inputIdx++;
        }
    }

    int moduleH = bounds.getHeight();
    int headerH = 14;

    if (desc->isOutput)
    {
        int spacing = (outputIdx > 0) ? (moduleH - headerH) / outputIdx : moduleH;
        int connY = bounds.getY() + headerH + thisOutputIdx * spacing + spacing / 2;
        return { bounds.getRight() - 4, connY };
    }
    else
    {
        int spacing = (inputIdx > 0) ? (moduleH - headerH) / inputIdx : moduleH;
        int connY = bounds.getY() + headerH + thisInputIdx * spacing + spacing / 2;
        return { bounds.getX() + 4, connY };
    }
}

void PatchCanvas::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff12122a));

    // Draw grid lines at column/row boundaries
    g.setColour(juce::Colour(0xff1a1a3a));
    auto clip = g.getClipBounds();

    int startX = (clip.getX() / gridX) * gridX;
    int startY = (clip.getY() / gridY) * gridY;

    for (int x = startX; x < clip.getRight(); x += gridX)
        g.drawVerticalLine(x, static_cast<float>(clip.getY()), static_cast<float>(clip.getBottom()));

    for (int y = startY; y < clip.getBottom(); y += gridY)
        g.drawHorizontalLine(y, static_cast<float>(clip.getX()), static_cast<float>(clip.getRight()));

    if (patch == nullptr)
        return;

    // Calculate separator position
    int separatorY = patch->getHeader().separatorPosition * gridY;
    if (separatorY == 0)
        separatorY = canvasHeight / 2;

    // Draw separator line
    g.setColour(juce::Colour(0xff5555aa));
    g.drawHorizontalLine(separatorY, 0.0f, static_cast<float>(canvasWidth));

    // Area labels
    g.setColour(juce::Colour(0xff8888cc));
    g.setFont(juce::FontOptions(12.0f));
    g.drawText("POLY", 4, 4, 60, 14, juce::Justification::centredLeft);
    g.drawText("COMMON", 4, separatorY + 4, 80, 14, juce::Justification::centredLeft);

    // Paint modules first, then cables on top
    paintModules(g, patch->getPolyVoiceArea(), polyAreaOffsetY);
    paintModules(g, patch->getCommonArea(), separatorY);

    paintCables(g, patch->getPolyVoiceArea(), polyAreaOffsetY);
    paintCables(g, patch->getCommonArea(), separatorY);
}

void PatchCanvas::paintModules(juce::Graphics& g, const ModuleContainer& container, int yOffset)
{
    for (auto& modulePtr : container.getModules())
    {
        auto& m = *modulePtr;
        auto rect = getModuleBounds(m, yOffset);

        // Check if module is visible in clip region
        if (!g.getClipBounds().intersects(rect))
            continue;

        const ModuleTheme* theme = nullptr;
        if (themeData != nullptr)
            theme = themeData->getModuleTheme(m.getDescriptor()->componentId);

        if (theme != nullptr)
            paintModuleThemed(g, m, rect, *theme);
        else
            paintModuleFallback(g, m, rect);
    }
}

void PatchCanvas::paintModuleThemed(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    paintModuleBackground(g, m, bounds, theme);
    paintCustomDisplays(g, m, bounds, theme);
    paintLabels(g, bounds, theme);
    paintTextDisplays(g, m, bounds, theme);
    paintSliders(g, m, bounds, theme);
    paintKnobs(g, m, bounds, theme);
    paintButtons(g, m, bounds, theme);
    paintConnectors(g, m, bounds, theme);
    paintLights(g, bounds, theme);
}

void PatchCanvas::paintModuleBackground(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& /*theme*/)
{
    auto bgColour = m.getDescriptor()->background;

    // Module body
    g.setColour(bgColour);
    g.fillRoundedRectangle(bounds.toFloat(), 3.0f);

    // Title bar (top 12px, slightly darker)
    auto titleBar = juce::Rectangle<int>(bounds.getX(), bounds.getY(), bounds.getWidth(), 12);
    g.setColour(bgColour.darker(0.15f));
    juce::Path titlePath;
    titlePath.addRoundedRectangle(static_cast<float>(titleBar.getX()), static_cast<float>(titleBar.getY()),
                                  static_cast<float>(titleBar.getWidth()), static_cast<float>(titleBar.getHeight()),
                                  3.0f, 3.0f, true, true, false, false);
    g.fillPath(titlePath);

    // Title text
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(9.0f));
    g.drawText(m.getTitle(), titleBar.reduced(4, 0), juce::Justification::centredLeft, true);

    // Border
    g.setColour(bgColour.brighter(0.3f));
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 3.0f, 1.0f);
}

void PatchCanvas::paintConnectors(juce::Graphics& g, const Module& /*m*/, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    for (auto& tc : theme.connectors)
    {
        float cx = static_cast<float>(bounds.getX() + tc.x);
        float cy = static_cast<float>(bounds.getY() + tc.y);
        float sz = static_cast<float>(tc.size);

        // Determine color from CSS class
        juce::Colour connColour = juce::Colours::white;
        if (tc.cssClass == "cAUDIO")         connColour = juce::Colour(0xffCB4F4F);
        else if (tc.cssClass == "cCONTROL")  connColour = juce::Colour(0xff5A5FB3);
        else if (tc.cssClass == "cLOGIC")    connColour = juce::Colour(0xffE5DE45);
        else if (tc.cssClass == "cSLAVE")    connColour = juce::Colour(0xffA8A8A8);
        else if (tc.cssClass == "cUSER1")    connColour = juce::Colour(0xff9AC899);
        else if (tc.cssClass == "cUSER2")    connColour = juce::Colour(0xffBB00D7);

        // Filled circle
        g.setColour(connColour);
        g.fillEllipse(cx, cy, sz, sz);

        // Dark outline
        g.setColour(juce::Colour(0xff222222));
        g.drawEllipse(cx, cy, sz, sz, 1.0f);
    }
}

void PatchCanvas::paintLabels(juce::Graphics& g, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    g.setColour(juce::Colours::black);
    g.setFont(juce::FontOptions(9.0f));

    for (auto& label : theme.labels)
    {
        g.drawText(label.text,
                   bounds.getX() + label.x, bounds.getY() + label.y,
                   120, 12,
                   juce::Justification::centredLeft, true);
    }
}

void PatchCanvas::paintKnobs(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    for (auto& tk : theme.knobs)
    {
        float cx = static_cast<float>(bounds.getX() + tk.x);
        float cy = static_cast<float>(bounds.getY() + tk.y);
        float sz = static_cast<float>(tk.size);

        // Knob background circle
        g.setColour(juce::Colour(0xff989898));
        g.fillEllipse(cx, cy, sz, sz);

        // Knob outline
        g.setColour(juce::Colour(0xff666666));
        g.drawEllipse(cx, cy, sz, sz, 1.0f);

        // Grip line — get parameter value for rotation
        float normalized = 0.5f;
        auto* param = findParameter(m, tk.componentId);
        if (param != nullptr)
        {
            auto* pd = param->getDescriptor();
            int range = pd->maxValue - pd->minValue;
            if (range > 0)
                normalized = static_cast<float>(param->getValue() - pd->minValue) / static_cast<float>(range);
        }

        // 225-degree range: from 7 o'clock (-135 deg) to 5 o'clock (+135 deg)
        float angle = (-135.0f + normalized * 270.0f) * (juce::MathConstants<float>::pi / 180.0f);
        float centerX = cx + sz * 0.5f;
        float centerY = cy + sz * 0.5f;
        float radius = sz * 0.5f;

        float innerR = radius * 0.3f;
        float outerR = radius * 0.85f;

        float sinA = std::sin(angle);
        float cosA = std::cos(angle);

        // Grip line from inner to outer radius
        g.setColour(juce::Colours::white);
        g.drawLine(centerX + sinA * innerR, centerY - cosA * innerR,
                   centerX + sinA * outerR, centerY - cosA * outerR, 1.5f);
    }
}

void PatchCanvas::paintButtons(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    for (auto& tb : theme.buttons)
    {
        float bx = static_cast<float>(bounds.getX() + tb.x);
        float by = static_cast<float>(bounds.getY() + tb.y);
        float bw = static_cast<float>(tb.width);
        float bh = static_cast<float>(tb.height);

        auto* param = findParameter(m, tb.componentId);
        int val = (param != nullptr) ? param->getValue() : 0;

        // --- Increment buttons: draw arrow pairs ---
        if (tb.isIncrement)
        {
            g.setColour(juce::Colour(0xff3a3a3a));
            g.fillRect(bx, by, bw, bh);
            g.setColour(juce::Colour(0xff555555));
            g.drawRect(bx, by, bw, bh, 1.0f);

            g.setColour(juce::Colour(0xffcccccc));
            float cx = bx + bw * 0.5f;
            float cy = by + bh * 0.5f;

            if (tb.landscape)
            {
                float half = bw * 0.5f;
                // Left arrow
                juce::Path leftArr;
                leftArr.addTriangle(bx + half * 0.25f, cy,
                                    bx + half * 0.75f, cy - bh * 0.3f,
                                    bx + half * 0.75f, cy + bh * 0.3f);
                g.fillPath(leftArr);
                // Right arrow
                juce::Path rightArr;
                rightArr.addTriangle(bx + half + half * 0.75f, cy,
                                     bx + half + half * 0.25f, cy - bh * 0.3f,
                                     bx + half + half * 0.25f, cy + bh * 0.3f);
                g.fillPath(rightArr);
            }
            else
            {
                float half = bh * 0.5f;
                // Up arrow
                juce::Path upArr;
                upArr.addTriangle(cx, by + half * 0.2f,
                                  cx - bw * 0.3f, by + half * 0.8f,
                                  cx + bw * 0.3f, by + half * 0.8f);
                g.fillPath(upArr);
                // Down arrow
                juce::Path downArr;
                downArr.addTriangle(cx, by + half + half * 0.8f,
                                    cx - bw * 0.3f, by + half + half * 0.2f,
                                    cx + bw * 0.3f, by + half + half * 0.2f);
                g.fillPath(downArr);
            }
            continue;
        }

        int numOptions = static_cast<int>(tb.labels.size());

        // --- Radio-selector buttons (cyclic=false, multiple options) ---
        if (!tb.cyclic && numOptions > 1)
        {
            // Draw segmented button showing all options, highlight selected
            for (int i = 0; i < numOptions; i++)
            {
                float segX, segY, segW, segH;

                if (tb.landscape)
                {
                    segW = bw / static_cast<float>(numOptions);
                    segH = bh;
                    segX = bx + static_cast<float>(i) * segW;
                    segY = by;
                }
                else
                {
                    segW = bw;
                    segH = bh / static_cast<float>(numOptions);
                    segX = bx;
                    segY = by + static_cast<float>(i) * segH;
                }

                bool selected = (i == val);

                // Segment background
                g.setColour(selected ? juce::Colour(0xff5566aa) : juce::Colour(0xff3a3a3a));
                g.fillRect(segX, segY, segW, segH);

                // Segment border
                g.setColour(juce::Colour(0xff555555));
                g.drawRect(segX, segY, segW, segH, 0.5f);

                // Label
                juce::String segLabel;
                if (i < numOptions)
                    segLabel = tb.labels[static_cast<size_t>(i)];
                // Image-only option: show index
                if (segLabel.isEmpty())
                    segLabel = juce::String(i);

                float fontSize = juce::jmin(8.0f, juce::jmin(segW * 0.8f, segH - 2.0f));
                if (fontSize < 4.0f) fontSize = 4.0f;

                g.setColour(selected ? juce::Colours::white : juce::Colour(0xffaaaaaa));
                g.setFont(juce::FontOptions(fontSize));
                g.drawText(segLabel,
                           static_cast<int>(segX), static_cast<int>(segY),
                           static_cast<int>(segW), static_cast<int>(segH),
                           juce::Justification::centred, true);
            }

            // Overall border
            g.setColour(juce::Colour(0xff666666));
            g.drawRect(bx, by, bw, bh, 1.0f);
            continue;
        }

        // --- Toggle buttons (cyclic=true) or single-option ---
        bool isOn = (val > 0);

        g.setColour(isOn ? juce::Colour(0xff505060) : juce::Colour(0xff3a3a3a));
        g.fillRect(bx, by, bw, bh);

        g.setColour(isOn ? juce::Colour(0xff7777aa) : juce::Colour(0xff555555));
        g.drawRect(bx, by, bw, bh, 1.0f);

        // Determine label text for current state
        juce::String labelText;
        if (!tb.labels.empty())
        {
            if (val >= 0 && val < numOptions)
                labelText = tb.labels[static_cast<size_t>(val)];
            else
                labelText = tb.labels[0];
        }

        // Image-only toggle: show on/off indicator
        if (labelText.isEmpty())
        {
            if (param != nullptr)
                labelText = juce::String(val);
        }

        if (labelText.isNotEmpty())
        {
            g.setColour(isOn ? juce::Colours::white : juce::Colour(0xffcccccc));
            g.setFont(juce::FontOptions(juce::jmin(8.0f, bh - 2.0f)));
            g.drawText(labelText,
                       static_cast<int>(bx), static_cast<int>(by),
                       static_cast<int>(bw), static_cast<int>(bh),
                       juce::Justification::centred, true);
        }
    }
}

void PatchCanvas::paintSliders(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    for (auto& ts : theme.sliders)
    {
        float sx = static_cast<float>(bounds.getX() + ts.x);
        float sy = static_cast<float>(bounds.getY() + ts.y);
        float sw = static_cast<float>(ts.width);
        float sh = static_cast<float>(ts.height);

        // Track background
        g.setColour(juce::Colour(0xff2a2a2a));
        g.fillRect(sx, sy, sw, sh);

        // Track border
        g.setColour(juce::Colour(0xff444444));
        g.drawRect(sx, sy, sw, sh, 1.0f);

        // Get parameter value for grip position
        float normalized = 0.5f;
        auto* param = findParameter(m, ts.componentId);
        if (param != nullptr)
        {
            auto* pd = param->getDescriptor();
            int range = pd->maxValue - pd->minValue;
            if (range > 0)
                normalized = static_cast<float>(param->getValue() - pd->minValue) / static_cast<float>(range);
        }

        // Draw grip
        g.setColour(juce::Colour(0xffaaaaaa));
        bool vertical = (ts.orientation != "horizontal");
        if (vertical)
        {
            // Grip moves from bottom (0) to top (1)
            float gripH = 4.0f;
            float gripY = sy + sh - gripH - (sh - gripH) * normalized;
            g.fillRect(sx + 1.0f, gripY, sw - 2.0f, gripH);
        }
        else
        {
            float gripW = 4.0f;
            float gripX = sx + (sw - gripW) * normalized;
            g.fillRect(gripX, sy + 1.0f, gripW, sh - 2.0f);
        }
    }
}

void PatchCanvas::paintTextDisplays(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    for (auto& td : theme.textDisplays)
    {
        float dx = static_cast<float>(bounds.getX() + td.x);
        float dy = static_cast<float>(bounds.getY() + td.y);
        float dw = static_cast<float>(td.width);
        float dh = static_cast<float>(td.height);

        // Dark blue background
        g.setColour(juce::Colour(0xff392F7D));
        g.fillRect(dx, dy, dw, dh);

        // Value text
        auto* param = findParameter(m, td.componentId);
        if (param != nullptr)
        {
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(9.0f));
            g.drawText(juce::String(param->getValue()),
                       static_cast<int>(dx), static_cast<int>(dy),
                       static_cast<int>(dw), static_cast<int>(dh),
                       juce::Justification::centred, true);
        }
    }
}

void PatchCanvas::paintLights(juce::Graphics& g, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    for (auto& tl : theme.lights)
    {
        float lx = static_cast<float>(bounds.getX() + tl.x);
        float ly = static_cast<float>(bounds.getY() + tl.y);
        float lw = static_cast<float>(tl.width);
        float lh = static_cast<float>(tl.height);

        if (tl.type == "led")
        {
            // LED: small dark circle (off state)
            g.setColour(juce::Colour(0xff333333));
            g.fillEllipse(lx, ly, lw, lh);
            g.setColour(juce::Colour(0xff555555));
            g.drawEllipse(lx, ly, lw, lh, 0.5f);
        }
        else
        {
            // Meter: dark rect (off state)
            g.setColour(juce::Colour(0xff333333));
            g.fillRect(lx, ly, lw, lh);
        }
    }
}

void PatchCanvas::paintCustomDisplays(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    for (auto& cd : theme.customDisplays)
    {
        float dx = static_cast<float>(bounds.getX() + cd.x);
        float dy = static_cast<float>(bounds.getY() + cd.y);
        float dw = static_cast<float>(cd.width);
        float dh = static_cast<float>(cd.height);

        // Dark background
        g.setColour(juce::Colour(0xff1a1a2e));
        g.fillRect(dx, dy, dw, dh);

        // Subtle border
        g.setColour(juce::Colour(0xff444466));
        g.drawRect(dx, dy, dw, dh, 0.5f);

        auto type = cd.type;

        // --- Envelope displays (ADSR, AD, AHD, multi-env) ---
        if (type == "adsr-envelope" || type == "adsr-mod-envelope"
            || type == "ad-envelope" || type == "ahd-envelope"
            || type == "multi-env-display")
        {
            // Draw a generic envelope shape using module parameters
            // Try to find attack/decay/sustain/release params
            float a = 0.3f, d = 0.3f, s = 0.7f, r = 0.3f;

            // Search for params by name heuristic
            for (auto& p : m.getParameters())
            {
                auto name = p.getDescriptor()->name.toLowerCase();
                auto* pd = p.getDescriptor();
                int range = pd->maxValue - pd->minValue;
                float norm = (range > 0) ? static_cast<float>(p.getValue() - pd->minValue) / static_cast<float>(range) : 0.5f;

                if (name.contains("attack"))       a = norm;
                else if (name.contains("decay"))   d = norm;
                else if (name.contains("sustain")) s = norm;
                else if (name.contains("release")) r = norm;
            }

            // AHD: hold instead of sustain+release
            bool isAD = (type == "ad-envelope");
            bool isAHD = (type == "ahd-envelope");

            float margin = 2.0f;
            float plotW = dw - margin * 2;
            float plotH = dh - margin * 2;
            float baseY = dy + dh - margin;
            float startX = dx + margin;

            juce::Path env;
            if (isAD)
            {
                float ax = startX + a * plotW * 0.5f;
                float dEnd = startX + plotW;
                env.startNewSubPath(startX, baseY);
                env.quadraticTo(startX + (ax - startX) * 0.5f, dy + margin, ax, dy + margin);
                env.quadraticTo(ax + (dEnd - ax) * 0.5f, baseY * 0.3f + (dy + margin) * 0.7f, dEnd, baseY);
            }
            else if (isAHD)
            {
                float ax = startX + a * plotW * 0.3f;
                float hEnd = ax + plotW * 0.3f;
                float dEnd = startX + plotW;
                env.startNewSubPath(startX, baseY);
                env.quadraticTo(startX + (ax - startX) * 0.5f, dy + margin, ax, dy + margin);
                env.lineTo(hEnd, dy + margin);
                env.quadraticTo(hEnd + (dEnd - hEnd) * 0.5f, baseY, dEnd, baseY);
            }
            else
            {
                // ADSR
                float segW = plotW * 0.25f;
                float ax = startX + a * segW;
                float dx2 = ax + d * segW;
                float susY = baseY - s * plotH;
                float rx = startX + segW * 3.0f;
                float rEnd = rx + r * segW;

                env.startNewSubPath(startX, baseY);
                env.quadraticTo(startX + (ax - startX) * 0.4f, dy + margin, ax, dy + margin);
                env.quadraticTo(ax + (dx2 - ax) * 0.6f, susY, dx2, susY);
                env.lineTo(rx, susY);
                env.quadraticTo(rx + (rEnd - rx) * 0.6f, baseY, rEnd, baseY);
            }

            g.setColour(juce::Colour(0xff55cc55));
            g.strokePath(env, juce::PathStrokeType(1.2f));
            continue;
        }

        // --- LFO Display ---
        if (type == "LFODisplay")
        {
            // Find waveform shape parameter
            int waveform = 0;
            for (auto& p : m.getParameters())
            {
                auto name = p.getDescriptor()->name.toLowerCase();
                if (name.contains("waveform") || name.contains("wave") || name.contains("shape"))
                {
                    waveform = p.getValue();
                    break;
                }
            }

            float margin = 2.0f;
            float plotW = dw - margin * 2;
            float plotH = dh - margin * 2;
            float midY = dy + dh * 0.5f;
            float amp = plotH * 0.45f;

            // Center reference line
            g.setColour(juce::Colour(0xff333355));
            g.drawHorizontalLine(static_cast<int>(midY), dx + margin, dx + dw - margin);

            juce::Path wave;
            int steps = static_cast<int>(plotW);
            for (int i = 0; i <= steps; i++)
            {
                float t = static_cast<float>(i) / static_cast<float>(steps);
                float px = dx + margin + t * plotW;
                float val = 0.0f;

                switch (waveform)
                {
                    case 0: // Sine
                        val = std::sin(t * juce::MathConstants<float>::twoPi);
                        break;
                    case 1: // Triangle
                        val = (t < 0.25f) ? t * 4.0f : (t < 0.75f) ? 2.0f - t * 4.0f : t * 4.0f - 4.0f;
                        break;
                    case 2: // Sawtooth
                        val = (t < 0.5f) ? t * 2.0f : t * 2.0f - 2.0f;
                        break;
                    case 3: // Inv sawtooth
                        val = (t < 0.5f) ? -t * 2.0f : 2.0f - t * 2.0f;
                        break;
                    case 4: // Square
                    default:
                        val = (t < 0.5f) ? 1.0f : -1.0f;
                        break;
                }

                float py = midY - val * amp;
                if (i == 0)
                    wave.startNewSubPath(px, py);
                else
                    wave.lineTo(px, py);
            }

            g.setColour(juce::Colour(0xff55aaff));
            g.strokePath(wave, juce::PathStrokeType(1.0f));
            continue;
        }

        // --- Overdrive / Clip / WaveWrap displays ---
        if (type == "overdrive-display" || type == "clip-display" || type == "wavewrap-display")
        {
            float amount = 0.5f;
            for (auto& p : m.getParameters())
            {
                auto name = p.getDescriptor()->name.toLowerCase();
                if (name.contains("overdrive") || name.contains("clip") || name.contains("wrap"))
                {
                    auto* pd = p.getDescriptor();
                    int range = pd->maxValue - pd->minValue;
                    if (range > 0)
                        amount = static_cast<float>(p.getValue() - pd->minValue) / static_cast<float>(range);
                    break;
                }
            }

            float margin = 2.0f;
            float plotW = dw - margin * 2;
            float plotH = dh - margin * 2;

            // Reference diagonal
            g.setColour(juce::Colour(0xff333355));
            g.drawLine(dx + margin, dy + dh - margin, dx + dw - margin, dy + margin, 0.5f);

            // Transfer curve
            juce::Path curve;
            int steps = static_cast<int>(plotW);
            float drive = 1.0f + amount * 8.0f;

            for (int i = 0; i <= steps; i++)
            {
                float t = static_cast<float>(i) / static_cast<float>(steps);
                float val;

                if (type == "wavewrap-display")
                    val = std::sin(t * juce::MathConstants<float>::pi * (1.0f + amount * 4.0f)) * 0.5f + 0.5f;
                else
                    val = std::tanh(t * drive) / std::tanh(drive); // soft clipping

                float px = dx + margin + t * plotW;
                float py = dy + dh - margin - val * plotH;

                if (i == 0)
                    curve.startNewSubPath(px, py);
                else
                    curve.lineTo(px, py);
            }

            g.setColour(juce::Colour(0xffff8844));
            g.strokePath(curve, juce::PathStrokeType(1.2f));
            continue;
        }

        // --- Filter displays ---
        if (type == "filter-e-display" || type == "filter-f-display")
        {
            float cutoff = 0.5f, reso = 0.0f;
            int filterType = 0;

            for (auto& p : m.getParameters())
            {
                auto name = p.getDescriptor()->name.toLowerCase();
                auto* pd = p.getDescriptor();
                int range = pd->maxValue - pd->minValue;
                float norm = (range > 0) ? static_cast<float>(p.getValue() - pd->minValue) / static_cast<float>(range) : 0.5f;

                if (name.contains("freq") || name.contains("cutoff"))  cutoff = norm;
                else if (name.contains("reson"))                        reso = norm;
                else if (name.contains("filter type") || name.contains("type")) filterType = p.getValue();
            }

            float margin = 2.0f;
            float plotW = dw - margin * 2;
            float plotH = dh - margin * 2;
            float midY = dy + dh * 0.5f;

            // Reference line
            g.setColour(juce::Colour(0xff333355));
            g.drawHorizontalLine(static_cast<int>(midY), dx + margin, dx + dw - margin);

            juce::Path filt;
            int steps = static_cast<int>(plotW);
            for (int i = 0; i <= steps; i++)
            {
                float t = static_cast<float>(i) / static_cast<float>(steps);
                float freq = t;
                float response = 0.0f;

                float cutFreq = cutoff;
                float dist = (freq - cutFreq) * 4.0f;
                float resoBoost = reso * 0.5f * std::exp(-dist * dist * 8.0f);

                switch (filterType)
                {
                    case 0: // LP
                        response = 1.0f / (1.0f + std::exp(dist * 6.0f)) + resoBoost;
                        break;
                    case 1: // BP
                        response = std::exp(-dist * dist * 4.0f) * (0.8f + resoBoost);
                        break;
                    case 2: // HP
                        response = 1.0f / (1.0f + std::exp(-dist * 6.0f)) + resoBoost;
                        break;
                    case 3: // BR (notch)
                        response = 1.0f - std::exp(-dist * dist * 4.0f) * 0.8f;
                        break;
                    default:
                        response = 1.0f / (1.0f + std::exp(dist * 6.0f)) + resoBoost;
                        break;
                }

                response = juce::jlimit(0.0f, 1.0f, response);
                float px = dx + margin + t * plotW;
                float py = dy + dh - margin - response * plotH;

                if (i == 0)
                    filt.startNewSubPath(px, py);
                else
                    filt.lineTo(px, py);
            }

            g.setColour(juce::Colour(0xff55aaff));
            g.strokePath(filt, juce::PathStrokeType(1.2f));
            continue;
        }

        // --- EQ displays ---
        if (type == "eq-mid-display" || type == "eq-shelving-display")
        {
            float freq = 0.5f, gain = 0.5f;
            for (auto& p : m.getParameters())
            {
                auto name = p.getDescriptor()->name.toLowerCase();
                auto* pd = p.getDescriptor();
                int range = pd->maxValue - pd->minValue;
                float norm = (range > 0) ? static_cast<float>(p.getValue() - pd->minValue) / static_cast<float>(range) : 0.5f;

                if (name.contains("freq"))      freq = norm;
                else if (name.contains("gain")) gain = norm;
            }

            float margin = 2.0f;
            float plotW = dw - margin * 2;
            float plotH = dh - margin * 2;
            float midY = dy + dh * 0.5f;

            g.setColour(juce::Colour(0xff333355));
            g.drawHorizontalLine(static_cast<int>(midY), dx + margin, dx + dw - margin);

            juce::Path eq;
            int steps = static_cast<int>(plotW);
            float gainAmt = (gain - 0.5f) * 2.0f; // -1 to +1

            for (int i = 0; i <= steps; i++)
            {
                float t = static_cast<float>(i) / static_cast<float>(steps);
                float response;

                if (type == "eq-shelving-display")
                {
                    // Shelf: sigmoid transition
                    response = 0.5f + gainAmt * 0.4f / (1.0f + std::exp(-(t - freq) * 12.0f));
                }
                else
                {
                    // Parametric peak
                    float dist = (t - freq) * 5.0f;
                    response = 0.5f + gainAmt * 0.4f * std::exp(-dist * dist);
                }

                float px = dx + margin + t * plotW;
                float py = dy + dh - margin - response * plotH;

                if (i == 0)
                    eq.startNewSubPath(px, py);
                else
                    eq.lineTo(px, py);
            }

            g.setColour(juce::Colour(0xffaaaa55));
            g.strokePath(eq, juce::PathStrokeType(1.2f));
            continue;
        }

        // --- Compressor / Expander displays ---
        if (type == "compressor-display" || type == "expander-display")
        {
            float threshold = 0.5f, ratio = 0.5f;
            for (auto& p : m.getParameters())
            {
                auto name = p.getDescriptor()->name.toLowerCase();
                auto* pd = p.getDescriptor();
                int range = pd->maxValue - pd->minValue;
                float norm = (range > 0) ? static_cast<float>(p.getValue() - pd->minValue) / static_cast<float>(range) : 0.5f;

                if (name.contains("threshold") || name.contains("thresh")) threshold = norm;
                else if (name.contains("ratio"))                            ratio = norm;
            }

            float margin = 2.0f;
            float plotW = dw - margin * 2;
            float plotH = dh - margin * 2;

            // Unity diagonal reference
            g.setColour(juce::Colour(0xff333355));
            g.drawLine(dx + margin, dy + dh - margin, dx + dw - margin, dy + margin, 0.5f);

            juce::Path curve;
            int steps = static_cast<int>(plotW);
            float ratioVal = 1.0f + ratio * 8.0f;

            for (int i = 0; i <= steps; i++)
            {
                float t = static_cast<float>(i) / static_cast<float>(steps);
                float output;

                if (type == "compressor-display")
                {
                    if (t < threshold)
                        output = t;
                    else
                        output = threshold + (t - threshold) / ratioVal;
                }
                else
                {
                    // Expander
                    if (t > threshold)
                        output = t;
                    else
                        output = threshold - (threshold - t) * ratioVal;
                    output = juce::jmax(0.0f, output);
                }

                float px = dx + margin + t * plotW;
                float py = dy + dh - margin - output * plotH;

                if (i == 0)
                    curve.startNewSubPath(px, py);
                else
                    curve.lineTo(px, py);
            }

            g.setColour(juce::Colour(0xffcc5555));
            g.strokePath(curve, juce::PathStrokeType(1.2f));
            continue;
        }

        // --- Phaser display ---
        if (type == "phaser-display")
        {
            float margin = 2.0f;
            float plotW = dw - margin * 2;
            float plotH = dh - margin * 2;
            float midY = dy + dh * 0.5f;

            g.setColour(juce::Colour(0xff333355));
            g.drawHorizontalLine(static_cast<int>(midY), dx + margin, dx + dw - margin);

            int nPeaks = 3;
            for (auto& p : m.getParameters())
            {
                if (p.getDescriptor()->name.toLowerCase().contains("peak"))
                {
                    nPeaks = juce::jlimit(1, 6, p.getValue());
                    break;
                }
            }

            juce::Path wave;
            int steps = static_cast<int>(plotW);
            for (int i = 0; i <= steps; i++)
            {
                float t = static_cast<float>(i) / static_cast<float>(steps);
                float val = std::sin(t * juce::MathConstants<float>::pi * static_cast<float>(nPeaks)) * 0.4f + 0.5f;
                float px = dx + margin + t * plotW;
                float py = dy + dh - margin - val * plotH;

                if (i == 0)
                    wave.startNewSubPath(px, py);
                else
                    wave.lineTo(px, py);
            }

            g.setColour(juce::Colour(0xffaa55cc));
            g.strokePath(wave, juce::PathStrokeType(1.0f));
            continue;
        }

        // --- Fallback: label with type name ---
        auto label = cd.type;
        label = label.replace("-display", "").replace("-envelope", " env")
                     .replace("-editor", "").replace("Display", "");

        g.setColour(juce::Colour(0xff666688));
        g.setFont(juce::FontOptions(juce::jmin(7.0f, dh - 2.0f)));
        g.drawText(label,
                   static_cast<int>(dx + 1), static_cast<int>(dy + 1),
                   static_cast<int>(dw - 2), static_cast<int>(dh - 2),
                   juce::Justification::centred, true);
    }
}

void PatchCanvas::paintModuleFallback(juce::Graphics& g, const Module& m, juce::Rectangle<int> rect)
{
    // Original simple rendering for modules without theme data
    auto bgColour = m.getDescriptor()->background;
    g.setColour(bgColour);
    g.fillRoundedRectangle(rect.toFloat(), 3.0f);

    // Border
    g.setColour(bgColour.brighter(0.3f));
    g.drawRoundedRectangle(rect.toFloat().reduced(0.5f), 3.0f, 1.0f);

    // Title text
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText(m.getTitle(), rect.reduced(4, 1).removeFromTop(13),
               juce::Justification::centredLeft, true);

    // Draw connector dots at geometric positions
    for (auto& conn : m.getConnectors())
    {
        // Geometric fallback position
        auto* desc = conn.getDescriptor();
        int outputIdx = 0, inputIdx = 0;
        int thisOutputIdx = -1, thisInputIdx = -1;
        for (auto& c : m.getConnectors())
        {
            if (c.getDescriptor()->isOutput)
            {
                if (&c == &conn) thisOutputIdx = outputIdx;
                outputIdx++;
            }
            else
            {
                if (&c == &conn) thisInputIdx = inputIdx;
                inputIdx++;
            }
        }

        int moduleH = rect.getHeight();
        int headerH = 14;
        int px, py;

        if (desc->isOutput)
        {
            int spacing = (outputIdx > 0) ? (moduleH - headerH) / outputIdx : moduleH;
            py = rect.getY() + headerH + thisOutputIdx * spacing + spacing / 2;
            px = rect.getRight() - 4;
        }
        else
        {
            int spacing = (inputIdx > 0) ? (moduleH - headerH) / inputIdx : moduleH;
            py = rect.getY() + headerH + thisInputIdx * spacing + spacing / 2;
            px = rect.getX() + 4;
        }

        g.setColour(getSignalColour(desc->signalType));
        g.fillEllipse(static_cast<float>(px - 3), static_cast<float>(py - 3), 6.0f, 6.0f);
    }
}

void PatchCanvas::paintCables(juce::Graphics& g, const ModuleContainer& container, int yOffset)
{
    for (auto& conn : container.getConnections())
    {
        if (conn.output == nullptr || conn.input == nullptr)
            continue;

        // Find the modules that own these connectors
        const Module* srcModule = nullptr;
        const Module* dstModule = nullptr;

        for (auto& modulePtr : container.getModules())
        {
            for (auto& c : modulePtr->getConnectors())
            {
                if (&c == conn.output)
                    srcModule = modulePtr.get();
                if (&c == conn.input)
                    dstModule = modulePtr.get();
            }
        }

        if (srcModule == nullptr || dstModule == nullptr)
            continue;

        auto srcPos = getConnectorPosition(*srcModule, *conn.output, yOffset);
        auto dstPos = getConnectorPosition(*dstModule, *conn.input, yOffset);

        auto cableCol = getSignalColour(conn.output->getDescriptor()->signalType);

        // Draw a curved cable
        juce::Path path;
        path.startNewSubPath(srcPos.toFloat());

        float midY = (srcPos.y + dstPos.y) * 0.5f;
        float sag = std::abs(static_cast<float>(srcPos.x - dstPos.x)) * 0.15f + 15.0f;

        path.cubicTo(static_cast<float>(srcPos.x), midY + sag,
                     static_cast<float>(dstPos.x), midY + sag,
                     static_cast<float>(dstPos.x), static_cast<float>(dstPos.y));

        // Dark outline behind the cable for contrast
        g.setColour(juce::Colour(0xaa000000));
        g.strokePath(path, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        // Colored cable on top
        g.setColour(cableCol);
        g.strokePath(path, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }
}

// --- PatchCanvasComponent (viewport wrapper) ---

PatchCanvasComponent::PatchCanvasComponent()
{
    viewport.setViewedComponent(&canvas, false);
    viewport.setScrollBarsShown(true, true);
    addAndMakeVisible(viewport);
}

void PatchCanvasComponent::resized()
{
    viewport.setBounds(getLocalBounds());
}

void PatchCanvasComponent::setPatch(Patch* p, const ModuleDescriptions* md, const ThemeData* td)
{
    canvas.setPatch(p, md, td);
}
