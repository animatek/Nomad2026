#include "MainComponent.h"
#include "ui/MidiSettingsDialog.h"

MainComponent::MainComponent(juce::ApplicationProperties& props)
    : appProperties(props)
{
    // Load module descriptions
    auto xmlPath = juce::File::getCurrentWorkingDirectory()
                       .getChildFile("nmedit/libs/nordmodular/data/module-descriptions/modules.xml");
    if (!moduleDescs.loadFromFile(xmlPath))
    {
        // Try relative to executable
        xmlPath = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                      .getParentDirectory()
                      .getParentDirectory()
                      .getParentDirectory()
                      .getChildFile("nmedit/libs/nordmodular/data/module-descriptions/modules.xml");
        moduleDescs.loadFromFile(xmlPath);
    }

    DBG("Loaded " + juce::String(moduleDescs.getModuleCount()) + " module descriptions");

    // Menu bar
    menuBar = std::make_unique<juce::MenuBarComponent>(this);
    addAndMakeVisible(menuBar.get());

    // Main layout
    mainLayout = std::make_unique<MainLayout>(moduleDescs);
    addAndMakeVisible(mainLayout.get());

    // Wire connection manager status updates to UI
    connectionManager.setStatusCallback([this](const ConnectionManager::Status& status)
    {
        juce::MessageManager::callAsync([this, status]()
        {
            onConnectionStatusChanged(status);
        });
    });

    connectionManager.setVoiceCountCallback([this](const int voiceCounts[4])
    {
        int total = voiceCounts[0] + voiceCounts[1] + voiceCounts[2] + voiceCounts[3];
        juce::MessageManager::callAsync([this, total]()
        {
            mainLayout->getStatusBar().setVoiceCount(total);
        });
    });

    connectionManager.setPatchDataCallback([](const std::vector<uint8_t>& data)
    {
        DBG("Patch data received: " + juce::String(data.size()) + " bytes");
    });

    setSize(1280, 800);

    // Auto-connect after UI is set up
    juce::MessageManager::callAsync([this]() { attemptAutoConnect(); });
}

MainComponent::~MainComponent()
{
    menuBar.reset();
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

#if ! JUCE_MAC
    menuBar->setBounds(area.removeFromTop(24));
#endif

    mainLayout->setBounds(area);
}

juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "Device" };
}

juce::PopupMenu MainComponent::getMenuForIndex(int menuIndex, const juce::String&)
{
    juce::PopupMenu menu;

    if (menuIndex == 0) // File
    {
        menu.addItem(1, "New Patch");
        menu.addItem(2, "Open...");
        menu.addItem(3, "Save");
        menu.addItem(4, "Save As...");
        menu.addSeparator();
        menu.addItem(10, "Quit");
    }
    else if (menuIndex == 1) // Edit
    {
        menu.addItem(20, "Undo", false);
        menu.addItem(21, "Redo", false);
    }
    else if (menuIndex == 2) // Device
    {
        menu.addItem(30, "MIDI Settings...");
        menu.addSeparator();
        bool connected = connectionManager.isConnected();
        menu.addItem(31, "Request Patch from Synth", connected);
        menu.addItem(32, "Send Patch to Synth", connected);
    }

    return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int)
{
    switch (menuItemID)
    {
        case 10:
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            break;
        case 30:
            showMidiSettingsDialog();
            break;
        case 31:
            connectionManager.requestPatch(0);  // Request from slot 0
            break;
        default:
            break;
    }
}

void MainComponent::showMidiSettingsDialog()
{
    MidiSettingsDialog::show(
        this,
        lastInputId,
        lastOutputId,
        connectionManager.getStatus(),
        [this](const juce::String& inputId, const juce::String& outputId)
        {
            handleConnectionRequest(inputId, outputId);
        },
        [this]()
        {
            handleDisconnectionRequest();
        });
}

void MainComponent::handleConnectionRequest(const juce::String& inputId, const juce::String& outputId)
{
    lastInputId = inputId;
    lastOutputId = outputId;
    connectionManager.connect(inputId, outputId);
}

void MainComponent::handleDisconnectionRequest()
{
    connectionManager.disconnect();
}

void MainComponent::onConnectionStatusChanged(const ConnectionManager::Status& status)
{
    mainLayout->getStatusBar().setConnectionStatus(status.message);

    // Save settings on successful connection
    if (status.state == ConnectionManager::State::Connected)
        saveMidiSettings(lastInputId, lastOutputId);
}

void MainComponent::attemptAutoConnect()
{
    auto* settings = appProperties.getUserSettings();
    if (settings == nullptr)
        return;

    auto inputId = settings->getValue("midiInputDevice", "");
    auto outputId = settings->getValue("midiOutputDevice", "");

    if (inputId.isEmpty() || outputId.isEmpty())
        return;

    // Verify the saved ports still exist
    auto inputs = ConnectionManager::getAvailableInputDevices();
    auto outputs = ConnectionManager::getAvailableOutputDevices();

    bool inputFound = false;
    bool outputFound = false;

    for (auto& dev : inputs)
        if (dev.identifier == inputId) { inputFound = true; break; }

    for (auto& dev : outputs)
        if (dev.identifier == outputId) { outputFound = true; break; }

    if (inputFound && outputFound)
    {
        DBG("Auto-connecting to saved MIDI ports");
        lastInputId = inputId;
        lastOutputId = outputId;
        connectionManager.connect(inputId, outputId);
    }
    else
    {
        DBG("Saved MIDI ports no longer available");
    }
}

void MainComponent::saveMidiSettings(const juce::String& inputId, const juce::String& outputId)
{
    auto* settings = appProperties.getUserSettings();
    if (settings == nullptr)
        return;

    settings->setValue("midiInputDevice", inputId);
    settings->setValue("midiOutputDevice", outputId);
    settings->saveIfNeeded();
}
