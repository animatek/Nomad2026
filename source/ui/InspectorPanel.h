#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class InspectorPanel : public juce::Component
{
public:
    InspectorPanel();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label placeholderLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InspectorPanel)
};
