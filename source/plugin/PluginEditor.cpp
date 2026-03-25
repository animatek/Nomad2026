#include "PluginEditor.h"

NomadPluginEditor::NomadPluginEditor(NomadPluginProcessor& processor)
    : AudioProcessorEditor(processor),
      nomadProcessor(processor)
{
    mainComponent = std::make_unique<MainComponent>(nomadProcessor.getAppProperties());
    addAndMakeVisible(mainComponent.get());

    // Default plugin window size
    setSize(1280, 800);
    setResizable(true, true);
}

void NomadPluginEditor::resized()
{
    mainComponent->setBounds(getLocalBounds());
}
