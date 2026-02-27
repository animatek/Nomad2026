#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class StatusBar : public juce::Component
{
public:
    StatusBar();

    void setConnectionStatus(const juce::String& status, bool connected = false);
    void setVoiceCount(int count);
    void setDspLoad(float percent);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label connectionLabel;
    juce::Label voiceLabel;
    juce::Label dspLabel;

    bool isConnected = false;
    juce::Rectangle<float> ledBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBar)
};
