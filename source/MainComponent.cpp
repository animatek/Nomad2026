#include "MainComponent.h"
#include "model/PatchParser.h"
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

    // Load classic theme
    auto themePath = juce::File::getCurrentWorkingDirectory()
                         .getChildFile("nmedit/libs/nordmodular/data/classic-theme/classic-theme.xml");
    if (!themeData.loadFromFile(themePath))
    {
        themePath = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                        .getParentDirectory()
                        .getParentDirectory()
                        .getParentDirectory()
                        .getChildFile("nmedit/libs/nordmodular/data/classic-theme/classic-theme.xml");
        themeData.loadFromFile(themePath);
    }

    DBG("Loaded " + juce::String(themeData.getModuleThemeCount()) + " module themes");

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

    connectionManager.setPatchDataCallback([this](const std::vector<std::vector<uint8_t>>& sections)
    {
        DBG("Patch data received: " + juce::String(sections.size()) + " sections — parsing...");

        PatchParser parser(moduleDescs);
        auto patch = parser.parse(sections);

        juce::MessageManager::callAsync([this, p = std::move(patch)]() mutable
        {
            currentPatch = std::move(p);
            if (currentPatch)
            {
                mainLayout->getCanvas().setPatch(currentPatch.get(), &moduleDescs, &themeData);
                mainLayout->getHeaderBar().setPatch(currentPatch.get());
                mainLayout->getStatusBar().setConnectionStatus(
                    "Connected - " + currentPatch->getName(), true);
            }
        });
    });

    // Wire parameter changes from canvas to synth (user turns knob in editor)
    mainLayout->getCanvas().setParameterChangeCallback([this](int section, int moduleId, int parameterId, int value)
    {
        connectionManager.sendParameter(section, moduleId, parameterId, value);
    });

    // Wire morph knob changes from header bar to synth
    // Morphs use section=2 (morph section), module=1 (morph module), parameter=0-3
    mainLayout->getHeaderBar().setMorphChangeCallback([this](int morphIndex, int value)
    {
        connectionManager.sendParameter(2, 1, morphIndex, value);
    });

    // Wire cable visibility toggles to repaint the canvas
    mainLayout->getHeaderBar().setCableVisibilityCallback([this]()
    {
        mainLayout->getCanvas().repaintCanvas();
    });

    // Wire parameter changes from synth to editor (user turns knob on hardware)
    connectionManager.setParameterChangeCallback([this](int section, int moduleId, int parameterId, int value)
    {
        juce::MessageManager::callAsync([this, section, moduleId, parameterId, value]()
        {
            if (currentPatch == nullptr)
                return;

            // Morph section (section=2, module=1, parameter=0-3)
            if (section == 2 && moduleId == 1 && parameterId >= 0 && parameterId < 4)
            {
                currentPatch->morphValues[static_cast<size_t>(parameterId)] = value;
                mainLayout->getHeaderBar().repaint();
                return;
            }

            // Skip if the user is currently dragging this exact parameter (avoid fighting the user)
            if (mainLayout->getCanvas().isDragging(section, moduleId, parameterId))
                return;

            auto& container = currentPatch->getContainer(section);
            auto* module = container.getModuleByIndex(moduleId);
            if (module == nullptr)
                return;

            auto* param = module->getParameter(parameterId);
            if (param == nullptr)
                return;

            // Only update + repaint if value actually changed (prevents unnecessary repaints)
            if (param->getValue() != value)
            {
                param->setValue(value);
                mainLayout->getCanvas().repaintCanvas();
            }
        });
    });

    setSize(1280, 800);

    // Auto-connect after UI is set up (with delay to let ALSA enumerate devices)
    juce::Timer::callAfterDelay(500, [this]() { attemptAutoConnect(); });
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
    bool connected = (status.state == ConnectionManager::State::Connected);
    mainLayout->getStatusBar().setConnectionStatus(status.message, connected);

    if (connected)
    {
        // Save settings on successful connection
        saveMidiSettings(lastInputId, lastOutputId);

        // Auto-request patch from slot 0 once connected
        connectionManager.requestPatch(0);
    }
}

void MainComponent::attemptAutoConnect()
{
    auto* settings = appProperties.getUserSettings();
    if (settings == nullptr)
        return;

    auto savedInputId   = settings->getValue("midiInputDevice", "");
    auto savedOutputId  = settings->getValue("midiOutputDevice", "");
    auto savedInputName  = settings->getValue("midiInputName", "");
    auto savedOutputName = settings->getValue("midiOutputName", "");

    if (savedInputId.isEmpty() && savedInputName.isEmpty())
        return;

    auto inputs = ConnectionManager::getAvailableInputDevices();
    auto outputs = ConnectionManager::getAvailableOutputDevices();

    DBG("Available MIDI inputs (" + juce::String(inputs.size()) + "):");
    for (auto& dev : inputs)
        DBG("  id=\"" + dev.identifier + "\" name=\"" + dev.name + "\"");
    DBG("Available MIDI outputs (" + juce::String(outputs.size()) + "):");
    for (auto& dev : outputs)
        DBG("  id=\"" + dev.identifier + "\" name=\"" + dev.name + "\"");

    // Find input: try identifier first, then fall back to name match
    juce::String resolvedInputId;
    for (auto& dev : inputs)
    {
        if (dev.identifier == savedInputId) { resolvedInputId = dev.identifier; break; }
    }
    if (resolvedInputId.isEmpty() && savedInputName.isNotEmpty())
    {
        for (auto& dev : inputs)
            if (dev.name == savedInputName) { resolvedInputId = dev.identifier; break; }
    }

    // Find output: try identifier first, then fall back to name match
    juce::String resolvedOutputId;
    for (auto& dev : outputs)
    {
        if (dev.identifier == savedOutputId) { resolvedOutputId = dev.identifier; break; }
    }
    if (resolvedOutputId.isEmpty() && savedOutputName.isNotEmpty())
    {
        for (auto& dev : outputs)
            if (dev.name == savedOutputName) { resolvedOutputId = dev.identifier; break; }
    }

    if (resolvedInputId.isNotEmpty() && resolvedOutputId.isNotEmpty())
    {
        DBG("Auto-connecting: input=" + resolvedInputId + " output=" + resolvedOutputId);
        lastInputId = resolvedInputId;
        lastOutputId = resolvedOutputId;
        connectionManager.connect(resolvedInputId, resolvedOutputId);
    }
    else
    {
        // ALSA may not have enumerated devices yet — retry a few times
        if (autoConnectRetries > 0 && (inputs.isEmpty() || outputs.isEmpty()))
        {
            autoConnectRetries--;
            DBG("No MIDI devices found yet, retrying in 500ms (" + juce::String(autoConnectRetries) + " left)");
            juce::Timer::callAfterDelay(500, [this]() { attemptAutoConnect(); });
        }
        else
        {
            DBG("Saved MIDI ports not found (id=" + savedInputId + "/" + savedOutputId
                + " name=" + savedInputName + "/" + savedOutputName + ")");
        }
    }
}

void MainComponent::saveMidiSettings(const juce::String& inputId, const juce::String& outputId)
{
    auto* settings = appProperties.getUserSettings();
    if (settings == nullptr)
        return;

    settings->setValue("midiInputDevice", inputId);
    settings->setValue("midiOutputDevice", outputId);

    // Also save device names for robust matching (ALSA identifiers can change between reboots)
    for (auto& dev : ConnectionManager::getAvailableInputDevices())
        if (dev.identifier == inputId) { settings->setValue("midiInputName", dev.name); break; }
    for (auto& dev : ConnectionManager::getAvailableOutputDevices())
        if (dev.identifier == outputId) { settings->setValue("midiOutputName", dev.name); break; }

    settings->saveIfNeeded();
    DBG("Saved MIDI settings: input=" + inputId + " output=" + outputId);
}
