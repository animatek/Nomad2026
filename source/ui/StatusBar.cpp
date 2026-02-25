#include "StatusBar.h"

StatusBar::StatusBar()
{
    auto setupLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        label.setFont(juce::Font(juce::FontOptions(12.0f)));
        addAndMakeVisible(label);
    };

    setupLabel(connectionLabel, "Disconnected");
    setupLabel(voiceLabel, "Voices: 0");
    setupLabel(dspLabel, "DSP: 0%");
}

void StatusBar::setConnectionStatus(const juce::String& status)
{
    connectionLabel.setText(status, juce::dontSendNotification);
}

void StatusBar::setVoiceCount(int count)
{
    voiceLabel.setText("Voices: " + juce::String(count), juce::dontSendNotification);
}

void StatusBar::setDspLoad(float percent)
{
    dspLabel.setText("DSP: " + juce::String(percent, 1) + "%", juce::dontSendNotification);
}

void StatusBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));
    g.setColour(juce::Colour(0xff333355));
    g.drawLine(0.0f, 0.0f, static_cast<float>(getWidth()), 0.0f, 1.0f);
}

void StatusBar::resized()
{
    auto area = getLocalBounds().reduced(4, 0);
    connectionLabel.setBounds(area.removeFromLeft(200));
    dspLabel.setBounds(area.removeFromRight(100));
    voiceLabel.setBounds(area.removeFromRight(100));
}
