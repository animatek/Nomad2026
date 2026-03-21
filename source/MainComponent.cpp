#include "MainComponent.h"
#include "model/PatchParser.h"
#include "model/PchFileIO.h"
#include "ui/MidiSettingsDialog.h"
#include "ui/PatchLocationDialog.h"
#include "protocol/StorePatchMessage.h"
#include "protocol/MorphAssignmentMessage.h"
#include "protocol/MorphRangeChangeMessage.h"
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

  // Wire canvas selection to inspector
  mainLayout->getCanvas().setModuleSelectedCallback(
      [this](Module* module, int section) {
        if (module)
          mainLayout->getInspector().setModule(module, section);
        else
          mainLayout->getInspector().clearModule();
      });

  // Wire inspector name changes to canvas repaint
  mainLayout->getInspector().onNameChanged = [this](int /*section*/, Module* /*module*/, const juce::String& newName) {
    std::cout << "[MAIN] Module renamed via inspector to: " << newName.toStdString() << std::endl;
    mainLayout->getCanvas().repaintCanvas();
  };

  // Wire inspector morph group remove
  mainLayout->getInspector().onMorphGroupChanged = [this](int section, Module* module,
                                                           int paramIndex, int morphGroup) {
    if (!currentPatch || !module) return;
    // Update patch morph assignments
    auto& assignments = currentPatch->morphAssignments;
    assignments.erase(
        std::remove_if(assignments.begin(), assignments.end(),
            [section, module, paramIndex](const MorphAssignment& ma) {
                return ma.section == section
                    && ma.module  == module->getContainerIndex()
                    && ma.param   == paramIndex;
            }),
        assignments.end());
    if (morphGroup >= 0)
    {
        MorphAssignment ma;
        ma.section = section; ma.module = module->getContainerIndex();
        ma.param = paramIndex; ma.morph = morphGroup; ma.range = 0;
        assignments.push_back(ma);
        int pid  = connectionManager.getCurrentPatchId();
        int slot = connectionManager.getCurrentSlot();
        MorphAssignmentMessage msg(pid, section, module->getContainerIndex(), paramIndex, morphGroup);
        connectionManager.sendRawSysEx(msg.toSysEx(slot));
        // Set range=0 on synth to match model
        MorphRangeChangeMessage rangeMsg(pid, section, module->getContainerIndex(), paramIndex, 0, 0);
        connectionManager.sendRawSysEx(rangeMsg.toSysEx(slot));
    }
    mainLayout->getCanvas().repaintCanvas();
  };

  // Wire inspector morph range change
  mainLayout->getInspector().onMorphRangeChanged = [this](int section, Module* module,
                                                           int paramIndex, int span, int dir) {
    if (!module || !currentPatch) return;
    // Keep patch model in sync
    int signedRange = (dir == 0) ? span : -span;
    int moduleId    = module->getContainerIndex();
    for (auto& ma : currentPatch->morphAssignments)
    {
        if (ma.section == section && ma.module == moduleId && ma.param == paramIndex)
        { ma.range = signedRange; break; }
    }
    int pid  = connectionManager.getCurrentPatchId();
    int slot = connectionManager.getCurrentSlot();
    MorphRangeChangeMessage msg(pid, section, moduleId, paramIndex, span, dir);
    connectionManager.sendRawSysEx(msg.toSysEx(slot));
    mainLayout->getCanvas().repaintCanvas();
  };

  // Wire patch list updates to patch browser panel
  connectionManager.setPatchListCallback([this](const std::vector<std::string>& names) {
    juce::MessageManager::callAsync([this, names]() {
      mainLayout->getPatchBrowser().setPatchList(names);
      mainLayout->getPatchBrowser().setLoadingState(false);
    });
  });

  // Wire patch browser callbacks
  mainLayout->getPatchBrowser().onPatchDoubleClicked = [this](int section, int position) {
    std::cout << "[MAIN] Loading patch from browser: section=" << section << " pos=" << position << std::endl;
    connectionManager.loadPatchFromBank(section, position);
    mainLayout->getHeaderBar().setCurrentLocation(section, position);
  };

  mainLayout->getPatchBrowser().onRefreshRequested = [this]() {
    mainLayout->getPatchBrowser().setLoadingState(true);
    connectionManager.requestPatchList();
  };

  mainLayout->getPatchBrowser().onPatchDelete = [this](int section, int position) {
    const auto& patchList = connectionManager.getPatchList();
    int index = section * 99 + position;
    juce::String patchName = (index < static_cast<int>(patchList.size()) && !patchList[index].empty())
                              ? patchList[index] : "--";
    auto* dialog = new juce::AlertWindow("Delete Patch",
        "Delete \"" + patchName + "\" from location " +
        juce::String((section + 1) * 100 + position + 1) + "?",
        juce::MessageBoxIconType::WarningIcon);
    dialog->addButton("Delete", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<MainComponent> safeThis(this);
    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [safeThis, section, position](int result) {
          if (safeThis != nullptr && result == 1)
              safeThis->connectionManager.deletePatchInBank(section, position);
        }), true);
  };

  mainLayout->getPatchBrowser().onPatchCopy = [this](int section, int position) {
    const auto& patchList = connectionManager.getPatchList();
    auto* dlg = new PatchLocationDialog(patchList, false, 0);
    juce::Component::SafePointer<MainComponent> safeThis(this);
    dlg->setCallback([safeThis, section, position](const PatchLocationDialog::Result& r) {
      if (safeThis != nullptr && r.confirmed)
        safeThis->connectionManager.copyPatchInBank(section, position, r.section, r.position);
    });
    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(dlg);
    opts.dialogTitle = "Copy Patch";
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.launchAsync();
  };

  mainLayout->getPatchBrowser().onPatchMove = [this](int section, int position) {
    const auto& patchList = connectionManager.getPatchList();
    auto* dlg = new PatchLocationDialog(patchList, false, 0);
    juce::Component::SafePointer<MainComponent> safeThis(this);
    dlg->setCallback([safeThis, section, position](const PatchLocationDialog::Result& r) {
      if (safeThis != nullptr && r.confirmed)
        safeThis->connectionManager.movePatchInBank(section, position, r.section, r.position);
    });
    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(dlg);
    opts.dialogTitle = "Move Patch";
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.launchAsync();
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
          // Clear inspector before replacing patch — its currentModule points into the old patch
          mainLayout->getInspector().clearModule();

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

  // Wire module drops from browser to patch model
  mainLayout->getCanvas().setModuleDropCallback(
      [this](int typeId, int section, int gridX, int gridY, const juce::String& name) {
        if (!currentPatch)
        {
          std::cout << "[MAIN] ERROR: Cannot add module - no current patch" << std::endl;
          return;
        }

        std::cout << "[MAIN] Module dropped: typeId=" << typeId
          << " section=" << section << " pos=(" << gridX << "," << gridY << ")"
          << " name=" << name << std::endl;

        // Create module in patch (will trigger synchronizer to send to synth)
        auto* module = currentPatch->createModule(section, typeId, gridX, gridY, name, moduleDescs);
        if (module)
        {
            // Repaint canvas to show new module
          mainLayout->getCanvas().repaintCanvas();
          std::cout << "[MAIN] Module created successfully" << std::endl;
        }
        else
        {
          std::cout << "[MAIN] ERROR: Failed to create module (type " << typeId << ")" << std::endl;
          mainLayout->getStatusBar().setConnectionStatus(
            "Failed to add module - check synth memory/limits", true);
        }
      });

  // Wire module delete from canvas context menu
  mainLayout->getCanvas().setDeleteModuleCallback(
      [this](int section, Module* module) {
        if (!currentPatch) return;
        auto& container = currentPatch->getContainer(section);
        container.removeModule(module);
        mainLayout->getCanvas().repaintCanvas();
        std::cout << "[MAIN] Module deleted from section " << section << std::endl;
      });

  // Wire module rename from canvas context menu
  mainLayout->getCanvas().setRenameModuleCallback(
      [](int /*section*/, Module* /*module*/, const juce::String& newName) {
        // Title is already updated on the module object; log for now
        // Future: send NameDump to synth when protocol supports it
        std::cout << "[MAIN] Module renamed to: " << newName.toStdString() << std::endl;
      });

  // Wire morph group assignment from parameter context menu
  mainLayout->getCanvas().setMorphAssignCallback(
      [this](int section, int moduleId, int paramId, int morphGroup) {
        if (!currentPatch) return;
        // Update patch model: add/update/remove morph assignment
        auto& assignments = currentPatch->morphAssignments;
        // Remove any existing assignment for this param
        assignments.erase(
            std::remove_if(assignments.begin(), assignments.end(),
                [section, moduleId, paramId](const MorphAssignment& ma) {
                    return ma.section == section && ma.module == moduleId && ma.param == paramId;
                }),
            assignments.end());
        if (morphGroup >= 0)
        {
            MorphAssignment ma;
            ma.section = section;
            ma.module = moduleId;
            ma.param = paramId;
            ma.morph = morphGroup;
            ma.range = 0;  // Default range: 0 (not moving with morph until explicitly set)
            assignments.push_back(ma);
            // Send only the assignment — the canvas will call morphRangeChangeCallback
            // separately with span=0, avoiding sending two messages in quick succession.
            int pid = connectionManager.getCurrentPatchId();
            int slot = connectionManager.getCurrentSlot();
            MorphAssignmentMessage msg(pid, section, moduleId, paramId, morphGroup);
            connectionManager.sendRawSysEx(msg.toSysEx(slot));
            std::cout << "[MAIN] Morph assign: section=" << section
                      << " module=" << moduleId << " param=" << paramId
                      << " group=" << morphGroup << std::endl;
        }
        else
        {
            std::cout << "[MAIN] Morph disabled for section=" << section
                      << " module=" << moduleId << " param=" << paramId << std::endl;
        }
        // Refresh inspector if the current module is the one that changed
        mainLayout->getInspector().refreshMorphList();
      });

  // Wire zero morph / Ctrl+drag (MorphRangeChange) from canvas
  mainLayout->getCanvas().setMorphRangeChangeCallback(
      [this](int section, int moduleId, int paramId, int span, int direction) {
        // Update patch model so serialization uses the correct range
        if (currentPatch)
        {
            int signedRange = (direction == 0) ? span : -span;
            for (auto& ma : currentPatch->morphAssignments)
            {
                if (ma.section == section && ma.module == moduleId && ma.param == paramId)
                { ma.range = signedRange; break; }
            }
        }
        int pid  = connectionManager.getCurrentPatchId();
        int slot = connectionManager.getCurrentSlot();
        MorphRangeChangeMessage msg(pid, section, moduleId, paramId, span, direction);
        connectionManager.sendRawSysEx(msg.toSysEx(slot));
        // Keep inspector morph list in sync with canvas changes
        mainLayout->getInspector().refreshMorphList();
      });

  // Wire morph knob changes from header bar to synth
  // Morphs use section=2 (morph section), module=1 (morph module),
  // parameter=0-3
  mainLayout->getHeaderBar().setMorphChangeCallback(
      [this](int morphIndex, int value) {
        connectionManager.sendParameter(2, 1, morphIndex, value);
      });

  // Wire patch name changes to send to synth
  mainLayout->getHeaderBar().setNameChangeCallback(
      [this](const juce::String& newName) {
        std::cout << "[MAIN] Patch name changed to: " << newName.toStdString() << std::endl;
        connectionManager.sendPatchTitle(newName);
      });

  // Wire quick save button
  mainLayout->getHeaderBar().setQuickSaveCallback(
      [this]() {
        std::cout << "[MAIN] Quick save triggered" << std::endl;
        // Get current location from header bar
        auto& headerBar = mainLayout->getHeaderBar();
        int section = headerBar.currentSection;
        int position = headerBar.currentPosition;

        if (section < 0 || position < 0)
        {
          juce::AlertWindow::showMessageBoxAsync(
              juce::MessageBoxIconType::WarningIcon,
              "Quick Save",
              "No save location set. Load a patch from the browser first.");
          return;
        }

        // Save patch to synth at the tracked location
        int displayLocation = (section + 1) * 100 + position + 1;
        int slot = connectionManager.getCurrentSlot();

        // Show "Saving..." message
        mainLayout->getStatusBar().showMessage("Quick saving to location " + juce::String(displayLocation) + "...", 0);

        StorePatchMessage msg(slot, section, position);
        auto sysex = msg.toSysEx(slot);
        connectionManager.sendRawSysEx(sysex);

        std::cout << "[MAIN] Quick saved to location " << displayLocation << std::endl;

        // Show success message (auto-hide after 3 seconds)
        mainLayout->getStatusBar().showMessage("Quick saved to location " + juce::String(displayLocation), 3000);
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

  connectionManager.setSynthErrorCallback([this](int errorCode) {
    mainLayout->getStatusBar().showMessage(
        "ERROR: Synth error code " + juce::String(errorCode)
        + " — check console for details", 8000);
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
  return {"File", "Edit", "Device", "Help", "About"};
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
  else if (menuIndex == 3) // Help
  {
    menu.addItem(40, "Nord Modular Forum", true);
    menu.addItem(41, "Nord Modular Facebook Group", true);
    menu.addItem(42, "Nord Modular Patches Archive", true);
  }
  else if (menuIndex == 4) // About
  {
    menu.addItem(50, "Support the Project (Patreon)", true);
    menu.addItem(51, "Source Code (GitHub)", true);
    menu.addItem(52, "Website", true);
  }

  return menu;
}

void MainComponent::openURL(const juce::String& url) {
  // Try JUCE's built-in method first
  if (juce::URL(url).launchInDefaultBrowser())
    return;

  // Fallback: use platform-specific commands
#if JUCE_LINUX
  juce::String command = "xdg-open \"" + url + "\" &";
#elif JUCE_MAC
  juce::String command = "open \"" + url + "\"";
#elif JUCE_WINDOWS
  juce::String command = "start \"\" \"" + url + "\"";
#else
  return;  // Unknown platform
#endif

  system(command.toRawUTF8());
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

  // Help menu
  case 40:  // Nord Modular Forum
    openURL("https://electro-music.com/forum/forum-43.html");
    break;
  case 41:  // Facebook Group
    openURL("https://www.facebook.com/groups/218654441592104");
    break;
  case 42:  // Patches Archive
    openURL("https://electro-music.com/nm_classic/");
    break;

  // About menu
  case 50:  // Patreon
    openURL("https://www.patreon.com/collection/2038913");
    break;
  case 51:  // GitHub
    openURL("https://github.com/animatek/Nomad2026/");
    break;
  case 52:  // Website
    openURL("https://animatek.net/");
    break;

  default:
    break;
  }
}

void MainComponent::newPatch() {
  // CRITICAL: Destroy synchronizer BEFORE replacing patch
  patchSynchronizer.reset();
  mainLayout->getInspector().clearModule();

  currentPatch = std::make_unique<Patch>();
  currentPatchFile = juce::File();
  mainLayout->getCanvas().setPatch(currentPatch.get(), &moduleDescs, &themeData);
  mainLayout->getHeaderBar().setPatch(currentPatch.get());
  mainLayout->getHeaderBar().clearCurrentLocation();  // Clear quick save location
  mainLayout->getStatusBar().setConnectionStatus("New Patch", false);

  // Re-enable synchronizer so live edits (add/delete modules, cables) are sent to synth
  if (connectionManager.isConnected()) {
    patchSynchronizer = std::make_unique<PatchSynchronizer>(*currentPatch, connectionManager);
    std::cout << "[SYNC] Patch synchronizer enabled for new patch" << std::endl;
  }
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
    mainLayout->getStatusBar().showMessage("ERROR:Failed to load: " + file.getFileName(), 5000);
    return;
  }

  // CRITICAL: Destroy synchronizer BEFORE replacing patch
  patchSynchronizer.reset();
  // Clear inspector before replacing patch — its currentModule points into the old patch
  mainLayout->getInspector().clearModule();

  currentPatch = std::move(patch);
  currentPatchFile = file;
  mainLayout->getCanvas().setPatch(currentPatch.get(), &moduleDescs, &themeData);
  mainLayout->getHeaderBar().setPatch(currentPatch.get());
  mainLayout->getStatusBar().showMessage("Loaded: " + file.getFileName(), 3000);

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

  int slot = connectionManager.getCurrentSlot();

  // Step 1: Upload the patch to the synth's current working slot.
  // This replaces whatever the synth has in RAM with our editor patch.
  mainLayout->getStatusBar().showMessage("Uploading patch to synth...", 0);

  // Step 2: Only show the "Store to Bank" dialog AFTER the synth ACKs the upload.
  // Sending StorePatch before the upload ACK causes synth error 6 ("upload incomplete").
  // Use SafePointer so the lambda is safe even if MainComponent is destroyed
  // before the async callback fires (e.g. window closed mid-upload).
  juce::Component::SafePointer<MainComponent> safeThis(this);
  connectionManager.setUploadCompleteCallback([safeThis, slot]() {
    if (safeThis == nullptr) return;
    // One-shot: clear the callback immediately so it doesn't fire again
    safeThis->connectionManager.setUploadCompleteCallback(nullptr);

    const char* slotNames[] = {"A", "B", "C", "D"};
    safeThis->mainLayout->getStatusBar().showMessage(
        juce::String("Patch uploaded to slot ") + slotNames[slot]
        + " — use Store to Bank to save permanently", 5000);

    // Only offer bank storage if the patch list is loaded
    if (!safeThis->connectionManager.isPatchListLoaded())
      return;

    auto* locationDialog = new PatchLocationDialog(safeThis->connectionManager.getPatchList(),
                                                    true,  // Show slot selector
                                                    slot);

    locationDialog->setCallback([safeThis](const PatchLocationDialog::Result& result) {
      if (safeThis == nullptr || !result.confirmed)
        return;

      int location = (result.section + 1) * 100 + result.position + 1;

      safeThis->mainLayout->getStatusBar().showMessage("Storing to bank location " + juce::String(location) + "...", 0);

      StorePatchMessage msg(result.slot, result.section, result.position);
      auto sysex = msg.toSysEx(result.slot);
      safeThis->connectionManager.sendRawSysEx(sysex);

      std::cout << "[STORE] Sent StorePatch: slot=" << result.slot
          << " section=" << result.section << " pos=" << result.position
          << " (location " << location << ")" << std::endl;

      const char* storeSlotNames[] = {"A", "B", "C", "D"};
      safeThis->mainLayout->getStatusBar().showMessage(
          "Stored slot " + juce::String(storeSlotNames[result.slot]) + " to bank location " + juce::String(location), 3000);
    });

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(locationDialog);
    options.dialogTitle = "Store Patch to Bank";
    options.dialogBackgroundColour = safeThis->getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
  });

  connectionManager.uploadPatch(slot, *currentPatch);
}

bool MainComponent::savePatchToFile(const juce::File &file) {
  if (currentPatch == nullptr)
    return false;

  PchFileIO io(moduleDescs);
  bool ok = io.writeFile(*currentPatch, file);

  if (ok) {
    mainLayout->getStatusBar().showMessage("Saved: " + file.getFileName(), 3000);
  } else {
    mainLayout->getStatusBar().showMessage("ERROR:Failed to save: " + file.getFileName(), 5000);
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
