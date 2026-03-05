#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include "model/ModuleDescriptions.h"
#include "model/ThemeData.h"
#include "model/Patch.h"
#include "midi/ConnectionManager.h"
#include "ui/MainLayout.h"

class MainComponent : public juce::Component,
                      public juce::MenuBarModel
{
public:
    explicit MainComponent(juce::ApplicationProperties& props);
    ~MainComponent() override;

    void resized() override;

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    ModuleDescriptions& getModuleDescriptions() { return moduleDescs; }

private:
    void showMidiSettingsDialog();
    void handleConnectionRequest(const juce::String& inputId, const juce::String& outputId);
    void handleDisconnectionRequest();
    void onConnectionStatusChanged(const ConnectionManager::Status& status);
    void attemptAutoConnect();
    void saveMidiSettings(const juce::String& inputId, const juce::String& outputId);

    juce::ApplicationProperties& appProperties;
    ModuleDescriptions moduleDescs;
    ThemeData themeData;
    ConnectionManager connectionManager;
    std::unique_ptr<MainLayout> mainLayout;
    std::unique_ptr<juce::MenuBarComponent> menuBar;

    std::unique_ptr<Patch> currentPatch;

    juce::String lastInputId;
    juce::String lastOutputId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
