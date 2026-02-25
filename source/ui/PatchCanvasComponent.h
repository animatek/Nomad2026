#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class PatchCanvas : public juce::Component
{
public:
    PatchCanvas();

    void paint(juce::Graphics& g) override;

    static constexpr int gridSize = 16;
    static constexpr int canvasWidth = 4096;
    static constexpr int canvasHeight = 4096;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchCanvas)
};

class PatchCanvasComponent : public juce::Component
{
public:
    PatchCanvasComponent();

    void resized() override;

private:
    juce::Viewport viewport;
    PatchCanvas canvas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchCanvasComponent)
};
