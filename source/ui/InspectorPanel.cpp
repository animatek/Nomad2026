#include "InspectorPanel.h"

InspectorPanel::InspectorPanel()
{
    placeholderLabel.setText("No module selected", juce::dontSendNotification);
    placeholderLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    placeholderLabel.setJustificationType(juce::Justification::centred);
    placeholderLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    addAndMakeVisible(placeholderLabel);
}

void InspectorPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e3a));
}

void InspectorPanel::resized()
{
    placeholderLabel.setBounds(getLocalBounds());
}
