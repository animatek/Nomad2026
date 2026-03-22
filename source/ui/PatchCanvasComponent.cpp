#include "PatchCanvasComponent.h"
#include "QuickAddPopup.h"
#include <cmath>
#include <set>

// --- PatchCanvas (inner scrollable surface) ---

PatchCanvas::PatchCanvas()
{
    setSize(canvasWidth, sectionHeight);
    setWantsKeyboardFocus(true);  // Enable keyboard input for Delete key
}

void PatchCanvas::setSection(int s)
{
    mySection = s;
    setSize(canvasWidth, sectionHeight);
}

PatchCanvas::~PatchCanvas()
{
    // If the popup is still open when we're destroyed, clear its callbacks
    // first so it can't fire onDismiss with a dangling 'this' pointer.
    if (activeQuickAdd != nullptr)
    {
        activeQuickAdd->clearCallbacks();
        delete activeQuickAdd;
        activeQuickAdd = nullptr;
    }
}

void PatchCanvas::setPatch(Patch* p, const ModuleDescriptions* md, const ThemeData* td)
{
    // Reset drag state — raw pointers into the old patch would become stale
    dragState = DragState();

    patch = p;
    moduleDescs = md;
    themeData = td;

    // Apply morph assignments from patch model to individual parameters
    if (p != nullptr)
    {
        for (const auto& ma : p->morphAssignments)
        {
            auto& container = p->getContainer(ma.section);
            auto* mod = container.getModuleByIndex(ma.module);
            if (mod == nullptr) continue;
            auto* param = mod->getParameter(ma.param);
            if (param == nullptr) continue;
            param->setMorphGroup(ma.morph);
            param->setMorphRange(ma.range);
        }
    }

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

Parameter* PatchCanvas::findParameter(Module& m, const juce::String& componentId)
{
    return const_cast<Parameter*>(
        static_cast<const PatchCanvas*>(this)->findParameter(static_cast<const Module&>(m), componentId));
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

    // Each canvas instance is section-specific (mySection 0=common, 1=poly).
    // yOffset is always 0 since each canvas starts from the top.
    if (mySection == 1)
    {
        paintModules(g, patch->getPolyVoiceArea(), 0);
        paintCables(g, patch->getPolyVoiceArea(), 0);
    }
    else
    {
        paintModules(g, patch->getCommonArea(), 0);
        paintCables(g, patch->getCommonArea(), 0);
    }

    // Cable creation preview (rubber-band cable)
    if (showCablePreview && dragState.sourceConnector != nullptr && dragState.module != nullptr)
    {
        auto srcPos = getConnectorPosition(*dragState.module, *dragState.sourceConnector, 0);

        // Color from source connector signal type
        auto* srcDesc = dragState.sourceConnector->getDescriptor();
        juce::Colour cableColor = juce::Colours::white;
        if (srcDesc)
        {
            switch (srcDesc->signalType)
            {
                case SignalType::Audio:   cableColor = juce::Colour(0xffCB4F4F); break;
                case SignalType::Control: cableColor = juce::Colour(0xff5A5FB3); break;
                case SignalType::Logic:   cableColor = juce::Colour(0xffE5DE45); break;
                case SignalType::MasterSlave: cableColor = juce::Colour(0xffA8A8A8); break;
                case SignalType::User1:   cableColor = juce::Colour(0xff9AC899); break;
                case SignalType::User2:   cableColor = juce::Colour(0xffBB00D7); break;
                default: break;
            }
        }

        // Check if cursor is hovering a valid destination connector
        auto hit = findConnectorAt(cablePreviewEnd);
        bool validTarget = hit.connector != nullptr
                        && hit.connector != dragState.sourceConnector
                        && hit.section == dragState.section
                        && srcDesc != nullptr
                        && srcDesc->isOutput != hit.connector->getDescriptor()->isOutput;

        juce::Path path;
        path.startNewSubPath(srcPos.toFloat());
        float endX = static_cast<float>(cablePreviewEnd.x);
        float endY = static_cast<float>(cablePreviewEnd.y);
        float midY = (srcPos.y + cablePreviewEnd.y) * 0.5f;
        float sag = std::abs(float(srcPos.x - cablePreviewEnd.x)) * 0.15f + 15.0f;
        path.cubicTo(static_cast<float>(srcPos.x), midY + sag,
                     endX, midY + sag,
                     endX, endY);

        // Dark outline
        g.setColour(juce::Colour(0xff111111).withAlpha(0.6f));
        g.strokePath(path, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Colored cable
        g.setColour(cableColor.withAlpha(validTarget ? 0.95f : 0.55f));
        g.strokePath(path, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Highlight valid target connector with a bright ring
        if (validTarget && hit.module != nullptr)
        {
            auto tPos = getConnectorPosition(*hit.module, *hit.connector, 0);
            float r = 10.0f;
            g.setColour(cableColor.brighter(0.5f));
            g.drawEllipse(tPos.x - r, tPos.y - r, r * 2, r * 2, 2.0f);
        }
    }

    // Rubber band selection rectangle
    if (showRubberBand)
    {
        auto rb = rubberBandRect.toFloat();
        g.setColour(juce::Colour(0x33ffffff));
        g.fillRect(rb);
        g.setColour(juce::Colour(0xffffdd44).withAlpha(0.8f));
        g.drawRect(rb, 1.5f);
    }

    // Module drop preview (ghost outline)
    if (showModuleDropPreview && moduleDescs != nullptr)
    {
        auto* descriptor = moduleDescs->getModuleByIndex(dropPreviewTypeId);
        if (descriptor != nullptr)
        {
            int x = dropPreviewGridX * gridX;
            int y = dropPreviewGridY * gridY;
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
            paintModuleThemed(g, m, rect, *theme, container);
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

void PatchCanvas::paintModuleThemed(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme, const ModuleContainer& container)
{
    paintModuleBackground(g, m, bounds, theme);
    paintCustomDisplays(g, m, bounds, theme);
    paintLabels(g, bounds, theme);
    paintTextDisplays(g, m, bounds, theme);
    paintSliders(g, m, bounds, theme);
    paintKnobs(g, m, bounds, theme);
    paintButtons(g, m, bounds, theme);
    paintConnectors(g, m, bounds, theme, container);
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

    // Selection highlight
    if (isSelected(&m))
    {
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.fillRoundedRectangle(bounds.toFloat(), 3.0f);
        g.setColour(juce::Colour(0xffffdd44).withAlpha(0.9f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 3.0f, 2.0f);
    }
}

bool PatchCanvas::hasHiddenCable(const Connector& conn, const ModuleContainer& container) const
{
    if (patch == nullptr) return false;
    const auto& hdr = patch->getHeader();

    for (auto& connection : container.getConnections())
    {
        if (connection.output == &conn || connection.input == &conn)
        {
            if (connection.output == nullptr || connection.output->getDescriptor() == nullptr)
                continue;

            bool visible = true;
            switch (connection.output->getDescriptor()->signalType)
            {
                case SignalType::Audio:       visible = hdr.cableVisRed;    break;
                case SignalType::Control:     visible = hdr.cableVisBlue;   break;
                case SignalType::Logic:       visible = hdr.cableVisYellow; break;
                case SignalType::MasterSlave: visible = hdr.cableVisGray;   break;
                case SignalType::User1:       visible = hdr.cableVisGreen;  break;
                case SignalType::User2:       visible = hdr.cableVisPurple; break;
                case SignalType::None:        visible = hdr.cableVisWhite;  break;
            }

            if (!visible) return true;
        }
    }
    return false;
}

void PatchCanvas::paintConnectors(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme, const ModuleContainer& container)
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

        // Find the actual connector object and check if output
        bool isOutput = false;
        const Connector* actualConnector = nullptr;
        for (auto& conn : m.getConnectors())
        {
            if (conn.getDescriptor() && conn.getDescriptor()->componentId == tc.componentId)
            {
                isOutput = conn.getDescriptor()->isOutput;
                actualConnector = &conn;
                break;
            }
        }

        // Check if this connector has a hidden (filtered) cable — show "capped" visual
        bool capped = (actualConnector != nullptr) && hasHiddenCable(*actualConnector, container);

        const float innerRatio = 0.38f;
        const float innerSz = sz * innerRatio;
        const float innerOffset = (sz - innerSz) * 0.5f;
        const juce::Colour darkHole = juce::Colour(0xff111111);
        const juce::Colour outline  = juce::Colour(0xff222222);

        if (isOutput)
        {
            // Output: filled circle
            g.setColour(connColour);
            g.fillEllipse(cx, cy, sz, sz);

            g.setColour(outline);
            g.drawEllipse(cx, cy, sz, sz, 1.0f);

            if (capped)
            {
                // Capped: filled center (no hole) with a subtle cross/cap indicator
                g.setColour(connColour.darker(0.4f));
                g.fillEllipse(cx + innerOffset, cy + innerOffset, innerSz, innerSz);
            }
            else
            {
                // Normal: dark inner circle (plug hole)
                g.setColour(darkHole);
                g.fillEllipse(cx + innerOffset, cy + innerOffset, innerSz, innerSz);
            }
        }
        else
        {
            // Input: rounded rectangle
            const float cornerRadius = sz * 0.25f;
            g.setColour(connColour);
            g.fillRoundedRectangle(cx, cy, sz, sz, cornerRadius);

            g.setColour(outline);
            g.drawRoundedRectangle(cx, cy, sz, sz, cornerRadius, 1.0f);

            if (capped)
            {
                // Capped: filled center (no socket hole)
                const float sqSz = innerSz * 1.1f;
                const float sqOff = (sz - sqSz) * 0.5f;
                g.setColour(connColour.darker(0.4f));
                g.fillRoundedRectangle(cx + sqOff, cy + sqOff, sqSz, sqSz, cornerRadius * 0.4f);
            }
            else
            {
                // Normal: dark inner square (socket hole)
                const float sqSz = innerSz * 1.1f;
                const float sqOff = (sz - sqSz) * 0.5f;
                g.setColour(darkHole);
                g.fillRoundedRectangle(cx + sqOff, cy + sqOff, sqSz, sqSz, cornerRadius * 0.4f);
            }
        }
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

        static const juce::Colour morphColors[4] = {
            juce::Colour(0xffCB4F4F),  // Group 1 — red
            juce::Colour(0xff9AC899),  // Group 2 — green
            juce::Colour(0xff5A5FB3),  // Group 3 — blue
            juce::Colour(0xffE5DE45),  // Group 4 — yellow
        };

        auto* param = findParameter(m, tk.componentId);
        int morphGroup = (param != nullptr) ? param->getMorphGroup() : -1;
        bool hasMorph = (morphGroup >= 0 && morphGroup < 4);
        juce::Colour baseColor = hasMorph ? morphColors[morphGroup] : juce::Colour(0xff989898);

        // Compute knob geometry first — needed for both wedge and grip
        float normalized = 0.5f;
        if (param != nullptr)
        {
            auto* pd = param->getDescriptor();
            int paramRange = pd->maxValue - pd->minValue;
            if (paramRange > 0)
                normalized = static_cast<float>(param->getValue() - pd->minValue) / static_cast<float>(paramRange);
        }

        // Knob center and radii
        float centerX = cx + sz * 0.5f;
        float centerY = cy + sz * 0.5f;
        float radius  = sz * 0.5f;

        // Knob angle: 270° range from 7 o'clock (-135°) to 5 o'clock (+135°)
        // Convention: 0° = 12 o'clock, positive = clockwise (same as JUCE addPieSegment)
        const float kRangedeg = 270.0f;
        const float kStartDeg = -135.0f;
        float knobAngleDeg = kStartDeg + normalized * kRangedeg;
        float knobAngle    = knobAngleDeg * juce::MathConstants<float>::pi / 180.0f;

        // Background circle — morph group color if assigned, grey otherwise
        g.setColour(baseColor);
        g.fillEllipse(cx, cy, sz, sz);

        // Morph wedge: starts at the knob's current position, sweeps by morphRange
        if (hasMorph && param != nullptr)
        {
            int morphRangeVal = param->getMorphRange();  // -127..127
            float sweepRad = (static_cast<float>(morphRangeVal) / 127.0f)
                             * kRangedeg * juce::MathConstants<float>::pi / 180.0f;

            if (std::abs(sweepRad) > 0.005f)
            {
                float fromAngle = (sweepRad >= 0.0f) ? knobAngle : knobAngle + sweepRad;
                float toAngle   = (sweepRad >= 0.0f) ? knobAngle + sweepRad : knobAngle;

                float r = radius * 0.82f;
                juce::Path wedge;
                wedge.addPieSegment(centerX - r, centerY - r, r * 2.0f, r * 2.0f,
                                    fromAngle, toAngle, 0.0f);
                g.setColour(baseColor.darker(0.55f));
                g.fillPath(wedge);
            }
        }

        // Outline
        g.setColour(hasMorph ? baseColor.darker(0.4f) : juce::Colour(0xff666666));
        g.drawEllipse(cx, cy, sz, sz, 1.0f);

        // Grip line — drawn on top of wedge so it's always visible
        float innerR = radius * 0.3f;
        float outerR = radius * 0.85f;
        float sinA   = std::sin(knobAngle);
        float cosA   = std::cos(knobAngle);

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

void PatchCanvas::shakeCables()
{
    cableSagOffsets.clear();
    std::mt19937 rng(static_cast<unsigned>(juce::Time::currentTimeMillis()));
    std::uniform_real_distribution<float> dist(-0.6f, 0.6f);

    auto addOffsets = [&](const ModuleContainer& container)
    {
        for (auto& conn : container.getConnections())
        {
            if (conn.output && conn.input)
            {
                auto key = std::make_pair(conn.output, conn.input);
                cableSagOffsets[key] = dist(rng);
            }
        }
    };

    if (patch != nullptr)
    {
        addOffsets(patch->getPolyVoiceArea());
        addOffsets(patch->getCommonArea());
    }
    repaint();
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

        // Draw a curved cable with optional shake offset
        juce::Path path;
        path.startNewSubPath(srcPos.toFloat());

        float midY = (srcPos.y + dstPos.y) * 0.5f;
        float baseSag = std::abs(static_cast<float>(srcPos.x - dstPos.x)) * 0.15f + 15.0f;

        // Apply shake offset if present
        float sagMultiplier = 1.0f;
        auto key = std::make_pair(conn.output, conn.input);
        auto it = cableSagOffsets.find(key);
        if (it != cableSagOffsets.end())
            sagMultiplier += it->second;

        float sag = baseSag * sagMultiplier;

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

    // Each canvas handles exactly one section; yOffset is always 0.
    ModuleContainer& activeContainer = (mySection == 1)
        ? patch->getPolyVoiceArea()
        : patch->getCommonArea();
    struct { ModuleContainer* container; int section; int yOffset; } areas[] = {
        { &activeContainer, mySection, 0 }
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
                        if (e.mods.isRightButtonDown())
                        {
                            showParameterContextMenu(m, area.section, *param);
                            return;
                        }
                        if (e.mods.isCtrlDown())
                        {
                            // Ctrl+drag: adjust morph range
                            // Auto-assign to group 0 if not yet assigned
                            if (param->getMorphGroup() < 0)
                            {
                                param->setMorphGroup(0);
                                param->setMorphRange(0);
                                if (morphAssignCallback)
                                    morphAssignCallback(area.section, m.getContainerIndex(),
                                                        param->getDescriptor()->index, 0);
                            }
                            dragState.type = DragState::MorphRange;
                            dragState.module = &m;
                            dragState.parameter = param;
                            dragState.section = area.section;
                            dragState.startPos = pos;
                            dragState.startValue = param->getMorphRange();
                            return;
                        }
                        dragState.type = DragState::Knob;
                        dragState.module = &m;
                        dragState.parameter = param;
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
                        if (e.mods.isRightButtonDown())
                        {
                            showParameterContextMenu(m, area.section, *param);
                            return;
                        }
                        if (e.mods.isCtrlDown())
                        {
                            // Ctrl+drag: adjust morph range
                            if (param->getMorphGroup() < 0)
                            {
                                param->setMorphGroup(0);
                                param->setMorphRange(0);
                                if (morphAssignCallback)
                                    morphAssignCallback(area.section, m.getContainerIndex(),
                                                        param->getDescriptor()->index, 0);
                            }
                            dragState.type = DragState::MorphRange;
                            dragState.module = &m;
                            dragState.parameter = param;
                            dragState.section = area.section;
                            dragState.startPos = pos;
                            dragState.startValue = param->getMorphRange();
                            return;
                        }
                        dragState.type = DragState::Slider;
                        dragState.module = &m;
                        dragState.parameter = param;
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
                        if (e.mods.isRightButtonDown())
                        {
                            showParameterContextMenu(m, area.section, *param);
                            return;
                        }
                        dragState.type = DragState::Button;
                        dragState.module = &m;
                        dragState.parameter = param;
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

            // Module body fallback
            if (e.mods.isRightButtonDown())
            {
                // If right-clicking on a selected module and there are multiple selected → selection menu
                if (isSelected(&m) && selection.size() > 1)
                {
                    showSelectionContextMenu();
                    return;
                }

                // Single-module context menu
                Module* modPtr = &m;
                int sec = area.section;

                juce::PopupMenu menu;
                menu.addItem(1, "Rename Module...");
                menu.addSeparator();
                menu.addItem(2, "Duplicate");
                menu.addItem(3, "Duplicate with Cables");
                menu.addItem(4, "Copy");
                menu.addSeparator();
                menu.addItem(5, "Delete Module");

                menu.showMenuAsync(juce::PopupMenu::Options{},
                    [this, modPtr, sec](int result)
                    {
                        if (result == 1)
                        {
                            // Rename: show text input dialog
                            auto* dialog = new juce::AlertWindow(
                                "Rename Module",
                                "Enter new name for \"" + modPtr->getTitle() + "\":",
                                juce::MessageBoxIconType::NoIcon);
                            dialog->addTextEditor("name", modPtr->getTitle(), "Module name:");
                            dialog->getTextEditor("name")->setInputRestrictions(16);
                            dialog->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
                            dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

                            dialog->enterModalState(true, juce::ModalCallbackFunction::create(
                                [this, dialog, modPtr, sec](int r) {
                                    if (r == 1)
                                    {
                                        juce::String newName = dialog->getTextEditorContents("name").trim();
                                        if (newName.isNotEmpty())
                                        {
                                            modPtr->setTitle(newName);
                                            if (renameModuleCallback)
                                                renameModuleCallback(sec, modPtr, newName);
                                            repaint();
                                        }
                                    }
                                    delete dialog;
                                }), true);
                        }
                        else if (result == 2)
                        {
                            // Duplicate single module (no cables)
                            selectModule(modPtr, sec);
                            duplicateSelection(false);
                        }
                        else if (result == 3)
                        {
                            // Duplicate with cables
                            selectModule(modPtr, sec);
                            duplicateSelection(true);
                        }
                        else if (result == 4)
                        {
                            // Copy
                            selectModule(modPtr, sec);
                            copySelectionToClipboard();
                        }
                        else if (result == 5)
                        {
                            if (deleteModuleCallback)
                                deleteModuleCallback(sec, modPtr);
                            if (selectedModule == modPtr)
                                clearSelection();
                            repaint();
                        }
                    });
                return;
            }

            // Left click → select module
            bool alreadySelected = isSelected(&m);
            bool addToSel = e.mods.isShiftDown();

            if (!alreadySelected || addToSel)
                selectModule(&m, area.section, addToSel);
            // If already selected and no shift → keep selection, just start move

            // Multi-move if more than one module selected
            if (selection.size() > 1)
            {
                dragState.type = DragState::MultiModuleMove;
                dragState.startPos = pos;
                multiMoveState.clear();
                for (auto& sel : selection)
                    multiMoveState.push_back({ sel.module, sel.section, sel.module->getPosition() });
            }
            else
            {
                dragState.type = DragState::ModuleMove;
                dragState.module = &m;
                dragState.section = area.section;
                dragState.startPos = pos;
                dragState.dragOffsetX = pos.x - rect.getX();
                dragState.dragOffsetY = pos.y - rect.getY();
            }
            repaint();
            return;
        }
    }

    // Clicked on empty area → start rubber band, clear selection
    if (!e.mods.isRightButtonDown())
    {
        clearSelection();
        dragState.type = DragState::RubberBand;
        dragState.startPos = pos;
        rubberBandRect = juce::Rectangle<int>(pos, pos);
        showRubberBand = true;
        repaint();
    }
    else
    {
        // Right-click on empty canvas → Add Module menu (by category) + Paste
        if (patch == nullptr || moduleDescs == nullptr) return;

        // Determine section + grid position for the new module
        // Each canvas is section-specific; yOffset is always 0.
        int clickSection = mySection;
        int clickGX = juce::jlimit(0, 39, pos.x / gridX);
        int clickGY = juce::jlimit(0, 127, pos.y / gridY);

        juce::PopupMenu menu;

        // "Add Module" submenu organised by category
        // IDs: 1000 + moduleIndex  (leaves plenty of room for other items)
        juce::PopupMenu addMenu;
        auto categories = moduleDescs->getCategories();
        for (auto& cat : categories)
        {
            juce::PopupMenu catMenu;
            for (auto* desc : moduleDescs->getModulesInCategory(cat))
                catMenu.addItem(1000 + desc->index, desc->fullname);
            addMenu.addSubMenu(cat, catMenu);
        }
        menu.addSubMenu("Add Module", addMenu);

        if (!clipboard.empty())
        {
            menu.addSeparator();
            menu.addItem(1, "Paste");
        }

        menu.addSeparator();
        menu.addItem(2, "Shake Cables");
        menu.addItem(3, "Reset Cables");

        menu.showMenuAsync(juce::PopupMenu::Options{},
            [this, clickSection, clickGX, clickGY, pos](int result)
            {
                if (result == 1)
                {
                    pasteFromClipboard(pos);
                }
                else if (result == 2)
                {
                    shakeCables();
                }
                else if (result == 3)
                {
                    cableSagOffsets.clear();
                    repaint();
                }
                else if (result >= 1000)
                {
                    int typeIndex = result - 1000;
                    auto* desc = moduleDescs->getModuleByIndex(typeIndex);
                    if (desc && moduleDropCallback)
                        moduleDropCallback(typeIndex, clickSection, clickGX, clickGY, desc->name);
                }
            });
    }
}

void PatchCanvas::mouseDrag(const juce::MouseEvent& e)
{
    if (dragState.type == DragState::None)
        return;

    auto currentPos = e.getPosition();

    if (dragState.type == DragState::RubberBand)
    {
        rubberBandRect = juce::Rectangle<int>(dragState.startPos, currentPos);
        updateRubberBandSelection(rubberBandRect.toNearestInt());
        repaint();
        return;
    }

    if (dragState.type == DragState::MorphRange)
    {
        if (dragState.parameter == nullptr || dragState.module == nullptr) return;
        // Dragging up increases range, down decreases (same sensitivity as knob)
        int dy = dragState.startPos.y - currentPos.y;  // up = positive
        int newRange = juce::jlimit(-127, 127, dragState.startValue + dy);
        dragState.parameter->setMorphRange(newRange);

        // Rate-limited send to synth
        auto now = juce::Time::currentTimeMillis();
        if (now - dragState.lastSendTime >= paramSendIntervalMs)
        {
            dragState.lastSendTime = now;
            if (morphRangeChangeCallback)
            {
                int span = std::abs(newRange);
                int direction = (newRange >= 0) ? 0 : 1;
                morphRangeChangeCallback(dragState.section,
                                         dragState.module->getContainerIndex(),
                                         dragState.parameter->getDescriptor()->index,
                                         span, direction);
            }
        }
        repaint();
        return;
    }

    if (dragState.type == DragState::MultiModuleMove)
    {
        int dx = (currentPos.x - dragState.startPos.x + gridX / 2) / gridX;
        int dy = (currentPos.y - dragState.startPos.y + gridY / 2) / gridY;

        for (auto& ms : multiMoveState)
        {
            int newX = juce::jmax(0, ms.startGridPos.x + dx);
            auto& container = patch->getContainer(ms.section);
            int rawY = juce::jmax(0, ms.startGridPos.y + dy);
            int newY = findNearestFreeY(container, ms.module, newX, rawY, ms.module->getDescriptor()->height);
            ms.module->setPosition({ newX, newY });
        }
        repaint();
        return;
    }

    if (dragState.type == DragState::ModuleMove)
    {
        int newGridX = juce::jmax(0, (currentPos.x - dragState.dragOffsetX + gridX / 2) / gridX);
        int rawGridY = juce::jmax(0, (currentPos.y - dragState.dragOffsetY + gridY / 2) / gridY);

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

    if (dragState.type == DragState::RubberBand)
    {
        showRubberBand = false;
        dragState = DragState();
        repaint();
        return;
    }

    if (dragState.type == DragState::MultiModuleMove)
    {
        multiMoveState.clear();
        dragState = DragState();
        return;
    }

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

    // MorphRange: send final morph range on release
    if (dragState.type == DragState::MorphRange && dragState.module != nullptr && morphRangeChangeCallback)
    {
        int finalRange = dragState.parameter->getMorphRange();
        int span = std::abs(finalRange);
        int direction = (finalRange >= 0) ? 0 : 1;
        morphRangeChangeCallback(dragState.section, dragState.module->getContainerIndex(),
                                 dragState.parameter->getDescriptor()->index,
                                 span, direction);
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
    // Delete / Backspace → delete selection
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (!selection.empty())
        {
            deleteSelection();
            return true;
        }
    }

    // Ctrl+C → copy
    if (key == juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0))
    {
        if (!selection.empty()) { copySelectionToClipboard(); return true; }
    }

    // Ctrl+V → paste at centre of viewport
    if (key == juce::KeyPress('v', juce::ModifierKeys::commandModifier, 0))
    {
        if (!clipboard.empty())
        {
            pasteFromClipboard({ getWidth() / 2, getHeight() / 4 });
            return true;
        }
    }

    // Ctrl+D → duplicate with cables
    if (key == juce::KeyPress('d', juce::ModifierKeys::commandModifier, 0))
    {
        if (!selection.empty()) { duplicateSelection(true); return true; }
    }

    // Enter → Quick Add popup at mouse position (only one at a time)
    if (key == juce::KeyPress::returnKey && patch != nullptr && moduleDescs != nullptr)
    {
        if (activeQuickAdd != nullptr)
            return true;  // already open

        auto mousePos = getMouseXYRelative();

        int section = mySection;
        int gx = juce::jlimit(0, 39, mousePos.x / gridX);
        int gy = juce::jlimit(0, 127, mousePos.y / gridY);

        auto screenPos = localPointToGlobal(mousePos);

        activeQuickAdd = new QuickAddPopup(
            *moduleDescs, screenPos, gx, gy,
            [this, section](const ModuleDescriptor* desc, int pgx, int pgy)
            {
                if (moduleDropCallback && desc)
                    moduleDropCallback(desc->index, section, pgx, pgy, desc->name);
            },
            [this]() { activeQuickAdd = nullptr; }
        );

        activeQuickAdd->grabFocusNow();
        return true;
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

    ModuleContainer& activeContainer = (mySection == 1)
        ? patch->getPolyVoiceArea()
        : patch->getCommonArea();
    struct { ModuleContainer* container; int section; int yOffset; } areas[] = {
        { &activeContainer, mySection, 0 }
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

    auto mousePos = dragSourceDetails.localPosition;
    dropPreviewSection = mySection;
    dropPreviewGridX = juce::jlimit(0, 39, mousePos.x / gridX);
    dropPreviewGridY = juce::jlimit(0, 127, mousePos.y / gridY);

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

    auto mousePos = dragSourceDetails.localPosition;
    int section = mySection;
    int dropX = juce::jlimit(0, 39, mousePos.x / PatchCanvas::gridX);
    int dropY = juce::jlimit(0, 127, mousePos.y / PatchCanvas::gridY);

    // Trigger callback if set
    if (moduleDropCallback)
        moduleDropCallback(typeId, section, dropX, dropY, moduleName);

    repaint();
}

// --- Parameter context menu ---

void PatchCanvas::showParameterContextMenu(Module& m, int section, Parameter& param)
{
    auto* pd = param.getDescriptor();
    if (pd == nullptr) return;

    bool atDefault = (param.getValue() == pd->defaultValue);
    int currentMorphGroup = param.getMorphGroup();  // -1=none, 0-3=assigned

    juce::PopupMenu menu;
    menu.addSectionHeader(pd->name);

    // 1. Default Value
    menu.addItem(1, "Default Value", !atDefault);

    // 2. Zero Morph — sends MorphRangeChange with span=0 to synth
    bool hasMorphAssigned = (currentMorphGroup >= 0);
    menu.addItem(2, "Zero Morph", hasMorphAssigned);

    // 3. Knob assignment submenu
    // Find current knob assignment for this param
    int currentKnob = -1;
    if (patch != nullptr)
    {
        for (int k = 0; k < 23; ++k)
        {
            const auto& ka = patch->knobAssignments[static_cast<size_t>(k)];
            if (ka.assigned && ka.section == section
                && ka.module == m.getContainerIndex() && ka.param == pd->index)
            { currentKnob = k; break; }
        }
    }
    {
        juce::PopupMenu knobSubMenu;
        // Knob 1-6
        for (int k = 0; k < 6; ++k)
        {
            juce::String label = "Knob " + juce::String(k + 1);
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            knobSubMenu.addItem(100 + k, label, true, k == currentKnob);
        }
        knobSubMenu.addSeparator();
        // Knob 7-12
        for (int k = 6; k < 12; ++k)
        {
            juce::String label = "Knob " + juce::String(k + 1);
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            knobSubMenu.addItem(100 + k, label, true, k == currentKnob);
        }
        knobSubMenu.addSeparator();
        // Knob 13-15
        for (int k = 12; k < 15; ++k)
        {
            juce::String label = "Knob " + juce::String(k + 1);
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            knobSubMenu.addItem(100 + k, label, true, k == currentKnob);
        }
        knobSubMenu.addSeparator();
        // Knob 16-18
        for (int k = 15; k < 18; ++k)
        {
            juce::String label = "Knob " + juce::String(k + 1);
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            knobSubMenu.addItem(100 + k, label, true, k == currentKnob);
        }
        knobSubMenu.addSeparator();
        // Pedal, After touch, On/Off switch
        const char* specialNames[] = { "Pedal", "After touch", "On/Off switch" };
        for (int k = 18; k < 21; ++k)
        {
            juce::String label = specialNames[k - 18];
            if (patch != nullptr && patch->knobAssignments[static_cast<size_t>(k)].assigned && k != currentKnob)
                label += " (used)";
            knobSubMenu.addItem(100 + k, label, true, k == currentKnob);
        }
        knobSubMenu.addSeparator();
        knobSubMenu.addItem(99, "Disable", currentKnob >= 0);
        menu.addSubMenu("Knob", knobSubMenu);
    }

    // 4. Morph Group submenu
    juce::PopupMenu morphSubMenu;
    for (int g = 1; g <= 4; ++g)
    {
        bool isCurrent = (currentMorphGroup == g - 1);
        morphSubMenu.addItem(10 + g, "Group " + juce::String(g),
                             true, isCurrent);
    }
    morphSubMenu.addSeparator();
    morphSubMenu.addItem(10, "Disable", currentMorphGroup >= 0);
    menu.addSubMenu("Morph", morphSubMenu);

    // 5. MIDI Controller submenu
    int currentMidiCtrl = -1;
    if (patch != nullptr)
    {
        for (const auto& ca : patch->ctrlAssignments)
        {
            if (ca.section == section && ca.module == m.getContainerIndex() && ca.param == pd->index)
            { currentMidiCtrl = ca.control; break; }
        }
    }
    {
        juce::PopupMenu midiSubMenu;
        for (int cc = 0; cc < 120; ++cc)
        {
            bool isCurrent = (cc == currentMidiCtrl);
            midiSubMenu.addItem(200 + cc, "CC " + juce::String(cc), true, isCurrent);
        }
        midiSubMenu.addSeparator();
        midiSubMenu.addItem(199, "Disable", currentMidiCtrl >= 0);
        menu.addSubMenu("MIDI Controller", midiSubMenu);
    }

    menu.showMenuAsync(juce::PopupMenu::Options{},
        [this, &m, section, &param, currentKnob, currentMidiCtrl](int result)
        {
            auto* pd2 = param.getDescriptor();
            if (pd2 == nullptr) return;

            if (result == 1)
            {
                // Set to default value
                param.setValue(pd2->defaultValue);
                if (parameterChangeCallback)
                    parameterChangeCallback(section, m.getContainerIndex(),
                                           pd2->index, pd2->defaultValue);
                repaint();
            }
            else if (result == 2)
            {
                // Zero Morph: remove morph assignment entirely (group + range)
                param.setMorphGroup(-1);
                param.setMorphRange(0);
                if (morphAssignCallback)
                    morphAssignCallback(section, m.getContainerIndex(),
                                       pd2->index, -1);
                repaint();
            }
            else if (result == 10)
            {
                // Disable morph assignment
                param.setMorphGroup(-1);
                param.setMorphRange(0);
                if (morphAssignCallback)
                    morphAssignCallback(section, m.getContainerIndex(),
                                       pd2->index, -1);
                repaint();
            }
            else if (result >= 11 && result <= 14)
            {
                // Assign to morph group 0-3, start at range=0
                int group = result - 11;
                param.setMorphGroup(group);
                param.setMorphRange(0);
                if (morphAssignCallback)
                    morphAssignCallback(section, m.getContainerIndex(),
                                       pd2->index, group);
                // Explicitly tell the synth range=0 so it matches our model
                if (morphRangeChangeCallback)
                    morphRangeChangeCallback(section, m.getContainerIndex(),
                                            pd2->index, 0, 0);
                repaint();
            }
            // Knob assignment (99=disable, 100-122=assign knob 0-22)
            else if (result == 99)
            {
                if (knobAssignCallback && currentKnob >= 0)
                    knobAssignCallback(section, m.getContainerIndex(), pd2->index, -1);
            }
            else if (result >= 100 && result < 123)
            {
                int knob = result - 100;
                if (knobAssignCallback)
                    knobAssignCallback(section, m.getContainerIndex(), pd2->index, knob);
            }
            // MIDI Controller assignment (199=disable, 200-319=assign CC 0-119)
            else if (result == 199)
            {
                if (midiCtrlAssignCallback && currentMidiCtrl >= 0)
                    midiCtrlAssignCallback(section, m.getContainerIndex(), pd2->index, -1);
            }
            else if (result >= 200 && result < 320)
            {
                int cc = result - 200;
                if (midiCtrlAssignCallback)
                    midiCtrlAssignCallback(section, m.getContainerIndex(), pd2->index, cc);
            }
        });
}

// --- Selection helpers ---

bool PatchCanvas::isSelected(const Module* m) const
{
    for (auto& s : selection)
        if (s.module == m) return true;
    return false;
}

void PatchCanvas::clearSelection()
{
    selection.clear();
    selectedModule = nullptr;
    selectedSection = -1;
    if (moduleSelectedCallback) moduleSelectedCallback(nullptr, -1);
}

void PatchCanvas::selectModule(Module* m, int section, bool addToSelection)
{
    if (!addToSelection) clearSelection();
    if (!isSelected(m))
        selection.push_back({ m, section });
    selectedModule = m;
    selectedSection = section;
    // Notify inspector — report the most recently selected module
    if (moduleSelectedCallback) moduleSelectedCallback(m, section);
}

void PatchCanvas::updateRubberBandSelection(juce::Rectangle<int> rect)
{
    selection.clear();
    if (patch == nullptr) return;

    ModuleContainer& container = (mySection == 1)
        ? patch->getPolyVoiceArea()
        : patch->getCommonArea();

    for (auto& modulePtr : container.getModules())
    {
        auto bounds = getModuleBounds(*modulePtr, 0);
        if (rect.intersects(bounds))
            selection.push_back({ modulePtr.get(), mySection });
    }
}

void PatchCanvas::deleteSelection()
{
    if (selection.empty() || patch == nullptr) return;

    for (auto& sel : selection)
    {
        auto& container = patch->getContainer(sel.section);
        container.removeModule(sel.module);
    }
    clearSelection();
    repaint();
}

void PatchCanvas::duplicateSelection(bool withCables)
{
    if (selection.empty() || patch == nullptr || moduleDescs == nullptr) return;

    // Offset for duplicates: 1 column to the right, 0 rows down
    const int offsetX = 1, offsetY = 2;

    // Map old Module* → {new Module*, section} — section stored at creation to
    // avoid a redundant O(N) search when building the new selection below.
    std::map<Module*, std::pair<Module*, int>> oldToNew;

    for (auto& sel : selection)
    {
        auto* desc = sel.module->getDescriptor();
        if (!desc) continue;

        auto& container = patch->getContainer(sel.section);
        auto pos = sel.module->getPosition();
        int newX = pos.x + offsetX;
        int newY = findNearestFreeY(container, nullptr, newX, pos.y + offsetY, desc->height);

        auto* newMod = patch->createModule(sel.section, desc->index, newX, newY,
                                           sel.module->getTitle(), *moduleDescs);
        if (!newMod) continue;

        // Copy parameter values
        auto& srcParams = sel.module->getParameters();
        auto& dstParams = newMod->getParameters();
        for (size_t i = 0; i < srcParams.size() && i < dstParams.size(); ++i)
            dstParams[i].setValue(srcParams[i].getValue());

        oldToNew[sel.module] = { newMod, sel.section };
    }

    // Recreate internal cables if requested
    if (withCables)
    {
        // Gather all selected module pointers
        std::set<Module*> selSet;
        for (auto& s : selection) selSet.insert(s.module);

        for (auto& sel : selection)
        {
            auto& container = patch->getContainer(sel.section);
            for (auto& cable : container.getConnections())
            {
                // Find which modules own output and input
                Module* srcMod = nullptr;
                Module* dstMod = nullptr;
                Connector* srcConn = cable.output;
                Connector* dstConn = cable.input;

                for (auto& m : container.getModules())
                {
                    for (auto& c : m->getConnectors())
                    {
                        if (&c == srcConn) srcMod = m.get();
                        if (&c == dstConn) dstMod = m.get();
                    }
                }

                // Only duplicate cable if BOTH endpoints are in the selection
                if (srcMod && dstMod && selSet.count(srcMod) && selSet.count(dstMod))
                {
                    auto* newSrcMod = oldToNew[srcMod].first;
                    auto* newDstMod = oldToNew[dstMod].first;
                    if (!newSrcMod || !newDstMod) continue;

                    // Find matching connectors by descriptor index
                    auto* srcDesc = srcConn->getDescriptor();
                    auto* dstDesc = dstConn->getDescriptor();
                    if (!srcDesc || !dstDesc) continue;

                    Connector* newSrc = newSrcMod->getConnector(srcDesc->index);
                    Connector* newDst = newDstMod->getConnector(dstDesc->index);
                    if (newSrc && newDst)
                        container.addConnection(newSrc, newDst);
                }
            }
        }
    }

    // Select the new modules — section is already known from creation time
    clearSelection();
    for (auto& [old, newModAndSection] : oldToNew)
    {
        auto* newMod = newModAndSection.first;
        int section  = newModAndSection.second;
        selection.push_back({ newMod, section });
    }

    repaint();
}

void PatchCanvas::copySelectionToClipboard()
{
    if (selection.empty() || patch == nullptr) return;

    clipboard.clear();
    clipboardCables.clear();

    std::map<Module*, int> modToClipIdx;

    for (int i = 0; i < (int)selection.size(); ++i)
    {
        auto& sel = selection[i];
        ClipboardEntry entry;
        entry.typeIndex = sel.module->getDescriptor() ? sel.module->getDescriptor()->index : 0;
        entry.name = sel.module->getTitle();
        entry.section = sel.section;
        entry.gridPos = sel.module->getPosition();
        for (auto& p : sel.module->getParameters())
            entry.paramValues.push_back(p.getValue());
        clipboard.push_back(entry);
        modToClipIdx[sel.module] = i;
    }

    // Store internal cables
    std::set<Module*> selSet;
    for (auto& s : selection) selSet.insert(s.module);

    for (auto& sel : selection)
    {
        auto& container = patch->getContainer(sel.section);
        for (auto& cable : container.getConnections())
        {
            Module* srcMod = nullptr; Module* dstMod = nullptr;
            for (auto& m : container.getModules())
            {
                for (auto& c : m->getConnectors())
                {
                    if (&c == cable.output) srcMod = m.get();
                    if (&c == cable.input)  dstMod = m.get();
                }
            }
            if (srcMod && dstMod && selSet.count(srcMod) && selSet.count(dstMod))
            {
                auto* srcDesc = cable.output->getDescriptor();
                auto* dstDesc = cable.input->getDescriptor();
                if (srcDesc && dstDesc)
                    clipboardCables.push_back({ modToClipIdx[srcMod], srcDesc->index,
                                                modToClipIdx[dstMod], dstDesc->index });
            }
        }
    }
}

void PatchCanvas::pasteFromClipboard(juce::Point<int> mousePos)
{
    if (clipboard.empty() || patch == nullptr || moduleDescs == nullptr) return;

    // Find bounding box of clipboard to offset paste position
    int minX = clipboard[0].gridPos.x, minY = clipboard[0].gridPos.y;
    for (auto& e : clipboard)
    {
        minX = std::min(minX, e.gridPos.x);
        minY = std::min(minY, e.gridPos.y);
    }

    // Determine target section and position from mouse
    int separatorY = patch->getHeader().separatorPosition * gridY;
    if (separatorY == 0) separatorY = canvasHeight / 2;
    int pasteSection = (mousePos.y < separatorY) ? 1 : 0;
    int pasteGridX = mousePos.x / gridX;
    int pasteGridY = (pasteSection == 1) ? mousePos.y / gridY
                                         : (mousePos.y - separatorY) / gridY;

    std::vector<Module*> pasted;
    for (auto& entry : clipboard)
    {
        int dx = entry.gridPos.x - minX;
        int dy = entry.gridPos.y - minY;
        auto& container = patch->getContainer(pasteSection);
        int newY = findNearestFreeY(container, nullptr, pasteGridX + dx,
                                    pasteGridY + dy,
                                    moduleDescs->getModuleByIndex(entry.typeIndex)
                                        ? moduleDescs->getModuleByIndex(entry.typeIndex)->height : 1);
        auto* newMod = patch->createModule(pasteSection, entry.typeIndex,
                                           pasteGridX + dx, newY,
                                           entry.name, *moduleDescs);
        if (!newMod) { pasted.push_back(nullptr); continue; }

        auto& params = newMod->getParameters();
        for (size_t i = 0; i < entry.paramValues.size() && i < params.size(); ++i)
            params[i].setValue(entry.paramValues[i]);
        pasted.push_back(newMod);
    }

    // Recreate cables
    auto& container = patch->getContainer(pasteSection);
    for (auto& cb : clipboardCables)
    {
        if (cb.srcModuleClipIdx >= (int)pasted.size()) continue;
        if (cb.dstModuleClipIdx >= (int)pasted.size()) continue;
        auto* s = pasted[cb.srcModuleClipIdx];
        auto* d = pasted[cb.dstModuleClipIdx];
        if (!s || !d) continue;
        auto* sc = s->getConnector(cb.srcConnectorIdx);
        auto* dc = d->getConnector(cb.dstConnectorIdx);
        if (sc && dc) container.addConnection(sc, dc);
    }

    // Select pasted modules
    clearSelection();
    for (auto* m : pasted)
        if (m) selection.push_back({ m, pasteSection });

    repaint();
}

void PatchCanvas::showSelectionContextMenu()
{
    juce::PopupMenu menu;
    juce::String label = selection.size() == 1
        ? "1 module selected"
        : juce::String((int)selection.size()) + " modules selected";
    menu.addSectionHeader(label);
    menu.addItem(1, "Duplicate");
    menu.addItem(2, "Duplicate with Cables");
    menu.addItem(3, "Copy");
    menu.addSeparator();
    menu.addItem(4, "Delete");

    menu.showMenuAsync(juce::PopupMenu::Options{}, [this](int result) {
        if (result == 1) duplicateSelection(false);
        else if (result == 2) duplicateSelection(true);
        else if (result == 3) copySelectionToClipboard();
        else if (result == 4) deleteSelection();
    });
}

// --- PatchCanvasComponent (two-panel split viewport) ---

PatchCanvasComponent::PatchCanvasComponent()
{
    // Set sections before anything else
    polyCanvas.setSection(1);    // Poly (top)
    commonCanvas.setSection(0);  // Common (bottom)

    // Setup viewports
    polyViewport.setViewedComponent(&polyCanvas, false);
    polyViewport.setScrollBarsShown(true, true);

    commonViewport.setViewedComponent(&commonCanvas, false);
    commonViewport.setScrollBarsShown(true, true);

    addAndMakeVisible(polyViewport);
    addAndMakeVisible(resizerBar);
    addAndMakeVisible(commonViewport);

    // StretchableLayout: [polyViewport | resizerBar | commonViewport]
    // Initial 50/50 split
    layout.setItemLayout(0, 60, -1.0, -0.5);   // poly  (min 60px, preferred 50%)
    layout.setItemLayout(1, resizerThick, resizerThick, resizerThick);  // resizer
    layout.setItemLayout(2, 60, -1.0, -0.5);   // common (min 60px, preferred 50%)
}

void PatchCanvasComponent::resized()
{
    auto area = getLocalBounds();
    juce::Component* comps[] = { &polyViewport, &resizerBar, &commonViewport };
    layout.layOutComponents(comps, 3,
                            area.getX(), area.getY(),
                            area.getWidth(), area.getHeight(),
                            true,   // vertical layout
                            true);
}

void PatchCanvasComponent::setPatch(Patch* p, const ModuleDescriptions* md, const ThemeData* td)
{
    polyCanvas.setPatch(p, md, td);
    commonCanvas.setPatch(p, md, td);

    if (p != nullptr)
    {
        // Scroll each panel to show the topmost module in that section
        auto scrollTo = [](juce::Viewport& vp, const ModuleContainer& container) {
            int minY = -1;
            for (auto& m : container.getModules())
            {
                int y = m->getPosition().y * PatchCanvas::gridY;
                minY = (minY < 0) ? y : juce::jmin(minY, y);
            }
            vp.setViewPosition(0, (minY > 0) ? juce::jmax(0, minY - 20) : 0);
        };
        scrollTo(polyViewport,   p->getPolyVoiceArea());
        scrollTo(commonViewport, p->getCommonArea());
    }
}
