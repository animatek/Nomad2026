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

void StatusBar::setConnectionStatus(const juce::String& status, bool connected)
{
    isConnected = connected;
    connectionLabel.setText(status, juce::dontSendNotification);

    // Set color based on connection state
    if (connected)
        connectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xff44cc44)); // Green
    else
        connectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa)); // Gray

    repaint();
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

    // Draw LED indicator
    if (!ledBounds.isEmpty())
    {
        // Outer glow for connected state
        if (isConnected)
        {
            g.setColour(juce::Colour(0xff44cc44).withAlpha(0.3f));
            g.fillEllipse(ledBounds.expanded(2.0f));
        }

        // LED circle
        g.setColour(isConnected ? juce::Colour(0xff44cc44) : juce::Colour(0xff555555));
        g.fillEllipse(ledBounds);

        // Highlight for 3D effect
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        auto highlightBounds = ledBounds.reduced(1.0f).translated(-0.5f, -0.5f);
        g.fillEllipse(highlightBounds.removeFromTop(ledBounds.getHeight() * 0.4f));
    }
}

void StatusBar::resized()
{
    auto area = getLocalBounds().reduced(8, 0);

    // LED indicator (small circle on the left)
    auto ledSize = 10.0f;
    auto ledY = (area.getHeight() - ledSize) * 0.5f;
    ledBounds = juce::Rectangle<float>(area.getX() + 4.0f, ledY, ledSize, ledSize);

    // Connection label (more space, starts after LED)
    auto connectionArea = area.removeFromLeft(280);
    connectionArea.removeFromLeft(20);  // Space for LED
    connectionLabel.setBounds(connectionArea);

    // Right-aligned labels
    dspLabel.setBounds(area.removeFromRight(100));
    voiceLabel.setBounds(area.removeFromRight(100));
}
