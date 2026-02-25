#include "MidiSettingsDialog.h"

MidiSettingsDialog::MidiSettingsDialog()
{
    inputLabel.setText("MIDI Input:", juce::dontSendNotification);
    outputLabel.setText("MIDI Output:", juce::dontSendNotification);
    statusLabel.setText("Disconnected", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));

    addAndMakeVisible(inputLabel);
    addAndMakeVisible(inputCombo);
    addAndMakeVisible(outputLabel);
    addAndMakeVisible(outputCombo);
    addAndMakeVisible(connectButton);
    addAndMakeVisible(statusLabel);

    connectButton.onClick = [this]()
    {
        if (connected)
        {
            if (onDisconnectionRequest)
                onDisconnectionRequest();
        }
        else
        {
            auto inputIdx = inputCombo.getSelectedItemIndex();
            auto outputIdx = outputCombo.getSelectedItemIndex();

            if (inputIdx >= 0 && outputIdx >= 0)
            {
                if (onConnectionRequest)
                    onConnectionRequest(inputIds[inputIdx], outputIds[outputIdx]);
            }
        }
    };

    refreshDeviceLists();
    setSize(400, 200);
}

void MidiSettingsDialog::refreshDeviceLists()
{
    inputCombo.clear();
    outputCombo.clear();
    inputIds.clear();
    outputIds.clear();

    auto inputs = ConnectionManager::getAvailableInputDevices();
    for (int i = 0; i < inputs.size(); ++i)
    {
        inputCombo.addItem(inputs[i].name, i + 1);
        inputIds.add(inputs[i].identifier);
    }

    auto outputs = ConnectionManager::getAvailableOutputDevices();
    for (int i = 0; i < outputs.size(); ++i)
    {
        outputCombo.addItem(outputs[i].name, i + 1);
        outputIds.add(outputs[i].identifier);
    }
}

void MidiSettingsDialog::setSelectedPorts(const juce::String& inputId, const juce::String& outputId)
{
    auto inputIdx = inputIds.indexOf(inputId);
    if (inputIdx >= 0)
        inputCombo.setSelectedItemIndex(inputIdx, juce::dontSendNotification);

    auto outputIdx = outputIds.indexOf(outputId);
    if (outputIdx >= 0)
        outputCombo.setSelectedItemIndex(outputIdx, juce::dontSendNotification);
}

void MidiSettingsDialog::setConnectedState(const ConnectionManager::Status& status)
{
    connected = (status.state == ConnectionManager::State::Connected);

    statusLabel.setText(status.message, juce::dontSendNotification);

    if (status.state == ConnectionManager::State::Connected)
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff44cc44));
    else if (status.state == ConnectionManager::State::Connecting)
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccc44));
    else
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));

    updateButtonState();
}

void MidiSettingsDialog::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto row = area.removeFromTop(28);
    inputLabel.setBounds(row.removeFromLeft(100));
    inputCombo.setBounds(row);

    area.removeFromTop(8);
    row = area.removeFromTop(28);
    outputLabel.setBounds(row.removeFromLeft(100));
    outputCombo.setBounds(row);

    area.removeFromTop(16);
    row = area.removeFromTop(32);
    connectButton.setBounds(row.removeFromLeft(120));
    row.removeFromLeft(16);
    statusLabel.setBounds(row);
}

void MidiSettingsDialog::updateButtonState()
{
    connectButton.setButtonText(connected ? "Disconnect" : "Connect");
}

void MidiSettingsDialog::show(juce::Component* /*parent*/,
                               const juce::String& currentInputId,
                               const juce::String& currentOutputId,
                               const ConnectionManager::Status& status,
                               std::function<void(const juce::String&, const juce::String&)> connectCb,
                               std::function<void()> disconnectCb)
{
    auto* dialog = new MidiSettingsDialog();
    dialog->setSelectedPorts(currentInputId, currentOutputId);
    dialog->setConnectedState(status);
    dialog->onConnectionRequest = std::move(connectCb);
    dialog->onDisconnectionRequest = std::move(disconnectCb);

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(dialog);
    opts.dialogTitle = "MIDI Settings";
    opts.dialogBackgroundColour = juce::Colour(0xff2a2a3e);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.launchAsync();
}
