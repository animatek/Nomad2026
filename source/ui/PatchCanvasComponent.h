#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/Patch.h"
#include "../model/ModuleDescriptions.h"

class PatchCanvas : public juce::Component
{
public:
    PatchCanvas();

    void paint(juce::Graphics& g) override;

    void setPatch(Patch* p, const ModuleDescriptions* md);

    // Matches original Java editor: 255px per column, 15px per row
    static constexpr int gridX = 255;         // pixels per grid column (module width)
    static constexpr int gridY = 15;          // pixels per grid row (height unit)
    static constexpr int canvasWidth = 255 * 40;    // 40 columns visible
    static constexpr int canvasHeight = 15 * 256;   // 256 rows
    static constexpr int polyAreaOffsetY = 0;        // poly starts at y=0

private:
    void paintModules(juce::Graphics& g, const ModuleContainer& container, int yOffset);
    void paintCables(juce::Graphics& g, const ModuleContainer& container, int yOffset);
    juce::Rectangle<int> getModuleBounds(const Module& m, int yOffset) const;
    juce::Point<int> getConnectorPosition(const Module& m, const Connector& conn, int yOffset) const;

    Patch* patch = nullptr;
    const ModuleDescriptions* moduleDescs = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchCanvas)
};

class PatchCanvasComponent : public juce::Component
{
public:
    PatchCanvasComponent();

    void resized() override;

    void setPatch(Patch* p, const ModuleDescriptions* md);

private:
    juce::Viewport viewport;
    PatchCanvas canvas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchCanvasComponent)
};
