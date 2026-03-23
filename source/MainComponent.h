#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include "model/ModuleDescriptions.h"
#include "model/ThemeData.h"
#include "model/Patch.h"
#include "model/PchFileIO.h"
#include "midi/ConnectionManager.h"
#include "sync/PatchSynchronizer.h"
#include "undo/PatchActions.h"
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
    void newPatch();
    void openPatch();
    void savePatch();
    void savePatchAs();
    void uploadToActiveSlot();
    void storePatchToBank();
    void loadPatchFromFile(const juce::File& file);
    bool savePatchToFile(const juce::File& file);

    void showMidiSettingsDialog();
    void handleConnectionRequest(const juce::String& inputId, const juce::String& outputId);
    void handleDisconnectionRequest();
    void onConnectionStatusChanged(const ConnectionManager::Status& status);
    void attemptAutoConnect();
    void saveMidiSettings(const juce::String& inputId, const juce::String& outputId);
    void openURL(const juce::String& url);

    juce::ApplicationProperties& appProperties;
    ModuleDescriptions moduleDescs;
    ThemeData themeData;
    ConnectionManager connectionManager;
    std::unique_ptr<MainLayout> mainLayout;
    std::unique_ptr<juce::MenuBarComponent> menuBar;

    std::unique_ptr<Patch> currentPatch;
    juce::File currentPatchFile;
    std::unique_ptr<PatchSynchronizer> patchSynchronizer;

    juce::UndoManager undoManager;
    std::unique_ptr<UndoContext> undoContext;
    void rebuildUndoContext();  // call after patch change

    juce::String lastInputId;
    juce::String lastOutputId;
    int autoConnectRetries = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
