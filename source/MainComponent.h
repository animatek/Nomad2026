#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class MainComponent : public juce::Component
{
public:
    MainComponent();
    void paint(juce::Graphics& g) override;
    void resized() override;
};
