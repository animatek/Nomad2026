#include "PatchCanvasComponent.h"
#include <cmath>
#include <set>

// --- PatchCanvas (inner scrollable surface) ---

PatchCanvas::PatchCanvas()
{
    setSize(canvasWidth, canvasHeight);
    setWantsKeyboardFocus(true);  // Enable keyboard input for Delete key
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

    // Cable creation preview (rubber-band cable)
    if (showCablePreview && dragState.sourceConnector != nullptr && dragState.module != nullptr)
    {
        int yOffset = (dragState.section == 1) ? polyAreaOffsetY : separatorY;
        auto srcPos = getConnectorPosition(*dragState.module, *dragState.sourceConnector, yOffset);

        juce::Path path;
        path.startNewSubPath(srcPos.toFloat());
        float midY = (srcPos.y + cablePreviewEnd.y) * 0.5f;
        float sag = std::abs(float(srcPos.x - cablePreviewEnd.x)) * 0.15f + 15.0f;
        path.cubicTo(static_cast<float>(srcPos.x), midY + sag,
                     static_cast<float>(cablePreviewEnd.x), midY + sag,
                     static_cast<float>(cablePreviewEnd.x), static_cast<float>(cablePreviewEnd.y));

        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.strokePath(path, juce::PathStrokeType(2.5f));
    }

    // Module drop preview (ghost outline)
    if (showModuleDropPreview && moduleDescs != nullptr)
    {
        auto* descriptor = moduleDescs->getModuleByIndex(dropPreviewTypeId);
        if (descriptor != nullptr)
        {
            int yOffset = (dropPreviewSection == 1) ? polyAreaOffsetY
                                                     : (patch ? patch->getHeader().separatorPosition * gridY : canvasHeight / 2);
            int x = dropPreviewGridX * gridX;
            int y = yOffset + dropPreviewGridY * gridY;
            int width = gridX;
            int height = descriptor->height * gridY;

            juce::Rectangle<int> previewBounds(x, y, width, height);

            // Draw semi-transparent module outline
            g.setColour(juce::Colours::cyan.withAlpha(0.3f));
            g.fillRoundedRectangle(previewBounds.toFloat(), 3.0f);

            g.setColour(juce::Colours::cyan.withAlpha(0.8f));
            g.drawRoundedRectangle(previewBounds.toFloat(), 3.0f, 2.0f);

            // Module name
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.setFont(juce::FontOptions(10.0f));
            g.drawText(descriptor->fullname, previewBounds.reduced(4, 4), juce::Justification::centred, true);
        }
    }
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

        // Draw selection border if this module is selected
        if (&m == selectedModule)
        {
            g.setColour(juce::Colours::yellow);
            g.drawRect(rect, 2);
        }
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

        // Check cable visibility by signal type
        if (patch != nullptr)
        {
            const auto& hdr = patch->getHeader();
            switch (conn.output->getDescriptor()->signalType)
            {
                case SignalType::Audio:       if (!hdr.cableVisRed)    continue; break;
                case SignalType::Control:     if (!hdr.cableVisBlue)   continue; break;
                case SignalType::Logic:       if (!hdr.cableVisYellow) continue; break;
                case SignalType::MasterSlave: if (!hdr.cableVisGray)   continue; break;
                case SignalType::User1:       if (!hdr.cableVisGreen)  continue; break;
                case SignalType::User2:       if (!hdr.cableVisPurple) continue; break;
                case SignalType::None:        if (!hdr.cableVisWhite)  continue; break;
            }
        }

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

// --- Mouse Event Handlers ---

void PatchCanvas::mouseDown(const juce::MouseEvent& e)
{
    if (patch == nullptr || themeData == nullptr)
        return;

    auto pos = e.getPosition();

    // Calculate separator position
    int separatorY = patch->getHeader().separatorPosition * gridY;
    if (separatorY == 0)
        separatorY = canvasHeight / 2;

    // Search both areas
    // Protocol convention: section 1 = poly, section 0 = common
    struct { ModuleContainer* container; int section; int yOffset; } areas[] = {
        { &patch->getPolyVoiceArea(), 1, polyAreaOffsetY },
        { &patch->getCommonArea(), 0, separatorY }
    };

    for (auto& area : areas)
    {
        for (auto& modulePtr : area.container->getModules())
        {
            auto& m = *modulePtr;
            auto rect = getModuleBounds(m, area.yOffset);

            if (!rect.contains(pos))
                continue;

            // Module was clicked, now test UI components
            auto* theme = themeData->getModuleTheme(m.getDescriptor()->componentId);
            if (theme == nullptr)
                continue;

            auto relPos = pos - rect.getPosition();

            // Test connectors FIRST (cable creation / deletion)
            for (auto& tc : theme->connectors)
            {
                juce::Rectangle<int> connRect(tc.x, tc.y, tc.size, tc.size);
                connRect = connRect.expanded(2);  // tolerance
                if (connRect.contains(relPos))
                {
                    auto* conn = findConnectorByComponentId(m, tc.componentId);
                    if (conn != nullptr)
                    {
                        if (e.mods.isRightButtonDown())
                        {
                            // Delete cables on this connector
                            area.container->removeConnectionsForConnector(conn);
                            repaint();
                            return;
                        }
                        // Start cable creation
                        dragState.type = DragState::CableCreate;
                        dragState.module = &m;
                        dragState.sourceConnector = conn;
                        dragState.section = area.section;
                        cablePreviewEnd = pos;
                        showCablePreview = true;
                        return;
                    }
                }
            }

            // Test knobs
            for (auto& tk : theme->knobs)
            {
                juce::Rectangle<int> knobRect(tk.x, tk.y, tk.size, tk.size);
                if (knobRect.contains(relPos))
                {
                    auto* param = findParameter(m, tk.componentId);
                    if (param != nullptr)
                    {
                        dragState.type = DragState::Knob;
                        dragState.module = &m;
                        dragState.parameter = const_cast<Parameter*>(param);
                        dragState.section = area.section;
                        dragState.startPos = pos;
                        dragState.startValue = param->getValue();
                        return;
                    }
                }
            }

            // Test sliders
            for (auto& ts : theme->sliders)
            {
                juce::Rectangle<int> sliderRect(ts.x, ts.y, ts.width, ts.height);
                if (sliderRect.contains(relPos))
                {
                    auto* param = findParameter(m, ts.componentId);
                    if (param != nullptr)
                    {
                        dragState.type = DragState::Slider;
                        dragState.module = &m;
                        dragState.parameter = const_cast<Parameter*>(param);
                        dragState.section = area.section;
                        dragState.startPos = pos;
                        dragState.startValue = param->getValue();
                        return;
                    }
                }
            }

            // Test buttons
            for (auto& tb : theme->buttons)
            {
                juce::Rectangle<int> btnRect(tb.x, tb.y, tb.width, tb.height);
                if (btnRect.contains(relPos))
                {
                    auto* param = findParameter(m, tb.componentId);
                    if (param != nullptr)
                    {
                        dragState.type = DragState::Button;
                        dragState.module = &m;
                        dragState.parameter = const_cast<Parameter*>(param);
                        dragState.section = area.section;
                        dragState.startPos = pos;
                        dragState.startValue = param->getValue();

                        // Buttons toggle/cycle on click (no drag)
                        auto* pd = param->getDescriptor();
                        int newValue = param->getValue() + 1;
                        if (newValue > pd->maxValue)
                            newValue = pd->minValue;

                        dragState.parameter->setValue(newValue);

                        if (parameterChangeCallback)
                            parameterChangeCallback(dragState.section, m.getContainerIndex(), pd->index, newValue);

                        repaint();
                        return;
                    }
                }
            }

            // Module body fallback → select and start module drag
            selectedModule = &m;
            selectedSection = area.section;
            dragState.type = DragState::ModuleMove;
            dragState.module = &m;
            dragState.section = area.section;
            dragState.startPos = pos;
            dragState.dragOffsetX = pos.x - rect.getX();
            dragState.dragOffsetY = pos.y - rect.getY();
            repaint();  // Repaint to show selection
            return;
        }
    }
}

void PatchCanvas::mouseDrag(const juce::MouseEvent& e)
{
    if (dragState.type == DragState::None)
        return;

    auto currentPos = e.getPosition();

    if (dragState.type == DragState::ModuleMove)
    {
        int separatorY = patch->getHeader().separatorPosition * gridY;
        if (separatorY == 0)
            separatorY = canvasHeight / 2;
        int yOffset = (dragState.section == 1) ? polyAreaOffsetY : separatorY;

        int newGridX = juce::jmax(0, (currentPos.x - dragState.dragOffsetX + gridX / 2) / gridX);
        int rawGridY = juce::jmax(0, (currentPos.y - dragState.dragOffsetY - yOffset + gridY / 2) / gridY);

        // Prevent overlap: snap to nearest free Y position in this column
        auto& container = patch->getContainer(dragState.section);
        int moduleHeight = dragState.module->getDescriptor()->height;
        int newGridY = findNearestFreeY(container, dragState.module, newGridX, rawGridY, moduleHeight);

        auto curPos = dragState.module->getPosition();
        if (newGridX != curPos.x || newGridY != curPos.y)
        {
            dragState.module->setPosition({ newGridX, newGridY });
            repaint();
        }
        return;
    }

    if (dragState.type == DragState::CableCreate)
    {
        cablePreviewEnd = currentPos;
        repaint();
        return;
    }

    if (dragState.parameter == nullptr)
        return;

    auto* pd = dragState.parameter->getDescriptor();
    int range = pd->maxValue - pd->minValue;
    if (range <= 0)
        return;

    int newValue = dragState.startValue;

    if (dragState.type == DragState::Knob)
    {
        // Rotary control: vertical drag (down = increase, up = decrease)
        int deltaY = dragState.startPos.y - currentPos.y;
        float sensitivity = 0.5f;  // Adjust for feel
        int valueDelta = static_cast<int>(deltaY * sensitivity);
        newValue = juce::jlimit(pd->minValue, pd->maxValue, dragState.startValue + valueDelta);
    }
    else if (dragState.type == DragState::Slider)
    {
        // Linear control: drag along slider axis
        // Determine if vertical or horizontal
        bool isVertical = true;  // Default, could check theme orientation

        if (isVertical)
        {
            // Vertical slider: drag up = increase
            int deltaY = dragState.startPos.y - currentPos.y;
            float normalized = static_cast<float>(deltaY) / 100.0f;  // 100px = full range
            int valueDelta = static_cast<int>(normalized * range);
            newValue = juce::jlimit(pd->minValue, pd->maxValue, dragState.startValue + valueDelta);
        }
        else
        {
            // Horizontal slider: drag right = increase
            int deltaX = currentPos.x - dragState.startPos.x;
            float normalized = static_cast<float>(deltaX) / 100.0f;
            int valueDelta = static_cast<int>(normalized * range);
            newValue = juce::jlimit(pd->minValue, pd->maxValue, dragState.startValue + valueDelta);
        }
    }

    // Update parameter and repaint
    if (newValue != dragState.parameter->getValue())
    {
        dragState.parameter->setValue(newValue);
        repaint();

        // Send to synth in real-time (rate limited)
        if (parameterChangeCallback && dragState.module != nullptr && newValue != dragState.lastSentValue)
        {
            auto now = juce::Time::getMillisecondCounter();
            if (now - dragState.lastSendTime >= paramSendIntervalMs)
            {
                parameterChangeCallback(dragState.section, dragState.module->getContainerIndex(), pd->index, newValue);
                dragState.lastSentValue = newValue;
                dragState.lastSendTime = now;
            }
        }
    }
}

void PatchCanvas::mouseUp(const juce::MouseEvent& e)
{
    if (dragState.type == DragState::None)
        return;

    if (dragState.type == DragState::ModuleMove)
    {
        dragState = DragState();
        return;
    }

    if (dragState.type == DragState::CableCreate)
    {
        showCablePreview = false;
        auto hit = findConnectorAt(e.getPosition());
        if (hit.connector != nullptr && hit.connector != dragState.sourceConnector
            && hit.section == dragState.section)
        {
            auto* src = dragState.sourceConnector;
            auto* dst = hit.connector;
            bool srcOut = src->getDescriptor()->isOutput;
            bool dstOut = dst->getDescriptor()->isOutput;
            auto& container = patch->getContainer(dragState.section);

            // Connect output→input, auto-swap if needed
            if (srcOut && !dstOut)
                container.addConnection(src, dst);
            else if (!srcOut && dstOut)
                container.addConnection(dst, src);
            // output→output or input→input: silently ignored
        }
        repaint();
        dragState = DragState();
        return;
    }

    if (dragState.parameter == nullptr)
    {
        dragState = DragState();
        return;
    }

    // Send final value to synth (only for knobs/sliders, buttons already sent on mouseDown)
    // Skip if the value was already sent during drag
    if (dragState.type != DragState::Button && parameterChangeCallback && dragState.module != nullptr)
    {
        int finalValue = dragState.parameter->getValue();
        if (finalValue != dragState.lastSentValue)
        {
            auto* pd = dragState.parameter->getDescriptor();
            parameterChangeCallback(dragState.section, dragState.module->getContainerIndex(), pd->index, finalValue);
        }
    }

    // Clear drag state
    dragState = DragState();
}

bool PatchCanvas::keyPressed(const juce::KeyPress& key)
{
    // Delete key removes selected module
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (selectedModule != nullptr && selectedSection >= 0 && patch != nullptr)
        {
            auto& container = patch->getContainer(selectedSection);
            container.removeModule(selectedModule);
            selectedModule = nullptr;
            selectedSection = -1;
            repaint();
            return true;
        }
    }
    return false;
}

bool PatchCanvas::isPositionFree(const ModuleContainer& container, const Module* exclude, int gx, int gy, int height) const
{
    for (auto& m : container.getModules())
    {
        if (m.get() == exclude)
            continue;
        auto pos = m->getPosition();
        if (pos.x != gx)
            continue;
        int mh = m->getDescriptor()->height;
        // Y ranges overlap if gy < pos.y + mh AND pos.y < gy + height
        if (gy < pos.y + mh && pos.y < gy + height)
            return false;
    }
    return true;
}

int PatchCanvas::findNearestFreeY(const ModuleContainer& container, const Module* exclude, int gx, int targetY, int height) const
{
    if (isPositionFree(container, exclude, gx, targetY, height))
        return targetY;

    // Search above and below alternately, return closest free slot
    for (int offset = 1; offset < 256; offset++)
    {
        int above = targetY - offset;
        if (above >= 0 && isPositionFree(container, exclude, gx, above, height))
            return above;
        int below = targetY + offset;
        if (isPositionFree(container, exclude, gx, below, height))
            return below;
    }
    return targetY; // fallback — should not happen
}

Connector* PatchCanvas::findConnectorByComponentId(Module& m, const juce::String& componentId)
{
    for (auto& c : m.getConnectors())
    {
        if (c.getDescriptor()->componentId == componentId)
            return &c;
    }
    return nullptr;
}

PatchCanvas::ConnectorHit PatchCanvas::findConnectorAt(juce::Point<int> pos)
{
    if (patch == nullptr || themeData == nullptr)
        return {};

    int separatorY = patch->getHeader().separatorPosition * gridY;
    if (separatorY == 0)
        separatorY = canvasHeight / 2;

    struct { ModuleContainer* container; int section; int yOffset; } areas[] = {
        { &patch->getPolyVoiceArea(), 1, polyAreaOffsetY },
        { &patch->getCommonArea(), 0, separatorY }
    };

    for (auto& area : areas)
    {
        for (auto& modulePtr : area.container->getModules())
        {
            auto& m = *modulePtr;
            auto rect = getModuleBounds(m, area.yOffset);
            if (!rect.contains(pos))
                continue;

            auto* theme = themeData->getModuleTheme(m.getDescriptor()->componentId);
            if (theme == nullptr)
                continue;

            auto relPos = pos - rect.getPosition();
            for (auto& tc : theme->connectors)
            {
                juce::Rectangle<int> connRect(tc.x, tc.y, tc.size, tc.size);
                connRect = connRect.expanded(2);
                if (connRect.contains(relPos))
                {
                    auto* conn = findConnectorByComponentId(m, tc.componentId);
                    if (conn != nullptr)
                        return { &m, conn, area.section };
                }
            }
        }
    }
    return {};
}

bool PatchCanvas::isDragging(int section, int moduleId, int parameterId) const
{
    if (dragState.type == DragState::None || dragState.module == nullptr || dragState.parameter == nullptr)
        return false;

    return dragState.section == section
        && dragState.module->getContainerIndex() == moduleId
        && dragState.parameter->getDescriptor()->index == parameterId;
}

// --- DragAndDropTarget implementation ---

bool PatchCanvas::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    // Accept module drags from ModuleBrowserPanel
    auto description = dragSourceDetails.description;
    if (!description.isObject())
        return false;

    auto* obj = description.getDynamicObject();
    if (obj == nullptr)
        return false;

    return obj->getProperty("type").toString() == "module";
}

void PatchCanvas::itemDragEnter(const SourceDetails& dragSourceDetails)
{
    if (!patch || !moduleDescs)
        return;

    auto* obj = dragSourceDetails.description.getDynamicObject();
    if (obj == nullptr)
        return;

    dropPreviewTypeId = obj->getProperty("typeId");
    showModuleDropPreview = true;
    repaint();
}

void PatchCanvas::itemDragMove(const SourceDetails& dragSourceDetails)
{
    if (!showModuleDropPreview || !patch || !moduleDescs)
        return;

    // Convert mouse position to grid coordinates
    auto mousePos = dragSourceDetails.localPosition;
    int separatorY = patch->getHeader().separatorPosition * gridY;
    if (separatorY == 0)
        separatorY = canvasHeight / 2;

    // Determine section based on Y position
    if (mousePos.y < separatorY)
    {
        // Poly area
        dropPreviewSection = 1;
        dropPreviewGridX = mousePos.x / gridX;
        dropPreviewGridY = (mousePos.y - polyAreaOffsetY) / gridY;
    }
    else
    {
        // Common area
        dropPreviewSection = 0;
        dropPreviewGridX = mousePos.x / gridX;
        dropPreviewGridY = (mousePos.y - separatorY) / gridY;
    }

    // Clamp to valid ranges
    dropPreviewGridX = juce::jlimit(0, 39, dropPreviewGridX);
    dropPreviewGridY = juce::jlimit(0, 127, dropPreviewGridY);

    repaint();
}

void PatchCanvas::itemDragExit(const SourceDetails& /*dragSourceDetails*/)
{
    showModuleDropPreview = false;
    repaint();
}

void PatchCanvas::itemDropped(const SourceDetails& dragSourceDetails)
{
    showModuleDropPreview = false;

    if (!patch || !moduleDescs)
        return;

    auto* obj = dragSourceDetails.description.getDynamicObject();
    if (obj == nullptr)
        return;

    int typeId = obj->getProperty("typeId");
    juce::String moduleName = obj->getProperty("name").toString();

    // Convert mouse position to grid coordinates (same as itemDragMove)
    auto mousePos = dragSourceDetails.localPosition;
    int separatorY = patch->getHeader().separatorPosition * gridY;
    if (separatorY == 0)
        separatorY = canvasHeight / 2;

    int section, dropX, dropY;
    if (mousePos.y < separatorY)
    {
        section = 1;  // Poly
        dropX = mousePos.x / PatchCanvas::gridX;
        dropY = (mousePos.y - polyAreaOffsetY) / PatchCanvas::gridY;
    }
    else
    {
        section = 0;  // Common
        dropX = mousePos.x / PatchCanvas::gridX;
        dropY = (mousePos.y - separatorY) / PatchCanvas::gridY;
    }

    // Clamp to valid ranges
    dropX = juce::jlimit(0, 39, dropX);
    dropY = juce::jlimit(0, 127, dropY);

    // Trigger callback if set
    if (moduleDropCallback)
        moduleDropCallback(typeId, section, dropX, dropY, moduleName);

    repaint();
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

    // Auto-scroll to where the modules are (use callAfterDelay to ensure viewport is sized)
    if (p != nullptr)
    {
        int separatorY = p->getHeader().separatorPosition * PatchCanvas::gridY;
        if (separatorY == 0)
            separatorY = PatchCanvas::canvasHeight / 2;

        // Find the topmost module across both areas.
        // Use -1 as sentinel so we can detect "no modules found".
        int minY = -1;

        for (auto& m : p->getPolyVoiceArea().getModules())
        {
            int y = PatchCanvas::polyAreaOffsetY + m->getPosition().y * PatchCanvas::gridY;
            minY = (minY < 0) ? y : juce::jmin(minY, y);
        }
        for (auto& m : p->getCommonArea().getModules())
        {
            int y = separatorY + m->getPosition().y * PatchCanvas::gridY;
            minY = (minY < 0) ? y : juce::jmin(minY, y);
        }

        // If no modules found, scroll to top; otherwise scroll just above the topmost module.
        int scrollY = (minY >= 0) ? juce::jmax(0, minY - 20) : 0;

        DBG("Auto-scroll: separatorY=" + juce::String(separatorY)
            + " topModule=" + juce::String(minY)
            + " scrollY=" + juce::String(scrollY));

        viewport.setViewPosition(0, scrollY);
    }
}
