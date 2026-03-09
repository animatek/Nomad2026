#include "MainComponent.h"
#include "model/PatchParser.h"
#include "model/PchFileIO.h"
#include "ui/MidiSettingsDialog.h"
#include "protocol/StorePatchMessage.h"
#include <iostream>
#include <iomanip>

MainComponent::MainComponent(juce::ApplicationProperties &props)
    : appProperties(props) {
  // Helper: find a data file by probing multiple locations regardless of CWD.
  // Searches CWD, next to the executable, and up to 5 parent dirs of the
  // executable.
  auto findDataFile = [](const juce::String &relativePath) -> juce::File {
    // 1. Relative to CWD (works when launched from terminal in project root)
    auto f =
        juce::File::getCurrentWorkingDirectory().getChildFile(relativePath);
    if (f.existsAsFile())
      return f;

    // 2. Relative to the executable, going up 1..5 parent directories
    auto exeDir =
        juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory();
    for (int i = 0; i < 5; ++i) {
      f = exeDir.getChildFile(relativePath);
      if (f.existsAsFile())
        return f;
      exeDir = exeDir.getParentDirectory();
    }

    return {}; // not found
  };

  // Load module descriptions
  auto xmlPath = findDataFile(
      "nmedit/libs/nordmodular/data/module-descriptions/modules.xml");
  if (xmlPath.existsAsFile())
    moduleDescs.loadFromFile(xmlPath);
  else
    DBG("WARNING: modules.xml not found!");

  DBG("Loaded " + juce::String(moduleDescs.getModuleCount()) +
      " module descriptions from: " + xmlPath.getFullPathName());

  // Load classic theme
  auto themePath = findDataFile(
      "nmedit/libs/nordmodular/data/classic-theme/classic-theme.xml");
  if (themePath.existsAsFile())
    themeData.loadFromFile(themePath);
  else
    DBG("WARNING: classic-theme.xml not found!");

  DBG("Loaded " + juce::String(themeData.getModuleThemeCount()) +
      " module themes from: " + themePath.getFullPathName());

  // Menu bar
  menuBar = std::make_unique<juce::MenuBarComponent>(this);
  addAndMakeVisible(menuBar.get());

  // Main layout
  mainLayout = std::make_unique<MainLayout>(moduleDescs);
  addAndMakeVisible(mainLayout.get());

  // Wire connection manager status updates to UI
  connectionManager.setStatusCallback(
      [this](const ConnectionManager::Status &status) {
        juce::MessageManager::callAsync(
            [this, status]() { onConnectionStatusChanged(status); });
      });

  connectionManager.setVoiceCountCallback([this](const int voiceCounts[4]) {
    int total =
        voiceCounts[0] + voiceCounts[1] + voiceCounts[2] + voiceCounts[3];
    juce::MessageManager::callAsync(
        [this, total]() { mainLayout->getStatusBar().setVoiceCount(total); });
  });

  // Wire patch list updates to inspector panel
  connectionManager.setPatchListCallback([this](const std::vector<std::string>& names) {
    juce::MessageManager::callAsync([this, names]() {
      mainLayout->getInspector().setPatchList(names);
    });
  });

  // Wire patch browser callbacks
  mainLayout->getInspector().onPatchDoubleClicked = [this](int section, int position) {
    std::cout << "[MAIN] Loading patch from browser: section=" << section << " pos=" << position << std::endl;
    connectionManager.loadPatchFromBank(section, position);
  };

  mainLayout->getInspector().onRefreshRequested = [this]() {
    std::cout << "[MAIN] Refresh requested" << std::endl;
    mainLayout->getInspector().setLoadingState(true);
    connectionManager.requestPatchList();
  };

  connectionManager.setPatchDataCallback(
      [this](const std::vector<std::vector<uint8_t>> &sections) {
        DBG("Patch data received: " + juce::String(sections.size()) +
            " sections — parsing...");

        PatchParser parser(moduleDescs);
        auto patch = parser.parse(sections);

        juce::MessageManager::callAsync([this, p = std::move(patch)]() mutable {
          // CRITICAL: Destroy synchronizer BEFORE replacing patch to avoid dangling reference
          patchSynchronizer.reset();

          currentPatch = std::move(p);
          if (currentPatch) {
            mainLayout->getCanvas().setPatch(currentPatch.get(), &moduleDescs,
                                             &themeData);
            mainLayout->getHeaderBar().setPatch(currentPatch.get());
            mainLayout->getStatusBar().setConnectionStatus(
                "Connected - " + currentPatch->getName(), true);

            // Enable patch synchronization (live editing)
            if (connectionManager.isConnected()) {
              patchSynchronizer = std::make_unique<PatchSynchronizer>(
                  *currentPatch, connectionManager);
              std::cout << "[SYNC] Patch synchronizer enabled after patch load from synth" << std::endl;
            }
          }
        });
      });

  // Wire parameter changes from canvas to synth (user turns knob in editor)
  mainLayout->getCanvas().setParameterChangeCallback(
      [this](int section, int moduleId, int parameterId, int value) {
        connectionManager.sendParameter(section, moduleId, parameterId, value);
      });

  // Wire morph knob changes from header bar to synth
  // Morphs use section=2 (morph section), module=1 (morph module),
  // parameter=0-3
  mainLayout->getHeaderBar().setMorphChangeCallback(
      [this](int morphIndex, int value) {
        connectionManager.sendParameter(2, 1, morphIndex, value);
      });

  // Wire cable visibility toggles to repaint the canvas
  mainLayout->getHeaderBar().setCableVisibilityCallback(
      [this]() { mainLayout->getCanvas().repaintCanvas(); });

  // Wire parameter changes from synth to editor (user turns knob on hardware)
  connectionManager.setParameterChangeCallback([this](int section, int moduleId,
                                                      int parameterId,
                                                      int value) {
    juce::MessageManager::callAsync([this, section, moduleId, parameterId,
                                     value]() {
      if (currentPatch == nullptr)
        return;

      // Morph section (section=2, module=1, parameter=0-3)
      if (section == 2 && moduleId == 1 && parameterId >= 0 &&
          parameterId < 4) {
        currentPatch->morphValues[static_cast<size_t>(parameterId)] = value;
        mainLayout->getHeaderBar().repaint();
        return;
      }

      // Skip if the user is currently dragging this exact parameter (avoid
      // fighting the user)
      if (mainLayout->getCanvas().isDragging(section, moduleId, parameterId))
        return;

      auto &container = currentPatch->getContainer(section);
      auto *module = container.getModuleByIndex(moduleId);
      if (module == nullptr)
        return;

      auto *param = module->getParameter(parameterId);
      if (param == nullptr)
        return;

      // Only update + repaint if value actually changed (prevents unnecessary
      // repaints)
      if (param->getValue() != value) {
        param->setValue(value);
        mainLayout->getCanvas().repaintCanvas();
      }
    });
  });

  setSize(1280, 800);

  // Auto-connect after UI is set up (with delay to let ALSA enumerate devices)
  juce::Timer::callAfterDelay(500, [this]() { attemptAutoConnect(); });
}

MainComponent::~MainComponent() { menuBar.reset(); }

void MainComponent::resized() {
  auto area = getLocalBounds();

#if !JUCE_MAC
  menuBar->setBounds(area.removeFromTop(24));
#endif

  mainLayout->setBounds(area);
}

juce::StringArray MainComponent::getMenuBarNames() {
  return {"File", "Edit", "Device"};
}

juce::PopupMenu MainComponent::getMenuForIndex(int menuIndex,
                                               const juce::String &) {
  juce::PopupMenu menu;

  if (menuIndex == 0) // File
  {
    menu.addItem(1, "New Patch");
    menu.addItem(2, "Open...");
    menu.addItem(3, "Save");
    menu.addItem(4, "Save As...");
    menu.addSeparator();
    menu.addItem(10, "Quit");
  } else if (menuIndex == 1) // Edit
  {
    menu.addItem(20, "Undo", false);
    menu.addItem(21, "Redo", false);
  } else if (menuIndex == 2) // Device
  {
    menu.addItem(30, "MIDI Settings...");
    menu.addSeparator();
    bool connected = connectionManager.isConnected();
    menu.addItem(31, "Request Patch from Synth", connected);
    menu.addItem(32, "Send Patch to Synth", connected);
  }

  return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int) {
  switch (menuItemID) {
  case 1:
    newPatch();
    break;
  case 2:
    openPatch();
    break;
  case 3:
    savePatch();
    break;
  case 4:
    savePatchAs();
    break;
  case 10:
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
    break;
  case 30:
    showMidiSettingsDialog();
    break;
  case 31:
    connectionManager.requestPatch(connectionManager.getCurrentSlot());
    break;
  case 32:
    savePatchToSynth();
    break;
  default:
    break;
  }
}

void MainComponent::newPatch() {
  // CRITICAL: Destroy synchronizer BEFORE replacing patch
  patchSynchronizer.reset();

  currentPatch = std::make_unique<Patch>();
  currentPatchFile = juce::File();
  mainLayout->getCanvas().setPatch(currentPatch.get(), &moduleDescs, &themeData);
  mainLayout->getHeaderBar().setPatch(currentPatch.get());
  mainLayout->getStatusBar().setConnectionStatus("New Patch", false);
}

void MainComponent::openPatch() {
  auto chooser = std::make_shared<juce::FileChooser>(
      "Open Patch", juce::File(), "*.pch");

  chooser->launchAsync(
      juce::FileBrowserComponent::openMode |
          juce::FileBrowserComponent::canSelectFiles,
      [this, chooser](const juce::FileChooser &fc) {
        auto result = fc.getResult();
        if (result.existsAsFile())
          loadPatchFromFile(result);
      });
}

void MainComponent::savePatch() {
  if (currentPatch == nullptr)
    return;

  if (currentPatchFile.existsAsFile()) {
    savePatchToFile(currentPatchFile);
  } else {
    savePatchAs();
  }
}

void MainComponent::savePatchAs() {
  if (currentPatch == nullptr)
    return;

  auto chooser = std::make_shared<juce::FileChooser>(
      "Save Patch As", juce::File(), "*.pch");

  chooser->launchAsync(
      juce::FileBrowserComponent::saveMode |
          juce::FileBrowserComponent::canSelectFiles,
      [this, chooser](const juce::FileChooser &fc) {
        auto result = fc.getResult();
        if (result != juce::File()) {
          auto file = result.hasFileExtension(".pch")
                          ? result
                          : result.withFileExtension("pch");
          if (savePatchToFile(file))
            currentPatchFile = file;
        }
      });
}

void MainComponent::loadPatchFromFile(const juce::File &file) {
  PchFileIO io(moduleDescs);
  auto patch = io.readFile(file);

  if (patch == nullptr) {
    mainLayout->getStatusBar().setConnectionStatus(
        "Failed to load: " + file.getFileName(), false);
    return;
  }

  // CRITICAL: Destroy synchronizer BEFORE replacing patch
  patchSynchronizer.reset();

  currentPatch = std::move(patch);
  currentPatchFile = file;
  mainLayout->getCanvas().setPatch(currentPatch.get(), &moduleDescs, &themeData);
  mainLayout->getHeaderBar().setPatch(currentPatch.get());
  mainLayout->getStatusBar().setConnectionStatus(
      "Loaded: " + file.getFileName(), false);

  // Enable patch synchronization if connected
  if (connectionManager.isConnected()) {
    patchSynchronizer = std::make_unique<PatchSynchronizer>(
        *currentPatch, connectionManager);
    std::cout << "[SYNC] Patch synchronizer enabled after file load" << std::endl;
  }
}

void MainComponent::savePatchToSynth() {
  if (!connectionManager.isConnected()) {
    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::WarningIcon,
        "Not Connected",
        "Please connect to the Nord Modular first.");
    return;
  }

  if (currentPatch == nullptr) {
    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::WarningIcon,
        "No Patch",
        "Please load a patch first.");
    return;
  }

  // StorePatch saves the current RAM slot to permanent flash memory.
  // Protocol: slot (0-3), section (0-8 = bankIndex), position (0-98).
  // Display locations 101-999 map to: section = (loc/100)-1, position = (loc%100)-1.
  auto* dialog = new juce::AlertWindow("Send Patch to Synth",
      "Save current patch to synth flash memory:",
      juce::MessageBoxIconType::QuestionIcon);

  // Slot ComboBox (A-D)
  dialog->addComboBox("slot", {"A (Slot 1)", "B (Slot 2)", "C (Slot 3)", "D (Slot 4)"}, "Slot:");
  dialog->getComboBoxComponent("slot")->setSelectedItemIndex(
      connectionManager.getCurrentSlot(), juce::dontSendNotification);

  // Bank location ComboBox (101-999, skipping x00)
  juce::StringArray locationItems;
  for (int bank = 1; bank <= 9; ++bank)
    for (int pos = 1; pos <= 99; ++pos)
      locationItems.add(juce::String(bank * 100 + pos));
  dialog->addComboBox("location", locationItems, "Location:");

  dialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
  dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

  dialog->enterModalState(true, juce::ModalCallbackFunction::create(
      [this, dialog](int result) {
        int slot = dialog->getComboBoxComponent("slot")->getSelectedItemIndex();
        int locationIdx = dialog->getComboBoxComponent("location")->getSelectedItemIndex();
        delete dialog;

        if (result == 0)
          return;

        // Convert flat index back to section/position
        // Index 0=101, 1=102, ..., 98=199, 99=201, etc.
        // Protocol uses bankIndex 0-8 (Java: dstLocation.getBank())
        int section = locationIdx / 99;       // Protocol section: 0-8
        int pos = locationIdx % 99;           // Protocol position: 0-98
        int location = (section + 1) * 100 + pos + 1;  // Display: 101-999

        StorePatchMessage msg(slot, section, pos);
        auto sysex = msg.toSysEx(slot);
        connectionManager.sendRawSysEx(sysex);

        std::cout << "[STORE] Sent StorePatch: slot=" << slot
            << " section=" << section << " pos=" << pos
            << " (location " << location << ")" << std::endl;

        std::cout << "[STORE]   SysEx: ";
        for (auto byte : sysex)
          std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
        std::cout << std::dec << std::endl;

        const char* slotNames[] = {"A", "B", "C", "D"};
        mainLayout->getStatusBar().setConnectionStatus(
            "Saved slot " + juce::String(slotNames[slot]) + " to " + juce::String(location), true);
      }),
      true);
}

bool MainComponent::savePatchToFile(const juce::File &file) {
  if (currentPatch == nullptr)
    return false;

  PchFileIO io(moduleDescs);
  bool ok = io.writeFile(*currentPatch, file);

  if (ok) {
    mainLayout->getStatusBar().setConnectionStatus(
        "Saved: " + file.getFileName(), false);
  } else {
    mainLayout->getStatusBar().setConnectionStatus(
        "Failed to save: " + file.getFileName(), false);
  }

  return ok;
}

void MainComponent::showMidiSettingsDialog() {
  MidiSettingsDialog::show(
      this, lastInputId, lastOutputId, connectionManager.getStatus(),
      [this](const juce::String &inputId, const juce::String &outputId) {
        handleConnectionRequest(inputId, outputId);
      },
      [this]() { handleDisconnectionRequest(); });
}

void MainComponent::handleConnectionRequest(const juce::String &inputId,
                                            const juce::String &outputId) {
  lastInputId = inputId;
  lastOutputId = outputId;
  connectionManager.connect(inputId, outputId);
}

void MainComponent::handleDisconnectionRequest() {
  connectionManager.disconnect();
}

void MainComponent::onConnectionStatusChanged(
    const ConnectionManager::Status &status) {
  bool connected = (status.state == ConnectionManager::State::Connected);
  mainLayout->getStatusBar().setConnectionStatus(status.message, connected);

  if (connected) {
    // Save settings on successful connection
    saveMidiSettings(lastInputId, lastOutputId);

    // Patch loading is triggered by SlotActivated (sc=0x09) from synth,
    // with a fallback timer in ConnectionManager if no slot message arrives.

    // Enable synchronizer if we have a patch loaded
    if (currentPatch && !patchSynchronizer) {
      patchSynchronizer = std::make_unique<PatchSynchronizer>(
          *currentPatch, connectionManager);
      std::cout << "[SYNC] Patch synchronizer enabled on connection" << std::endl;
    }

    // Request patch list from synth to populate the browser
    mainLayout->getInspector().setLoadingState(true);
    connectionManager.requestPatchList();
  } else {
    // Disable synchronizer on disconnect
    patchSynchronizer.reset();
    std::cout << "[SYNC] Patch synchronizer disabled on disconnect" << std::endl;
  }
}

void MainComponent::attemptAutoConnect() {
  auto *settings = appProperties.getUserSettings();
  if (settings == nullptr)
    return;

  auto savedInputId = settings->getValue("midiInputDevice", "");
  auto savedOutputId = settings->getValue("midiOutputDevice", "");
  auto savedInputName = settings->getValue("midiInputName", "");
  auto savedOutputName = settings->getValue("midiOutputName", "");

  if (savedInputId.isEmpty() && savedInputName.isEmpty())
    return;

  auto inputs = ConnectionManager::getAvailableInputDevices();
  auto outputs = ConnectionManager::getAvailableOutputDevices();

  DBG("Available MIDI inputs (" + juce::String(inputs.size()) + "):");
  for (auto &dev : inputs)
    DBG("  id=\"" + dev.identifier + "\" name=\"" + dev.name + "\"");
  DBG("Available MIDI outputs (" + juce::String(outputs.size()) + "):");
  for (auto &dev : outputs)
    DBG("  id=\"" + dev.identifier + "\" name=\"" + dev.name + "\"");

  // Find input: try identifier first, then fall back to name match
  juce::String resolvedInputId;
  for (auto &dev : inputs) {
    if (dev.identifier == savedInputId) {
      resolvedInputId = dev.identifier;
      break;
    }
  }
  if (resolvedInputId.isEmpty() && savedInputName.isNotEmpty()) {
    for (auto &dev : inputs)
      if (dev.name == savedInputName) {
        resolvedInputId = dev.identifier;
        break;
      }
  }

  // Find output: try identifier first, then fall back to name match
  juce::String resolvedOutputId;
  for (auto &dev : outputs) {
    if (dev.identifier == savedOutputId) {
      resolvedOutputId = dev.identifier;
      break;
    }
  }
  if (resolvedOutputId.isEmpty() && savedOutputName.isNotEmpty()) {
    for (auto &dev : outputs)
      if (dev.name == savedOutputName) {
        resolvedOutputId = dev.identifier;
        break;
      }
  }

  if (resolvedInputId.isNotEmpty() && resolvedOutputId.isNotEmpty()) {
    DBG("Auto-connecting: input=" + resolvedInputId +
        " output=" + resolvedOutputId);
    lastInputId = resolvedInputId;
    lastOutputId = resolvedOutputId;
    connectionManager.connect(resolvedInputId, resolvedOutputId);
  } else {
    // ALSA may not have enumerated devices yet — retry a few times
    if (autoConnectRetries > 0 && (inputs.isEmpty() || outputs.isEmpty())) {
      autoConnectRetries--;
      DBG("No MIDI devices found yet, retrying in 500ms (" +
          juce::String(autoConnectRetries) + " left)");
      juce::Timer::callAfterDelay(500, [this]() { attemptAutoConnect(); });
    } else {
      DBG("Saved MIDI ports not found (id=" + savedInputId + "/" +
          savedOutputId + " name=" + savedInputName + "/" + savedOutputName +
          ")");
    }
  }
}

void MainComponent::saveMidiSettings(const juce::String &inputId,
                                     const juce::String &outputId) {
  auto *settings = appProperties.getUserSettings();
  if (settings == nullptr)
    return;

  settings->setValue("midiInputDevice", inputId);
  settings->setValue("midiOutputDevice", outputId);

  // Also save device names for robust matching (ALSA identifiers can change
  // between reboots)
  for (auto &dev : ConnectionManager::getAvailableInputDevices())
    if (dev.identifier == inputId) {
      settings->setValue("midiInputName", dev.name);
      break;
    }
  for (auto &dev : ConnectionManager::getAvailableOutputDevices())
    if (dev.identifier == outputId) {
      settings->setValue("midiOutputName", dev.name);
      break;
    }

  settings->saveIfNeeded();
  DBG("Saved MIDI settings: input=" + inputId + " output=" + outputId);
}
