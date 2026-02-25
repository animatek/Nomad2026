#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../midi/ConnectionManager.h"

class MidiSettingsDialog : public juce::Component
{
public:
    MidiSettingsDialog();

    void refreshDeviceLists();
    void setSelectedPorts(const juce::String& inputId, const juce::String& outputId);
    void setConnectedState(const ConnectionManager::Status& status);

    // Callbacks
    std::function<void(const juce::String& inputId, const juce::String& outputId)> onConnectionRequest;
    std::function<void()> onDisconnectionRequest;

    void resized() override;

    static void show(juce::Component* parent,
                     const juce::String& currentInputId,
                     const juce::String& currentOutputId,
                     const ConnectionManager::Status& status,
                     std::function<void(const juce::String&, const juce::String&)> connectCb,
                     std::function<void()> disconnectCb);

private:
    juce::Label inputLabel;
    juce::ComboBox inputCombo;
    juce::Label outputLabel;
    juce::ComboBox outputCombo;
    juce::TextButton connectButton { "Connect" };
    juce::Label statusLabel;

    // Store device identifiers parallel to combo box items
    juce::StringArray inputIds;
    juce::StringArray outputIds;

    bool connected = false;

    void updateButtonState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiSettingsDialog)
};
