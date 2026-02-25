#include "PatchCanvasComponent.h"

// --- PatchCanvas (inner scrollable surface) ---

PatchCanvas::PatchCanvas()
{
    setSize(canvasWidth, canvasHeight);
}

void PatchCanvas::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff12122a));

    // Draw grid
    g.setColour(juce::Colour(0xff1a1a3a));
    auto bounds = g.getClipBounds();

    int startX = (bounds.getX() / gridSize) * gridSize;
    int startY = (bounds.getY() / gridSize) * gridSize;

    for (int x = startX; x < bounds.getRight(); x += gridSize)
        g.drawVerticalLine(x, static_cast<float>(bounds.getY()), static_cast<float>(bounds.getBottom()));

    for (int y = startY; y < bounds.getBottom(); y += gridSize)
        g.drawHorizontalLine(y, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));
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
