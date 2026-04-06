#include "PatchCanvasComponent.h"
#include "QuickAddPopup.h"
#include <cmath>
#include <set>

// --- PatchCanvas (inner scrollable surface) ---

void PatchCanvas::setLightMeterData(const int lights[128], const int meters[128])
{
    std::copy(lights, lights + 128, globalLightValues);
    std::copy(meters, meters + 128, globalMeterValues);
    repaint();
}

int PatchCanvas::computeModuleLightIndex(const Module& targetModule, int targetSection, bool forMeters) const
{
    if (patch == nullptr || themeData == nullptr) return 0;

    // Build sorted list: poly (section=1) first, then common (section=0), each sorted by containerIndex
    struct ModuleRef { const Module* mod; int section; };
    std::vector<ModuleRef> ordered;

    auto addSection = [&](const ModuleContainer& container, int sec)
    {
        size_t start = ordered.size();
        for (auto& m : container.getModules())
            ordered.push_back({ m.get(), sec });
        std::sort(ordered.begin() + static_cast<std::ptrdiff_t>(start), ordered.end(),
                  [](const ModuleRef& a, const ModuleRef& b) {
                      return a.mod->getContainerIndex() < b.mod->getContainerIndex();
                  });
    };

    addSection(patch->getPolyVoiceArea(), 1);
    addSection(patch->getCommonArea(), 0);

    int baseIndex = 0;
    for (auto& ref : ordered)
    {
        if (ref.mod == &targetModule && ref.section == targetSection)
            return baseIndex;

        // Count lights/meters for this module from its theme
        auto compId = ref.mod->getDescriptor() ? ref.mod->getDescriptor()->componentId : juce::String();
        if (const ModuleTheme* theme = themeData->getModuleTheme(compId))
        {
            for (auto& light : theme->lights)
            {
                if (forMeters && light.type == "meter") ++baseIndex;
                if (!forMeters && light.type == "led")  ++baseIndex;
            }
        }
    }
    return baseIndex;
}

PatchCanvas::PatchCanvas()
{
    setSize(canvasWidth, sectionHeight);
    setWantsKeyboardFocus(true);  // Enable keyboard input for Delete key
}

void PatchCanvas::updateSizeForZoom()
{
    int w = juce::roundToInt(canvasWidth * zoomLevel);
    int h = juce::roundToInt(sectionHeight * zoomLevel);
    setSize(w, h);
}

void PatchCanvas::setZoomLevel(float z, juce::Point<int> /*anchor*/)
{
    z = juce::jlimit(zoomMin, zoomMax, z);
    if (std::abs(z - zoomLevel) < 0.001f)
        return;
    zoomLevel = z;
    updateSizeForZoom();
    repaint();
}

void PatchCanvas::resetZoom()
{
    if (auto* vp = findParentComponentOfClass<juce::Viewport>())
    {
        auto vpCenter = vp->getViewPosition() + juce::Point<int>(vp->getWidth() / 2, vp->getHeight() / 2);
        auto canvasCenter = screenToCanvas(vpCenter);
        zoomLevel = 1.0f;
        updateSizeForZoom();
        vp->setViewPosition(canvasCenter.x - vp->getWidth() / 2,
                            canvasCenter.y - vp->getHeight() / 2);
    }
    else
    {
        zoomLevel = 1.0f;
        updateSizeForZoom();
    }
    repaint();
}

void PatchCanvas::zoomToSelection()
{
    if (selection.empty())
        return;

    int minX = 999999, minY = 999999, maxX = 0, maxY = 0;
    for (auto& sel : selection)
    {
        auto gpos = sel.module->getPosition();
        int px = gpos.x * gridX;
        int py = gpos.y * gridY;
        int ph = sel.module->getDescriptor() ? sel.module->getDescriptor()->height * gridY : 60;
        minX = juce::jmin(minX, px);
        minY = juce::jmin(minY, py);
        maxX = juce::jmax(maxX, px + gridX);
        maxY = juce::jmax(maxY, py + ph);
    }

    if (auto* vp = findParentComponentOfClass<juce::Viewport>())
    {
        int bw = maxX - minX;
        int bh = maxY - minY;
        if (bw > 0 && bh > 0)
        {
            float zx = static_cast<float>(vp->getWidth()) / static_cast<float>(bw);
            float zy = static_cast<float>(vp->getHeight()) / static_cast<float>(bh);
            float newZoom = juce::jlimit(zoomMin, zoomMax, juce::jmin(zx, zy) * 0.9f);
            zoomLevel = newZoom;
            updateSizeForZoom();

            int cx = juce::roundToInt((minX + bw / 2.0f) * newZoom);
            int cy = juce::roundToInt((minY + bh / 2.0f) * newZoom);
            vp->setViewPosition(cx - vp->getWidth() / 2, cy - vp->getHeight() / 2);
        }
    }
    repaint();
}

void PatchCanvas::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        // Zoom towards mouse cursor
        float oldZoom = zoomLevel;
        float newZoom = juce::jlimit(zoomMin, zoomMax, oldZoom + wheel.deltaY * zoomStep * 3.0f);
        if (std::abs(newZoom - oldZoom) < 0.001f)
            return;

        // Get canvas-space point under cursor before zoom
        auto canvasPt = screenToCanvas(e.getPosition());

        zoomLevel = newZoom;
        updateSizeForZoom();

        // Adjust viewport so the same canvas point stays under the cursor
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
        {
            auto vpMouse = e.getEventRelativeTo(vp).getPosition();
            int newVpX = juce::roundToInt(canvasPt.x * newZoom) - vpMouse.x;
            int newVpY = juce::roundToInt(canvasPt.y * newZoom) - vpMouse.y;
            vp->setViewPosition(juce::jmax(0, newVpX), juce::jmax(0, newVpY));
        }

        repaint();
        return;
    }

    // Default: let viewport handle normal scrolling
    juce::Component::mouseWheelMove(e, wheel);
}

void PatchCanvas::setSection(int s)
{
    mySection = s;
    updateSizeForZoom();
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
    // Reset all raw pointers into the old patch to avoid dangling refs
    dragState = DragState();
    selection.clear();
    selectedModule = nullptr;
    selectedSection = -1;
    cableSagOffsets.clear();
    activeQuickAdd = nullptr;
    showCablePreview = false;
    showModuleDropPreview = false;
    showRubberBand = false;

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

    // Apply zoom transform — all subsequent drawing is in canvas (logical) coordinates
    g.addTransform(juce::AffineTransform::scale(zoomLevel));

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
    {
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.setFont(juce::FontOptions(28.0f));
        g.drawText("Press Enter to add modules", clip, juce::Justification::centred, false);
        return;
    }

    // Each canvas instance is section-specific (mySection 0=common, 1=poly).
    // yOffset is always 0 since each canvas starts from the top.
    auto& container = (mySection == 1) ? patch->getPolyVoiceArea() : patch->getCommonArea();

    if (container.getModules().empty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.setFont(juce::FontOptions(28.0f));
        g.drawText("Press Enter to add modules", clip, juce::Justification::centred, false);
    }

    paintModules(g, container, 0);
    paintCables(g, container, 0);

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
            paintModuleThemed(g, m, mySection, rect, *theme, container);
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

void PatchCanvas::paintModuleThemed(juce::Graphics& g, const Module& m, int section, juce::Rectangle<int> bounds, const ModuleTheme& theme, const ModuleContainer& container)
{
    paintModuleBackground(g, m, bounds, theme);
    paintCustomDisplays(g, m, bounds, theme);
    paintLabels(g, bounds, theme);
    paintTextDisplays(g, m, bounds, theme);
    paintSliders(g, m, bounds, theme);
    paintKnobs(g, m, bounds, theme);
    paintButtons(g, m, bounds, theme, m.getDescriptor()->background);
    paintResetButtons(g, m, bounds, theme);
    paintStaticIcons(g, bounds, theme);
    paintConnectors(g, m, bounds, theme, container);
    paintLights(g, m, section, bounds, theme);
}

void PatchCanvas::paintModuleBackground(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    auto bgColour = m.getDescriptor()->background;

    // Module body (flat background, no title band)
    g.setColour(bgColour);
    g.fillRoundedRectangle(bounds.toFloat(), 3.0f);

    // GroupBoxes — rounded rect section borders (e.g. PWidth in OscA)
    for (auto& gb : theme.groupBoxes)
    {
        auto gbRect = juce::Rectangle<float>(
            static_cast<float>(bounds.getX() + gb.x),
            static_cast<float>(bounds.getY() + gb.y),
            static_cast<float>(gb.width),
            static_cast<float>(gb.height));
        g.setColour(bgColour.darker(0.25f));
        g.fillRoundedRectangle(gbRect, 3.0f);
        g.setColour(bgColour.darker(0.5f));
        g.drawRoundedRectangle(gbRect, 3.0f, 1.0f);
    }

    // Module name (no band, text directly on background)
    auto titleBar = juce::Rectangle<int>(bounds.getX(), bounds.getY() + 2, bounds.getWidth(), 12);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.setFont(juce::FontOptions("Fira Sans", 12.5f, juce::Font::bold));
    g.drawText(m.getTitle(), titleBar.reduced(4, 0), juce::Justification::centredLeft, true);

    // Subtle edge lines on all four sides
    g.setColour(juce::Colour(0x44000000));
    float x1 = static_cast<float>(bounds.getX());
    float y1 = static_cast<float>(bounds.getY());
    float x2 = static_cast<float>(bounds.getRight());
    float y2 = static_cast<float>(bounds.getBottom());
    g.drawLine(x1, y1, x2, y1, 1.0f); // top
    g.drawLine(x1, y2, x2, y2, 1.0f); // bottom
    g.drawLine(x1, y1, x1, y2, 1.0f); // left
    g.drawLine(x2, y1, x2, y2, 1.0f); // right

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
            // Output: rounded rectangle (square shape)
            const float cornerRadius = sz * 0.25f;
            g.setColour(connColour);
            g.fillRoundedRectangle(cx, cy, sz, sz, cornerRadius);

            g.setColour(outline);
            g.drawRoundedRectangle(cx, cy, sz, sz, cornerRadius, 1.0f);

            if (capped)
            {
                const float sqSz = innerSz * 1.1f;
                const float sqOff = (sz - sqSz) * 0.5f;
                g.setColour(connColour.darker(0.4f));
                g.fillRoundedRectangle(cx + sqOff, cy + sqOff, sqSz, sqSz, cornerRadius * 0.4f);
            }
            else
            {
                // Dark inner square (plug hole)
                const float sqSz = innerSz * 1.1f;
                const float sqOff = (sz - sqSz) * 0.5f;
                g.setColour(darkHole);
                g.fillRoundedRectangle(cx + sqOff, cy + sqOff, sqSz, sqSz, cornerRadius * 0.4f);
            }
        }
        else
        {
            // Input: filled circle
            g.setColour(connColour);
            g.fillEllipse(cx, cy, sz, sz);

            g.setColour(outline);
            g.drawEllipse(cx, cy, sz, sz, 1.0f);

            if (capped)
            {
                g.setColour(connColour.darker(0.4f));
                g.fillEllipse(cx + innerOffset, cy + innerOffset, innerSz, innerSz);
            }
            else
            {
                // Dark inner circle (socket hole)
                g.setColour(darkHole);
                g.fillEllipse(cx + innerOffset, cy + innerOffset, innerSz, innerSz);
            }
        }
    }

    // For each input connector, if there's a knob at similar Y and nearby X, draw a connecting line
    const int yTolerance = 12;
    g.setColour(juce::Colour(0xff1a1a1a));
    for (auto& tc : theme.connectors)
    {
        if (tc.cssClass == "cSLAVE") continue;
        bool isOut = false;
        for (auto& conn : m.getConnectors())
            if (conn.getDescriptor() && conn.getDescriptor()->componentId == tc.componentId)
                { isOut = conn.getDescriptor()->isOutput; break; }
        if (isOut) continue;

        float connCx = static_cast<float>(bounds.getX() + tc.x) + tc.size * 0.5f;
        float connCy = static_cast<float>(bounds.getY() + tc.y) + tc.size * 0.5f;

        float bestDist = 999.0f;
        float knobCx = -1.0f;
        float bestKnobSz = 21.0f;
        for (auto& tk : theme.knobs)
        {
            float kx = static_cast<float>(bounds.getX() + tk.x) + tk.size * 0.5f;
            float ky = static_cast<float>(bounds.getY() + tk.y) + tk.size * 0.5f;
            float dy = std::abs(ky - connCy);
            float dx = kx - connCx;
            if (dy <= static_cast<float>(yTolerance) && dx > 0.0f && dx < bestDist)
            {
                bestDist = dx;
                knobCx = kx;
                bestKnobSz = static_cast<float>(tk.size);
            }
        }

        if (knobCx > 0.0f && bestDist < 40.0f)
        {
            float renderScale = (bestKnobSz >= 26.0f) ? 0.82f : 0.78f;
            float knobRadius  = bestKnobSz * renderScale * 0.5f;
            float lineY  = connCy;
            float lineX0 = connCx + tc.size * 0.5f;
            float lineX1 = knobCx - knobRadius - 1.0f;
            if (lineX1 > lineX0)
                g.drawLine(lineX0, lineY, lineX1, lineY, 1.0f);
        }
    }
}

void PatchCanvas::paintLabels(juce::Graphics& g, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    g.setColour(juce::Colours::black);
    g.setFont(juce::FontOptions(9.0f));

    for (auto& label : theme.labels)
    {
        if (label.text.containsChar('\n'))
        {
            // Multiline label: use y as top of first line (XML positions are intentional)
            auto lines = juce::StringArray::fromLines(label.text);
            const int lineH = 10;
            const int startY = bounds.getY() + label.y;
            for (int i = 0; i < lines.size(); ++i)
            {
                g.drawText(lines[i],
                           bounds.getX() + label.x, startY + i * lineH,
                           60, lineH,
                           juce::Justification::centredLeft, true);
            }
        }
        else
        {
            g.drawText(label.text,
                       bounds.getX() + label.x, bounds.getY() + label.y,
                       120, 12,
                       juce::Justification::centredLeft, true);
        }
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

        // Knob center — always at the XML-defined centre regardless of render scale
        float centerX = cx + sz * 0.5f;
        float centerY = cy + sz * 0.5f;

        // Render the knob slightly smaller than the XML size so it looks
        // proportional on screen (original Nomad knobs are compact).
        float renderScale = (sz >= 26.0f) ? 0.82f : 0.78f;
        float rSz    = sz * renderScale;
        float radius  = rSz * 0.5f;
        float rcx    = centerX - radius;
        float rcy    = centerY - radius;

        // Knob angle: 270° range from 7 o'clock (-135°) to 5 o'clock (+135°)
        const float kRangedeg = 270.0f;
        const float kStartDeg = -135.0f;
        float knobAngleDeg = kStartDeg + normalized * kRangedeg;
        float knobAngle    = knobAngleDeg * juce::MathConstants<float>::pi / 180.0f;

        // Background circle — morph group color if assigned, grey otherwise
        g.setColour(baseColor);
        g.fillEllipse(rcx, rcy, rSz, rSz);

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
        g.drawEllipse(rcx, rcy, rSz, rSz, 1.0f);

        // Travel-limit tick marks at -135° (7 o'clock) and +135° (5 o'clock)
        // Drawn OUTSIDE the knob circle, on the module background
        {
            const float pi = juce::MathConstants<float>::pi;
            const float tickInner = radius * 1.08f;
            const float tickOuter = radius * 1.45f;
            const float limitAngles[2] = { -135.0f * pi / 180.0f, 135.0f * pi / 180.0f };
            g.setColour(juce::Colour(0xff333333));
            for (float a : limitAngles)
            {
                float sa = std::sin(a), ca = std::cos(a);
                g.drawLine(centerX + sa * tickInner, centerY - ca * tickInner,
                           centerX + sa * tickOuter, centerY - ca * tickOuter, 1.5f);
            }
        }

        // Grip line — drawn on top of everything so it's always visible
        float innerR = radius * 0.3f;
        float outerR = radius * 0.85f;
        float sinA   = std::sin(knobAngle);
        float cosA   = std::cos(knobAngle);

        g.setColour(juce::Colours::white);
        g.drawLine(centerX + sinA * innerR, centerY - cosA * innerR,
                   centerX + sinA * outerR, centerY - cosA * outerR, 1.5f);

        // Lock indicator — small padlock icon at bottom-right of knob
        if (param != nullptr && param->isLocked())
        {
            float lockSize = juce::jmax(7.0f, rSz * 0.35f);
            float lx = rcx + rSz - lockSize + 1.0f;
            float ly = rcy + rSz - lockSize + 1.0f;
            float bodyH = lockSize * 0.55f;
            float bodyW = lockSize * 0.85f;
            float bodyX = lx + (lockSize - bodyW) * 0.5f;
            float bodyY = ly + lockSize - bodyH;
            // Lock body
            g.setColour(juce::Colour(0xffE0C030));
            g.fillRoundedRectangle(bodyX, bodyY, bodyW, bodyH, 1.0f);
            // Shackle arc
            float shackleW = bodyW * 0.55f;
            float shackleH = lockSize - bodyH;
            float shackleX = bodyX + (bodyW - shackleW) * 0.5f;
            g.setColour(juce::Colour(0xffC0A020));
            juce::Path shackle;
            shackle.addArc(shackleX, ly, shackleW, shackleH * 2.0f,
                           -juce::MathConstants<float>::pi, 0.0f, true);
            g.strokePath(shackle, juce::PathStrokeType(1.5f));
        }
    }
}

// Draw a waveform/icon shape by name into the given rectangle
// Paths are drawn in a centred sub-rect (75% wide, 55% tall) so they look
// compact and proportional at any button size.
static void drawButtonIcon(juce::Graphics& g, const juce::String& iconName,
                           float ix, float iy, float iw, float ih, juce::Colour colour)
{
    g.setColour(colour);
    float mx = ix + iw * 0.5f,  my = iy + ih * 0.5f;
    float pw = iw * 0.75f,       ph = ih * 0.55f;
    float x0 = mx - pw * 0.5f,  x1 = mx + pw * 0.5f;
    float y0 = my - ph * 0.5f,  y1 = my + ph * 0.5f;

    juce::Path p;

    if (iconName == "wf_sine")
    {
        p.startNewSubPath(x0, my);
        for (int i = 0; i <= 32; ++i)
        {
            float t = static_cast<float>(i) / 32.0f;
            p.lineTo(x0 + pw * t,
                     my - ph * 0.5f * std::sin(t * juce::MathConstants<float>::twoPi));
        }
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "wf_tri")
    {
        p.startNewSubPath(x0, my);
        p.lineTo(x0 + pw * 0.25f, y0);
        p.lineTo(x0 + pw * 0.75f, y1);
        p.lineTo(x1, my);
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "wf_saw")
    {
        p.startNewSubPath(x0, y1);
        p.lineTo(x1, y0);
        p.lineTo(x1, y1);
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else if (iconName == "wf_square")
    {
        p.startNewSubPath(x0, my);
        p.lineTo(x0, y0);
        p.lineTo(mx, y0);
        p.lineTo(mx, y1);
        p.lineTo(x1, y1);
        p.lineTo(x1, my);
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }
    else
    {
        g.drawEllipse(ix + iw * 0.1f, iy + ih * 0.1f, iw * 0.8f, ih * 0.8f, 1.0f);
    }
}

void PatchCanvas::paintButtons(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme, juce::Colour moduleBg)
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

        // Helper: draw a single bevel-effect button segment
        // pressed=true → sunken look; false → raised look
        auto drawBevelSegment = [&](float sx, float sy, float sw, float sh,
                                    bool pressed, juce::Colour baseFill,
                                    const juce::String& label, juce::Colour labelColour,
                                    const juce::String& iconRef = juce::String())
        {
            // Fill
            g.setColour(baseFill);
            g.fillRect(sx, sy, sw, sh);

            // Bevel edges: raised = light top/left, dark bottom/right; pressed = inverted
            juce::Colour hiEdge  = baseFill.brighter(0.55f);
            juce::Colour loEdge  = baseFill.darker(0.55f);
            juce::Colour topLeft  = pressed ? loEdge : hiEdge;
            juce::Colour botRight = pressed ? hiEdge : loEdge;

            g.setColour(topLeft);
            g.drawLine(sx, sy, sx + sw, sy, 1.0f);
            g.drawLine(sx, sy, sx, sy + sh, 1.0f);
            g.setColour(botRight);
            g.drawLine(sx, sy + sh - 1.0f, sx + sw, sy + sh - 1.0f, 1.0f);
            g.drawLine(sx + sw - 1.0f, sy, sx + sw - 1.0f, sy + sh, 1.0f);

            float ox = pressed ? 1.0f : 0.0f;
            float oy = pressed ? 1.0f : 0.0f;

            if (iconRef.isNotEmpty())
            {
                drawButtonIcon(g, iconRef,
                               sx + ox + 1.0f, sy + oy + 1.0f,
                               sw - 2.0f, sh - 2.0f, labelColour);
            }
            else if (label.isNotEmpty())
            {
                float fontSize = juce::jmin(8.0f, juce::jmin(sw * 0.85f, sh - 2.0f));
                if (fontSize < 4.0f) fontSize = 4.0f;
                g.setColour(labelColour);
                g.setFont(juce::FontOptions("Fira Sans", fontSize, juce::Font::bold));
                g.drawText(label,
                           static_cast<int>(sx + ox), static_cast<int>(sy + oy),
                           static_cast<int>(sw), static_cast<int>(sh),
                           juce::Justification::centred, true);
            }
        };

        // --- Radio-selector buttons (cyclic=false, multiple options) ---
        if (!tb.cyclic && numOptions > 1)
        {
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
                    // Vertical: index 0 = bottom, last = top (matches original)
                    int ri = numOptions - 1 - i;
                    segW = bw;
                    segH = bh / static_cast<float>(numOptions);
                    segX = bx;
                    segY = by + static_cast<float>(ri) * segH;
                }

                bool selected = (i == val);

                juce::String segLabel;
                if (i < static_cast<int>(tb.labels.size()))
                    segLabel = tb.labels[static_cast<size_t>(i)];

                juce::String segIcon;
                if (i < static_cast<int>(tb.imageRefs.size()))
                    segIcon = tb.imageRefs[static_cast<size_t>(i)];

                // Only show numeric fallback if no label AND no icon
                if (segLabel.isEmpty() && segIcon.isEmpty())
                    segLabel = juce::String(i);

                juce::Colour base  = selected ? moduleBg.brighter(0.25f).withSaturation(0.5f) : moduleBg.darker(0.15f);
                juce::Colour label = selected ? juce::Colour(0xff111111) : juce::Colour(0xff333333);
                drawBevelSegment(segX, segY, segW, segH, selected, base, segLabel, label, segIcon);
            }

            // Outer border
            g.setColour(juce::Colour(0xff222222));
            g.drawRect(bx, by, bw, bh, 1.0f);
            continue;
        }

        // --- Toggle buttons (cyclic=true) or single-option ---
        bool isOn = (val > 0);

        // Determine label text for current state
        juce::String labelText;
        if (!tb.labels.empty())
        {
            if (val >= 0 && val < numOptions)
                labelText = tb.labels[static_cast<size_t>(val)];
            else
                labelText = tb.labels[0];
        }
        if (labelText.isEmpty() && param != nullptr)
            labelText = juce::String(val);

        // --- Mute / compact toggle: small button (<=20x20) rendered as connector-sized square ---
        if (tb.cyclic && bw <= 20.0f && bh <= 20.0f)
        {
            const float sq = 13.0f;
            float sx = bx + (bw - sq) * 0.5f;
            float sy = by + (bh - sq) * 0.5f;

            juce::Colour muteBase = isOn ? juce::Colour(0xffcc4444) : moduleBg.darker(0.2f);
            juce::Colour muteText = isOn ? juce::Colours::white : juce::Colour(0xff444444);
            drawBevelSegment(sx, sy, sq, sq, isOn, muteBase, labelText, muteText);

            g.setColour(juce::Colour(0xff222222));
            g.drawRect(sx, sy, sq, sq, 1.0f);
            continue;
        }

        juce::Colour base      = isOn ? moduleBg.brighter(0.2f).withSaturation(0.4f) : moduleBg.darker(0.15f);
        juce::Colour labelCol  = isOn ? juce::Colour(0xff111111) : juce::Colour(0xff333333);
        drawBevelSegment(bx, by, bw, bh, isOn, base, labelText, labelCol);

        // Outer border
        g.setColour(juce::Colour(0xff222222));
        g.drawRect(bx, by, bw, bh, 1.0f);
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

        // Lock indicator — small yellow dot at bottom-right corner
        if (param != nullptr && param->isLocked())
        {
            g.setColour(juce::Colour(0xffE0C030));
            g.fillEllipse(sx + sw - 5.0f, sy + sh - 5.0f, 4.0f, 4.0f);
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

        // Dark blue background — clip height to max 13px for compact look
        float renderH = juce::jmin(dh, 13.0f);
        float renderY = dy + (dh - renderH) * 0.5f;

        g.setColour(juce::Colour(0xff2A2560));
        g.fillRect(dx, renderY, dw, renderH);

        // Inner-bevel border: darker on top/left, slightly lighter on bottom/right
        g.setColour(juce::Colour(0xff181440));
        g.drawLine(dx, renderY,           dx + dw, renderY,           1.0f);
        g.drawLine(dx, renderY,           dx,       renderY + renderH, 1.0f);
        g.setColour(juce::Colour(0xff4A3FA0));
        g.drawLine(dx,       renderY + renderH, dx + dw, renderY + renderH, 1.0f);
        g.drawLine(dx + dw,  renderY,           dx + dw, renderY + renderH, 1.0f);

        // Value text
        auto* param = findParameter(m, td.componentId);
        if (param != nullptr)
        {
            int val = param->getValue();
            juce::String displayStr;

            if (td.noteFormat)
            {
                // Clavia convention: MIDI 0=C0, MIDI 60=C5, MIDI 127=G10
                static const char* noteNames[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
                int octave = val / 12;
                displayStr = juce::String(noteNames[val % 12]) + juce::String(octave);
            }
            else if (td.partialFormat)
            {
                // Slave oscillator partial ratio (nmformat.js fmtPartials)
                // Values at multiples of 12 offset by 4: 4,16,28,40,52,64,76,88,100,112,124
                static const char* partialFractions[] =
                    { "1:32","1:16","1:8","1:4","1:2","1:1","2:1","4:1","8:1","16:1","32:1" };
                if ((val + 8) % 12 == 0)
                {
                    int idx = val / 12;
                    if (idx >= 0 && idx <= 10)
                        displayStr = partialFractions[idx];
                    else
                        displayStr = juce::String(val);
                }
                else
                {
                    double ratio = std::pow(2.0, (val - 64.0) / 12.0);
                    displayStr = "x" + juce::String(ratio, 3);
                }
            }
            else
            {
                displayStr = juce::String(val);
            }

            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(8.5f));
            g.drawText(displayStr,
                       static_cast<int>(dx), static_cast<int>(renderY),
                       static_cast<int>(dw), static_cast<int>(renderH),
                       juce::Justification::centred, true);
        }

        // Partial format: draw ◄ ► arrow buttons below the display box
        if (td.partialFormat)
        {
            float arrowY = renderY + renderH + 1.0f;
            float arrowH = 8.0f;
            float midX   = dx + dw * 0.5f;

            // Button backgrounds
            g.setColour(juce::Colour(0xff2a2a2a));
            g.fillRect(dx, arrowY, dw, arrowH);
            g.setColour(juce::Colour(0xff444444));
            g.drawRect(dx, arrowY, dw, arrowH, 1.0f);
            g.drawLine(midX, arrowY, midX, arrowY + arrowH, 1.0f);

            // ◄ left arrow (decrement partial)
            float lCx = dx + dw * 0.25f, cy = arrowY + arrowH * 0.5f;
            juce::Path la;
            la.addTriangle(lCx - 3.0f, cy, lCx + 2.5f, cy - 2.5f, lCx + 2.5f, cy + 2.5f);
            g.setColour(juce::Colour(0xffaaaaaa));
            g.fillPath(la);

            // ► right arrow (increment partial)
            float rCx = dx + dw * 0.75f;
            juce::Path ra;
            ra.addTriangle(rCx + 3.0f, cy, rCx - 2.5f, cy - 2.5f, rCx - 2.5f, cy + 2.5f);
            g.fillPath(ra);
        }
    }
}

void PatchCanvas::paintResetButtons(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    for (auto& rb : theme.resetButtons)
    {
        auto* param = findParameter(m, rb.componentId);
        int val = (param != nullptr) ? param->getValue() : -1;
        bool atDefault = (val == rb.defaultValue);

        float rx = static_cast<float>(bounds.getX() + rb.x);
        float ry = static_cast<float>(bounds.getY() + rb.y);
        float rw = static_cast<float>(rb.width);
        float rh = static_cast<float>(rb.height);

        juce::Path tri;
        tri.addTriangle(rx, ry, rx + rw, ry, rx + rw * 0.5f, ry + rh);
        g.setColour(atDefault ? juce::Colour(0xff44cc44) : juce::Colour(0xff2a4a2a));
        g.fillPath(tri);
        g.setColour(juce::Colour(0xff111111));
        g.strokePath(tri, juce::PathStrokeType(0.5f));
    }
}

void PatchCanvas::paintStaticIcons(juce::Graphics& g, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    for (auto& si : theme.staticIcons)
    {
        const float scale  = 1.14f;
        const float pad    = 4.0f;
        const float margin = 2.0f;   // min gap from module edge
        float iw = static_cast<float>(si.width)  * scale;
        float ih = static_cast<float>(si.height) * scale;

        // Clamp box so there's always `margin` px gap from every module edge
        float bw = iw + pad * 2.0f;
        float bh = ih + pad * 2.0f;
        float bx = static_cast<float>(bounds.getX() + si.x) - pad - 3.0f;
        float by = static_cast<float>(bounds.getY() + si.y) - pad + 5.0f;
        bx = juce::jlimit(static_cast<float>(bounds.getX())  + margin,
                          static_cast<float>(bounds.getRight())  - bw - margin, bx);
        by = juce::jlimit(static_cast<float>(bounds.getY())  + margin,
                          static_cast<float>(bounds.getBottom()) - bh - margin, by);

        // Icon follows the (possibly shifted) box
        float ix = bx + pad;
        float iy = by + pad;

        // Semi-transparent black rounded box
        g.setColour(juce::Colour(0x66000000));
        g.fillRoundedRectangle(bx, by, bw, bh, 3.0f);
        g.setColour(juce::Colour(0x66ffffff));
        g.drawRoundedRectangle(bx, by, bw, bh, 3.0f, 1.0f);

        // Waveform icon in white
        drawButtonIcon(g, si.iconName, ix, iy, iw, ih, juce::Colours::white);
    }
}

void PatchCanvas::paintLights(juce::Graphics& g, const Module& m, int section, juce::Rectangle<int> bounds, const ModuleTheme& theme)
{
    // Compute base indices for this module's LEDs and meters in the global arrays
    int ledBase   = computeModuleLightIndex(m, section, false);
    int meterBase = computeModuleLightIndex(m, section, true);

    // Build map of meter vertical centers (for LED alignment) and meter index per component-id
    std::map<juce::String, float> meterCenterY;
    std::map<juce::String, int>   meterGlobalIdx;
    int meterCount = 0;
    for (auto& tl : theme.lights)
    {
        if (tl.type == "meter")
        {
            float cy = static_cast<float>(bounds.getY() + tl.y) + tl.height * 0.5f;
            // Only store first occurrence for center (multiple meters per channel use same id)
            if (meterCenterY.find(tl.componentId) == meterCenterY.end())
                meterCenterY[tl.componentId] = cy;
            meterGlobalIdx[tl.componentId] = meterBase + meterCount;
            ++meterCount;
        }
    }

    // Track LED index per component-id
    std::map<juce::String, int> ledGlobalIdx;
    int ledCount = 0;
    for (auto& tl : theme.lights)
    {
        if (tl.type == "led")
        {
            ledGlobalIdx[tl.componentId] = ledBase + ledCount;
            ++ledCount;
        }
    }

    for (auto& tl : theme.lights)
    {
        float lx = static_cast<float>(bounds.getX() + tl.x) + (tl.type == "led" ? 2.0f : 0.0f);
        float lw = static_cast<float>(tl.width);
        float lh = static_cast<float>(tl.height);

        if (tl.type == "led")
        {
            // Vertically centre LED on its paired meter
            float ly;
            auto it = meterCenterY.find(tl.componentId);
            if (it != meterCenterY.end())
                ly = it->second - lh * 0.5f;
            else
                ly = static_cast<float>(bounds.getY() + tl.y);

            // Determine if LED is on:
            // - ledOnValue >= 0: driven by paired meter reaching that threshold
            // - otherwise: driven by globalLightValues
            bool ledOn = false;
            if (tl.ledOnValue >= 0)
            {
                int mIdx = meterGlobalIdx.count(tl.componentId) ? meterGlobalIdx[tl.componentId] : meterBase;
                int mVal = (mIdx < 128) ? globalMeterValues[mIdx] : 0;
                ledOn = (mVal >= tl.ledOnValue);
            }
            else
            {
                int ledIdx = ledGlobalIdx.count(tl.componentId) ? ledGlobalIdx[tl.componentId] : ledBase;
                ledOn = (ledIdx < 128) && (globalLightValues[ledIdx] > 0);
            }

            if (ledOn)
            {
                // On: bright red (clipping indicator)
                g.setColour(juce::Colour(0xffff2200));
                g.fillEllipse(lx, ly, lw, lh);
                g.setColour(juce::Colour(0xffff6644));
                g.drawEllipse(lx, ly, lw, lh, 0.5f);
            }
            else
            {
                // Off: dark circle
                g.setColour(juce::Colour(0xff333333));
                g.fillEllipse(lx, ly, lw, lh);
                g.setColour(juce::Colour(0xff555555));
                g.drawEllipse(lx, ly, lw, lh, 0.5f);
            }
        }
        else  // meter
        {
            float ly = static_cast<float>(bounds.getY() + tl.y);

            // Background
            g.setColour(juce::Colour(0xff222222));
            g.fillRect(lx, ly, lw, lh);

            // Get meter value (0-127)
            int mIdx = meterGlobalIdx.count(tl.componentId) ? meterGlobalIdx[tl.componentId] : meterBase;
            int mVal = (mIdx < 128) ? globalMeterValues[mIdx] : 0;

            if (mVal > 0)
            {
                float fill = static_cast<float>(mVal) / 127.0f;
                float barW = lw * fill;

                // Colour: green → yellow → red based on level
                juce::Colour barColour;
                if (fill < 0.6f)       barColour = juce::Colour(0xff22cc44);
                else if (fill < 0.85f) barColour = juce::Colour(0xffddcc00);
                else                   barColour = juce::Colour(0xffee2200);

                g.setColour(barColour);
                g.fillRect(lx, ly, barW, lh);
            }

            // dB scale below meter bar — only between meters (not after the last one)
            if (lw >= 60.0f && ly + lh + 12.0f < static_cast<float>(bounds.getBottom()))
            {
                const int dbMarks[] = { 0, -6, -12, -18, -24, -30 };
                g.setColour(juce::Colours::black);
                g.setFont(juce::FontOptions(6.0f));
                for (int db : dbMarks)
                {
                    float t = 1.0f + db / 30.0f;   // 0 dB → t=1.0, -30 dB → t=0.0
                    float tx = lx + lw * t;
                    g.drawLine(tx, ly + lh, tx, ly + lh + 2.0f, 1.0f);
                    juce::String label = (db == 0) ? "0" : juce::String(db);
                    g.drawText(label,
                               static_cast<int>(tx) - 8, static_cast<int>(ly + lh + 2),
                               16, 8,
                               juce::Justification::centred, false);
                }
            }
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

    auto pos = screenToCanvas(e.getPosition());

    // Middle-click: start canvas pan (drag to scroll viewport)
    if (e.mods.isMiddleButtonDown())
    {
        dragState = DragState();
        dragState.type = DragState::CanvasPan;
        dragState.startPos = e.getScreenPosition();  // absolute screen coords for pan
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        return;
    }

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
                            // Fire undo callbacks for each cable before removing
                            if (cableDeletedCallback && undoManager)
                            {
                                undoManager->beginNewTransaction("Delete Cables");
                                for (auto& cable : area.container->getConnections())
                                {
                                    if (cable.output == conn || cable.input == conn)
                                    {
                                        auto findOwner = [&](Connector* c) -> Module* {
                                            for (auto& mp : area.container->getModules())
                                                for (auto& mc : mp->getConnectors())
                                                    if (&mc == c) return mp.get();
                                            return nullptr;
                                        };
                                        auto* outMod = findOwner(cable.output);
                                        auto* inMod = findOwner(cable.input);
                                        if (outMod && inMod)
                                            cableDeletedCallback(area.section,
                                                outMod->getContainerIndex(), cable.output->getDescriptor()->index, cable.output->getDescriptor()->isOutput,
                                                inMod->getContainerIndex(), cable.input->getDescriptor()->index, cable.input->getDescriptor()->isOutput);
                                    }
                                }
                            }
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

                        int oldButtonValue = dragState.parameter->getValue();
                        dragState.parameter->setValue(newValue);

                        if (parameterChangeCallback)
                            parameterChangeCallback(dragState.section, m.getContainerIndex(), pd->index, newValue);
                        // Button clicks complete immediately — fire drag complete for undo
                        if (paramDragCompleteCallback)
                            paramDragCompleteCallback(dragState.section, m.getContainerIndex(), pd->index, oldButtonValue, newValue);

                        repaint();
                        return;
                    }
                }
            }

            // Test partial arrow buttons on textDisplays
            for (auto& td : theme->textDisplays)
            {
                if (!td.partialFormat) continue;

                // Arrow row geometry (mirrors paintTextDisplays)
                float dh      = static_cast<float>(td.height);
                float renderH = juce::jmin(dh, 13.0f);
                float renderY = td.y + (dh - renderH) * 0.5f;
                float arrowY  = renderY + renderH + 1.0f;
                float arrowH  = 8.0f;

                juce::Rectangle<float> arrowRect(static_cast<float>(td.x), arrowY,
                                                 static_cast<float>(td.width), arrowH);
                if (!arrowRect.contains(relPos.toFloat())) continue;

                auto* param     = findParameter(m, td.componentId);   // p2
                auto* fineParam = findParameter(m, "p3");              // fine detune
                if (param == nullptr) continue;

                static const int kSnaps[]  = {4, 16, 28, 40, 52, 64, 76, 88, 100, 112, 124};
                static const int kNumSnaps = 11;
                int val    = param->getValue();
                int newVal = val;
                bool goUp  = (relPos.x > td.x + td.width / 2);

                if (goUp)
                {
                    for (int i = 0; i < kNumSnaps; ++i)
                        if (kSnaps[i] > val) { newVal = kSnaps[i]; break; }
                }
                else
                {
                    for (int i = kNumSnaps - 1; i >= 0; --i)
                        if (kSnaps[i] < val) { newVal = kSnaps[i]; break; }
                }

                if (newVal != val)
                {
                    int oldVal = val;
                    param->setValue(newVal);
                    if (parameterChangeCallback)
                        parameterChangeCallback(area.section, m.getContainerIndex(),
                                                param->getDescriptor()->index, newVal);
                    if (paramDragCompleteCallback)
                        paramDragCompleteCallback(area.section, m.getContainerIndex(),
                                                  param->getDescriptor()->index, oldVal, newVal);

                    // Reset fine (p3) to center/zero (value 64)
                    if (fineParam != nullptr && fineParam->getValue() != 64)
                    {
                        int oldFine = fineParam->getValue();
                        fineParam->setValue(64);
                        if (parameterChangeCallback)
                            parameterChangeCallback(area.section, m.getContainerIndex(),
                                                    fineParam->getDescriptor()->index, 64);
                        if (paramDragCompleteCallback)
                            paramDragCompleteCallback(area.section, m.getContainerIndex(),
                                                      fineParam->getDescriptor()->index, oldFine, 64);
                    }

                    repaint();
                    return;
                }
                return; // already at limit — consume click anyway
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
                menu.addItem(6, "Initialize Module");
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
                            if (undoManager)
                                undoManager->beginNewTransaction("Delete Module");
                            if (deleteModuleCallback)
                                deleteModuleCallback(sec, modPtr);
                            if (selectedModule == modPtr)
                                clearSelection();
                            repaint();
                        }
                        else if (result == 6)
                        {
                            if (initModuleCallback)
                                initModuleCallback(sec, modPtr);
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
                dragState.startGridPos = m.getPosition();
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
                    {
                        if (undoManager) undoManager->beginNewTransaction("Add Module");
                        moduleDropCallback(typeIndex, clickSection, clickGX, clickGY, desc->name);
                    }
                }
            });
    }
}

void PatchCanvas::mouseDrag(const juce::MouseEvent& e)
{
    if (dragState.type == DragState::None)
        return;

    auto currentPos = screenToCanvas(e.getPosition());

    if (dragState.type == DragState::CanvasPan)
    {
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
        {
            auto screenPos = e.getScreenPosition();  // absolute screen coords — stable
            int dx = screenPos.x - dragState.startPos.x;
            int dy = screenPos.y - dragState.startPos.y;
            auto vpos = vp->getViewPosition();
            vp->setViewPosition(vpos.x - dx, vpos.y - dy);
            dragState.startPos = screenPos;
        }
        return;
    }

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

    if (dragState.type == DragState::CanvasPan)
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        dragState = DragState();
        return;
    }

    if (dragState.type == DragState::RubberBand)
    {
        showRubberBand = false;
        dragState = DragState();
        repaint();
        return;
    }

    if (dragState.type == DragState::MultiModuleMove)
    {
        if (moduleMoveCallback && undoManager)
        {
            undoManager->beginNewTransaction("Move Modules");
            for (auto& ms : multiMoveState)
            {
                auto newPos = ms.module->getPosition();
                if (newPos != ms.startGridPos)
                    moduleMoveCallback(ms.section, ms.module->getContainerIndex(),
                                       ms.startGridPos, newPos);
            }
        }
        multiMoveState.clear();
        dragState = DragState();
        return;
    }

    if (dragState.type == DragState::ModuleMove)
    {
        if (moduleMoveCallback && undoManager && dragState.module)
        {
            auto newPos = dragState.module->getPosition();
            if (newPos != dragState.startGridPos)
            {
                undoManager->beginNewTransaction("Move Module");
                moduleMoveCallback(dragState.section, dragState.module->getContainerIndex(),
                                   dragState.startGridPos, newPos);
            }
        }
        dragState = DragState();
        return;
    }

    if (dragState.type == DragState::CableCreate)
    {
        showCablePreview = false;
        auto hit = findConnectorAt(screenToCanvas(e.getPosition()));
        if (hit.connector != nullptr && hit.connector != dragState.sourceConnector
            && hit.section == dragState.section)
        {
            auto* src = dragState.sourceConnector;
            auto* dst = hit.connector;
            bool srcOut = src->getDescriptor()->isOutput;
            bool dstOut = dst->getDescriptor()->isOutput;
            auto& container = patch->getContainer(dragState.section);

            // Connect output→input, auto-swap if needed
            Connector* outConn = nullptr;
            Connector* inConn = nullptr;
            if (srcOut && !dstOut) { outConn = src; inConn = dst; }
            else if (!srcOut && dstOut) { outConn = dst; inConn = src; }

            if (outConn && inConn)
            {
                container.addConnection(outConn, inConn);

                if (cableCreatedCallback && dragState.module)
                {
                    if (undoManager)
                        undoManager->beginNewTransaction("Add Cable");
                    // Find module owners for undo info
                    auto findOwner = [&](Connector* c) -> Module* {
                        for (auto& mp : container.getModules())
                            for (auto& mc : mp->getConnectors())
                                if (&mc == c) return mp.get();
                        return nullptr;
                    };
                    auto* outMod = findOwner(outConn);
                    auto* inMod = findOwner(inConn);
                    if (outMod && inMod)
                        cableCreatedCallback(dragState.section,
                            outMod->getContainerIndex(), outConn->getDescriptor()->index, true,
                            inMod->getContainerIndex(), inConn->getDescriptor()->index, false);
                }
            }
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
    if (dragState.type != DragState::Button && dragState.module != nullptr)
    {
        int finalValue = dragState.parameter->getValue();
        if (finalValue != dragState.lastSentValue && parameterChangeCallback)
        {
            auto* pd = dragState.parameter->getDescriptor();
            parameterChangeCallback(dragState.section, dragState.module->getContainerIndex(), pd->index, finalValue);
        }

        // Fire drag complete for undo (knobs/sliders only — buttons fire on mouseDown)
        if (paramDragCompleteCallback && finalValue != dragState.startValue)
        {
            auto* pd = dragState.parameter->getDescriptor();
            paramDragCompleteCallback(dragState.section, dragState.module->getContainerIndex(),
                                      pd->index, dragState.startValue, finalValue);
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
            pasteFromClipboard(screenToCanvas({ getWidth() / 2, getHeight() / 4 }));
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

        auto mousePos = screenToCanvas(getMouseXYRelative());

        int section = mySection;
        int gx = juce::jlimit(0, 39, mousePos.x / gridX);
        int gy = juce::jlimit(0, 127, mousePos.y / gridY);

        auto screenPos = localPointToGlobal(getMouseXYRelative());

        activeQuickAdd = new QuickAddPopup(
            *moduleDescs, screenPos, gx, gy,
            [this, section](const ModuleDescriptor* desc, int pgx, int pgy)
            {
                if (moduleDropCallback && desc)
                {
                    if (undoManager) undoManager->beginNewTransaction("Add Module");
                    moduleDropCallback(desc->index, section, pgx, pgy, desc->name);
                }
            },
            [this]() { activeQuickAdd = nullptr; }
        );

        activeQuickAdd->grabFocusNow();
        return true;
    }

    // Ctrl+R → randomize (simple), Ctrl+Shift+R → randomize (gaussian)
    if (key == juce::KeyPress('r', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        if (fileCommandCallback) { fileCommandCallback("randomizeGaussian"); return true; }
    }
    if (key == juce::KeyPress('r', juce::ModifierKeys::commandModifier, 0))
    {
        if (fileCommandCallback) { fileCommandCallback("randomize"); return true; }
    }

    // Ctrl+Z → undo
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0))
    {
        if (undoCallback) { undoCallback(); return true; }
    }

    // Ctrl+Shift+Z → redo
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        if (redoCallback) { redoCallback(); return true; }
    }

    // Ctrl+Y → redo (alternative)
    if (key == juce::KeyPress('y', juce::ModifierKeys::commandModifier, 0))
    {
        if (redoCallback) { redoCallback(); return true; }
    }

    // Ctrl+N → new patch, Ctrl+O → open, Ctrl+S → save, Ctrl+W → close
    if (fileCommandCallback)
    {
        if (key == juce::KeyPress('n', juce::ModifierKeys::commandModifier, 0))
            { fileCommandCallback("new"); return true; }
        if (key == juce::KeyPress('o', juce::ModifierKeys::commandModifier, 0))
            { fileCommandCallback("open"); return true; }
        if (key == juce::KeyPress('s', juce::ModifierKeys::commandModifier, 0))
            { fileCommandCallback("save"); return true; }
        if (key == juce::KeyPress('p', juce::ModifierKeys::commandModifier, 0))
            { fileCommandCallback("patchSettings"); return true; }
    }

    // F1 → show help popup for the selected/hovered module
    if (key == juce::KeyPress::F1Key)
    {
        // Prefer the module under the mouse cursor, fall back to last selected
        Module* target = nullptr;
        auto mousePos = screenToCanvas(getMouseXYRelative());
        if (patch != nullptr)
        {
            for (auto& modPtr : patch->getPolyVoiceArea().getModules())
            {
                auto pos = modPtr->getPosition();
                int pw = 255;
                int ph = (modPtr->getDescriptor() ? modPtr->getDescriptor()->height * 15 : 60);
                juce::Rectangle<int> bounds(pos.x * gridX, pos.y * gridY, pw, ph);
                if (bounds.contains(mousePos)) { target = modPtr.get(); break; }
            }
            if (!target)
                for (auto& modPtr : patch->getCommonArea().getModules())
                {
                    auto pos = modPtr->getPosition();
                    int ph = (modPtr->getDescriptor() ? modPtr->getDescriptor()->height * 15 : 60);
                    juce::Rectangle<int> bounds(pos.x * gridX, pos.y * gridY, 255, ph);
                    if (bounds.contains(mousePos)) { target = modPtr.get(); break; }
                }
        }
        if (!target && selectedModule != nullptr)
            target = selectedModule;

        if (target && target->getDescriptor())
        {
            // Pass both fullname and short name separated by '|' so findModuleHelp
            // can try each — e.g. "12/18/24dB Classic Low Pass Filter|FilterF"
            juce::String helpQuery = target->getDescriptor()->fullname
                                   + "|" + target->getDescriptor()->name;
            ModuleHelpPopup::show(helpQuery, this);
        }

        return true;
    }

    // Ctrl++ → Zoom In, Ctrl+- → Zoom Out
    if (key.getModifiers().isCommandDown())
    {
        if (key.getKeyCode() == '+' || key.getKeyCode() == '=' || key.getKeyCode() == juce::KeyPress::numberPadAdd)
        {
            setZoomLevel(zoomLevel + zoomStep);
            return true;
        }
        if (key.getKeyCode() == '-' || key.getKeyCode() == juce::KeyPress::numberPadSubtract)
        {
            setZoomLevel(zoomLevel - zoomStep);
            return true;
        }
    }

    // Shift+Z → always reset zoom to 100%
    if (key == juce::KeyPress('z', juce::ModifierKeys::shiftModifier, 0))
    {
        resetZoom();
        return true;
    }

    // Z → zoom-to-selection (if any) or reset to 100%
    if (key.getTextCharacter() == 'z' && !key.getModifiers().isCommandDown())
    {
        if (!selection.empty())
            zoomToSelection();
        else
            resetZoom();
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

    auto mousePos = screenToCanvas(dragSourceDetails.localPosition.toInt());
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

    auto mousePos = screenToCanvas(dragSourceDetails.localPosition.toInt());
    int section = mySection;
    int dropX = juce::jlimit(0, 39, mousePos.x / PatchCanvas::gridX);
    int dropY = juce::jlimit(0, 127, mousePos.y / PatchCanvas::gridY);

    // Trigger callback if set
    if (moduleDropCallback)
    {
        if (undoManager) undoManager->beginNewTransaction("Add Module");
        moduleDropCallback(typeId, section, dropX, dropY, moduleName);
    }

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

    // 2a. Lock Parameter (toggle)
    menu.addItem(3, param.isLocked() ? "Unlock Parameter" : "Lock Parameter");

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

            if (result == 3)
            {
                // Toggle lock
                param.setLocked(!param.isLocked());
                repaint();
            }
            else if (result == 1)
            {
                // Set to default value
                int oldVal = param.getValue();
                param.setValue(pd2->defaultValue);
                if (parameterChangeCallback)
                    parameterChangeCallback(section, m.getContainerIndex(),
                                           pd2->index, pd2->defaultValue);
                if (paramDragCompleteCallback && oldVal != pd2->defaultValue)
                    paramDragCompleteCallback(section, m.getContainerIndex(),
                                              pd2->index, oldVal, pd2->defaultValue);
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

    if (undoManager)
        undoManager->beginNewTransaction("Delete Selection");

    for (auto& sel : selection)
    {
        if (deleteModuleCallback)
            deleteModuleCallback(sel.section, sel.module);
        else
        {
            auto& container = patch->getContainer(sel.section);
            container.removeModule(sel.module);
        }
    }
    clearSelection();
    repaint();
}

void PatchCanvas::duplicateSelection(bool withCables)
{
    if (selection.empty() || patch == nullptr || moduleDescs == nullptr) return;

    if (undoManager)
        undoManager->beginNewTransaction("Duplicate");

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

    if (undoManager)
        undoManager->beginNewTransaction("Paste");

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
    menu.addItem(5, "Initialize");
    menu.addSeparator();
    menu.addItem(4, "Delete");

    menu.showMenuAsync(juce::PopupMenu::Options{}, [this](int result) {
        if (result == 1) duplicateSelection(false);
        else if (result == 2) duplicateSelection(true);
        else if (result == 3) copySelectionToClipboard();
        else if (result == 4) deleteSelection();
        else if (result == 5) {
            if (initModuleCallback) {
                if (undoManager)
                    undoManager->beginNewTransaction("Initialize Selection");
                for (auto& sel : selection)
                    if (sel.module)
                        initModuleCallback(sel.section, sel.module);
            }
        }
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
    layout.setItemLayout(0, 60, -1.0, -0.9);   // poly  (min 60px, preferred 90%)
    layout.setItemLayout(1, resizerThick, resizerThick, resizerThick);  // resizer
    layout.setItemLayout(2, 60, -1.0, -0.1);   // common (min 60px, preferred 10%)
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
