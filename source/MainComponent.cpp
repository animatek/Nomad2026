#include "MainComponent.h"

MainComponent::MainComponent()
{
    setSize(1024, 768);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    g.setFont(juce::Font(juce::FontOptions(36.0f)));
    g.setColour(juce::Colours::white);
    g.drawText("Nomad2026", getLocalBounds().removeFromTop(getHeight() / 2),
               juce::Justification::centredBottom, true);

    g.setFont(juce::Font(juce::FontOptions(18.0f)));
    g.setColour(juce::Colour(0xffaaaaaa));
    g.drawText("Nord Modular G1 Editor", getLocalBounds().removeFromBottom(getHeight() / 2),
               juce::Justification::centredTop, true);
}

void MainComponent::resized()
{
}
