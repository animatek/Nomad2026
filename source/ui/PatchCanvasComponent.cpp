#include "PatchCanvasComponent.h"

// --- PatchCanvas (inner scrollable surface) ---

PatchCanvas::PatchCanvas()
{
    setSize(canvasWidth, canvasHeight);
}

void PatchCanvas::setPatch(Patch* p, const ModuleDescriptions* md)
{
    patch = p;
    moduleDescs = md;
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

juce::Point<int> PatchCanvas::getConnectorPosition(const Module& m, const Connector& conn, int yOffset) const
{
    auto bounds = getModuleBounds(m, yOffset);
    auto* desc = conn.getDescriptor();

    // Count how many connectors of each type to compute vertical spacing
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
    int headerH = 14; // title area

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

    // Draw grid dots at column boundaries
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

    // Calculate separator position — poly area on top, common area below
    int separatorY = patch->getHeader().separatorPosition * gridY;
    if (separatorY == 0)
        separatorY = canvasHeight / 2;

    // Draw separator line
    g.setColour(juce::Colour(0xff5555aa));
    g.drawHorizontalLine(separatorY, 0.0f, static_cast<float>(canvasWidth));

    // Labels
    g.setColour(juce::Colour(0xff8888cc));
    g.setFont(juce::FontOptions(12.0f));
    g.drawText("POLY", 4, 4, 60, 14, juce::Justification::centredLeft);
    g.drawText("COMMON", 4, separatorY + 4, 80, 14, juce::Justification::centredLeft);

    // Paint cables first (behind modules)
    paintCables(g, patch->getPolyVoiceArea(), polyAreaOffsetY);
    paintCables(g, patch->getCommonArea(), separatorY);

    // Paint modules
    paintModules(g, patch->getPolyVoiceArea(), polyAreaOffsetY);
    paintModules(g, patch->getCommonArea(), separatorY);
}

void PatchCanvas::paintModules(juce::Graphics& g, const ModuleContainer& container, int yOffset)
{
    for (auto& modulePtr : container.getModules())
    {
        auto& m = *modulePtr;
        auto rect = getModuleBounds(m, yOffset);

        // Module background
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

        // Draw connector dots
        for (auto& conn : m.getConnectors())
        {
            auto pos = getConnectorPosition(m, conn, yOffset);
            auto* cd = conn.getDescriptor();

            auto connColour = getSignalColour(cd->signalType);

            g.setColour(connColour);
            g.fillEllipse(static_cast<float>(pos.x - 3), static_cast<float>(pos.y - 3), 6.0f, 6.0f);
        }
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

        g.setColour(cableCol.withAlpha(0.7f));

        // Draw a curved cable
        juce::Path path;
        path.startNewSubPath(srcPos.toFloat());

        float midY = (srcPos.y + dstPos.y) * 0.5f;
        float sag = std::abs(static_cast<float>(srcPos.x - dstPos.x)) * 0.15f + 15.0f;

        path.cubicTo(static_cast<float>(srcPos.x), midY + sag,
                     static_cast<float>(dstPos.x), midY + sag,
                     static_cast<float>(dstPos.x), static_cast<float>(dstPos.y));

        g.strokePath(path, juce::PathStrokeType(1.5f));
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

void PatchCanvasComponent::setPatch(Patch* p, const ModuleDescriptions* md)
{
    canvas.setPatch(p, md);
}
