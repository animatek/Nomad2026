#include "ConnectionManager.h"
#include "../protocol/StorePatchMessage.h"
#include "../protocol/SetPatchTitleMessage.h"
#include "../model/PatchSerializer.h"
#include "../model/Patch.h"
#include <iostream>
#include <iomanip>

ConnectionManager::ConnectionManager()
{
    protocol.addListener(this);
}

ConnectionManager::~ConnectionManager()
{
    *alive = false;   // Cancel any pending Timer::callAfterDelay lambdas
    protocol.stopTimer();
    protocol.removeListener(this);
    disconnect();
}

bool ConnectionManager::connect(const juce::String& inputId, const juce::String& outputId)
{
    disconnect();

    midiDevice = std::make_unique<MidiDeviceManager>(protocol);

    if (!midiDevice->connect(inputId, outputId))
    {
        midiDevice.reset();
        setStatus(State::Disconnected, "Failed to open MIDI ports");
        return false;
    }

    setStatus(State::Connecting, "Connecting...");

    // Send IAm handshake (sender=0 means PC, version 3.3)
    // IAm has no checksum per the PDL2 spec
    IAmMessage iam;
    iam.sender = 0;
    iam.versionHigh = 3;
    iam.versionLow = 3;
    auto payload = iam.encode();
    protocol.sendMessage(NmCmd::IAm, 0, payload, /*expectsReply=*/true, /*addChecksum=*/false);

    startHandshakeTimeout();
    return true;
}

void ConnectionManager::disconnect()
{
    cancelHandshakeTimeout();
    collectingSections = false;
    slotDetected = false;
    slotDetectGeneration++;

    if (midiDevice)
    {
        midiDevice->disconnect();
        midiDevice.reset();
    }

    setStatus(State::Disconnected, "Disconnected");
}

juce::Array<juce::MidiDeviceInfo> ConnectionManager::getAvailableInputDevices()
{
    return MidiDeviceManager::getAvailableInputDevices();
}

juce::Array<juce::MidiDeviceInfo> ConnectionManager::getAvailableOutputDevices()
{
    return MidiDeviceManager::getAvailableOutputDevices();
}

void ConnectionManager::onIAmReceived(const IAmMessage& msg)
{
    // sender=1 means the synth is responding
    if (msg.sender == 1)
    {
        cancelHandshakeTimeout();

        status.synthVersionHigh = msg.versionHigh;
        status.synthVersionLow = msg.versionLow;

        setStatus(State::Connected,
                  "Connected: Nord Modular v" +
                  juce::String(msg.versionHigh) + "." +
                  juce::String(msg.versionLow));

        // Start fallback timer: if synth doesn't send SlotActivated within 3s,
        // default to slot 0
        startSlotDetectionFallback();
    }
}

void ConnectionManager::onParameterChanged(const ParameterChangeMessage& msg)
{
    // Synth notifies us of a parameter change (user turned a knob on the hardware)
    if (parameterChangeCallback)
        parameterChangeCallback(msg.section, msg.module, msg.parameter, msg.value);
}

void ConnectionManager::requestPatch(int slot)
{
    if (!isConnected())
        return;

    // Cancel any pending edit queue — we're about to reload from synth
    if (!ackedQueue.empty() || ackedQueueWaiting)
    {
        ackedQueue.clear();
        ackedQueueWaiting = false;
        ++ackedQueueGeneration;
    }

    // Reset any in-progress request (new request supersedes old one)
    waitingForPatchAck = true;
    collectingSections = false;
    pendingPatchSlot = slot;
    patchPacketsReceived = 0;
    sectionAccumulator.clear();
    patchSections.clear();
    sectionsReceived = 0;
    patchTimeoutGeneration++;  // Invalidate any pending timeout

    RequestPatchMessage req;
    req.slot = slot;
    auto payload = req.encode();
    protocol.sendMessage(NmCmd::PatchHandling, slot, payload, /*expectsReply=*/true, /*addChecksum=*/true);

    DBG("Requesting patch from slot " + juce::String(slot));
}

int ConnectionManager::getCurrentSlot() const
{
    return currentSlot;
}

void ConnectionManager::selectSlot(int slot)
{
    if (!isConnected() || slot < 0 || slot > 3)
        return;

    // PDL2: ActivateSlot := 0:1 slot:7
    // cc=0x17, pp=0x41, sc=0x09
    std::vector<uint8_t> payload;
    payload.push_back(0x41);  // pp = PatchManagerCommand
    payload.push_back(0x09);  // sc = ActivateSlot
    payload.push_back(static_cast<uint8_t>(slot & 0x7F));  // 0:1 slot:7

    protocol.sendMessage(NmCmd::PatchHandling, slot, payload, false, true);
    currentSlot = slot;

    std::cout << "[SLOT] Sent ActivateSlot: " << slot << std::endl;
}

void ConnectionManager::loadPatchFromBank(int section, int position, int targetSlot)
{
    if (!isConnected())
        return;

    int slot = (targetSlot >= 0) ? targetSlot : currentSlot;

    lastLoadedSection = section;
    lastLoadedPosition = position;
    suppressNextLocationClear = true;

    std::cout << "[LOAD] Loading patch from bank: section=" << section
              << " position=" << position << " to slot=" << slot << std::endl;

    LoadPatchMessage msg;
    msg.slot = slot;
    msg.section = section;
    msg.position = position;
    auto payload = msg.encode();
    protocol.sendMessage(NmCmd::PatchHandling, slot, payload, /*expectsReply=*/true, /*addChecksum=*/true);

    // After loading, request the patch data to update UI
    // Give the synth a moment to load it
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(200, [this, slot, aliveFlag]()
    {
        if (!*aliveFlag) return;
        requestPatch(slot);
    });
}

std::vector<uint8_t> ConnectionManager::buildUploadSysEx(int sectionIndex, int numSections, int slot)
{
    bool isFirst = (sectionIndex == 0);
    bool isLast  = (sectionIndex == numSections - 1);
    int  cc      = 0x1c | (isFirst ? 1 : 0) | (isLast ? 2 : 0);
    int  sectionsEnded = sectionIndex + 1;

    // payload[0]: 0:1 command:1 pid:6
    //   MSB=0, command=1 (bulk upload), pid=sectionsEnded
    uint8_t cmdPidByte = static_cast<uint8_t>(0x40 | (sectionsEnded & 0x3F));

    std::vector<uint8_t> msg;
    msg.push_back(0xF0);
    msg.push_back(0x33);
    msg.push_back(static_cast<uint8_t>(((cc & 0x1F) << 2) | (slot & 0x03)));
    msg.push_back(0x06);
    msg.push_back(cmdPidByte);
    msg.insert(msg.end(), uploadSections[static_cast<size_t>(sectionIndex)].begin(),
                          uploadSections[static_cast<size_t>(sectionIndex)].end());
    // Checksum: sum of all bytes (F0 through last payload byte) % 128
    uint32_t sum = 0;
    for (auto b : msg)
        sum += b;
    msg.push_back(static_cast<uint8_t>(sum % 128));
    msg.push_back(0xF7);
    return msg;
}

void ConnectionManager::sendNextUploadSection()
{
    int total = static_cast<int>(uploadSections.size());
    if (uploadSectionIndex >= total)
    {
        // All sections sent and ACKed — done
        std::cout << "[UPLOAD] All " << total << " sections sent and ACKed." << std::endl;
        waitingForUploadAck = false;
        // Notify MainComponent that upload is complete
        if (uploadCompleteCallback)
        {
            auto cb = uploadCompleteCallback;
            juce::MessageManager::callAsync([cb]() { cb(); });
        }
        // Suppress the next auto-fetch triggered by NewPatchInSlot (sc=0x38).
        // currentPatch is already authoritative — it IS the patch we just uploaded.
        // Re-fetching would replace it with a synth copy that may not include
        // morph assignments (the synth's working-slot memory may strip them).
        suppressNextAutoFetch = true;
        return;
    }

    auto msg = buildUploadSysEx(uploadSectionIndex, total, uploadSlot);

    // Log full SysEx for debugging
    std::cout << "[UPLOAD]   section " << uploadSectionIndex
              << "/" << total << " size=" << msg.size() << " hex:";
    for (size_t k = 0; k < msg.size(); ++k)
        std::cout << " " << std::hex << std::setw(2) << std::setfill('0') << (int)msg[k];
    std::cout << std::dec << std::endl;

    sendRawSysEx(msg);
    // waitingForUploadAck stays true — onAckReceived will call sendNextUploadSection
}

void ConnectionManager::uploadPatch(int slot, const Patch& patch)
{
    if (!isConnected())
        return;

    // Cancel any pending edit operations in the ACK queue.
    // The upload sends the complete patch state, making individual
    // DeleteModule/DeleteCable/NewModule messages redundant and potentially
    // conflicting with the upload sequence on the synth.
    if (!ackedQueue.empty() || ackedQueueWaiting)
    {
        std::cout << "[UPLOAD] Clearing edit queue (" << ackedQueue.size()
                  << " pending messages discarded)" << std::endl;
        ackedQueue.clear();
        ackedQueueWaiting = false;
        // Bump generation so any in-flight timeout lambda is invalidated
        ++ackedQueueGeneration;
    }

    // Serialize the patch into individual PDL2 sections (Java upload order, 16 sections)
    PatchSerializer serializer;
    uploadSections = serializer.serializeForUpload(patch);
    uploadSlot = slot;
    uploadSectionIndex = 0;

    std::cout << "[UPLOAD] Uploading patch \"" << patch.getName().toStdString()
              << "\" to slot " << slot << " (" << uploadSections.size() << " sections)" << std::endl;

    // Send sections one at a time, waiting for ACK between each (like Java protocol)
    waitingForUploadAck = true;
    sendNextUploadSection();
}

void ConnectionManager::sendParameter(int section, int moduleId, int parameterId, int value)
{
    if (!isConnected())
    {
        DBG("sendParameter: NOT CONNECTED");
        return;
    }

    ParameterChangeMessage msg;
    msg.pid = currentPatchId;
    msg.section = section;
    msg.module = moduleId;
    msg.parameter = parameterId;
    msg.value = value;

    auto payload = msg.encode();

    DBG("sendParameter: slot=" + juce::String(currentSlot)
        + " pid=" + juce::String(currentPatchId)
        + " section=" + juce::String(section)
        + " module=" + juce::String(moduleId)
        + " param=" + juce::String(parameterId)
        + " value=" + juce::String(value));

    // Parameter messages use cc=0x13, have checksum, no reply expected
    // IMPORTANT: Use currentSlot, not 0!
    protocol.sendMessage(NmCmd::ParameterChange, currentSlot, payload, /*expectsReply=*/false, /*addChecksum=*/true);
}

void ConnectionManager::sendPatchTitle(const juce::String& title)
{
    if (!isConnected())
    {
        DBG("sendPatchTitle: NOT CONNECTED");
        return;
    }

    SetPatchTitleMessage msg(currentSlot, currentPatchId, title);
    auto sysex = msg.toSysEx(currentSlot);
    sendRawSysEx(sysex);

    DBG("sendPatchTitle: slot=" + juce::String(currentSlot)
        + " pid=" + juce::String(currentPatchId)
        + " title=\"" + title + "\"");
}

void ConnectionManager::sendRawSysEx(const std::vector<uint8_t>& sysex)
{
    if (!isConnected() || !midiDevice)
        return;

    midiDevice->sendSysEx(sysex);
}

void ConnectionManager::sendAckedSysEx(const std::vector<uint8_t>& sysex)
{
    if (!isConnected() || !midiDevice)
        return;

    ackedQueue.push_back(sysex);
    drainAckedQueue();
}

void ConnectionManager::drainAckedQueue()
{
    if (ackedQueueWaiting || ackedQueue.empty())
        return;

    auto msg = ackedQueue.front();
    ackedQueue.pop_front();
    ackedQueueWaiting = true;
    int generation = ++ackedQueueGeneration;
    midiDevice->sendSysEx(msg);

    std::cout << "[QUEUE] Sent queued message (gen=" << generation
              << ", " << ackedQueue.size() << " remaining), waiting for ACK" << std::endl;

    // 3-second timeout: if no ACK arrives, unblock the queue.
    // The generation check ensures only the timeout for the *current* message fires.
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(ackedTimeoutMs, [this, generation, aliveFlag]() {
        if (!*aliveFlag) return;
        if (ackedQueueWaiting && ackedQueueGeneration == generation)
        {
            std::cout << "[QUEUE] ACK timeout (gen=" << generation << ") — unblocking queue ("
                      << ackedQueue.size() << " pending)" << std::endl;
            ackedQueueWaiting = false;
            drainAckedQueue();
        }
    });
}

void ConnectionManager::onAckReceived(const AckMessage& msg)
{
    DBG("ACK received: pid1=" + juce::String(msg.pid1)
        + " type=0x" + juce::String::toHexString(msg.type)
        + " pid2=" + juce::String(msg.pid2));

    // Unblock the acked queue — any pending edit messages can now be sent
    if (ackedQueueWaiting)
    {
        ackedQueueWaiting = false;
        std::cout << "[QUEUE] ACK received, unblocking queue ("
                  << ackedQueue.size() << " pending)" << std::endl;
        drainAckedQueue();
    }

    if (waitingForUploadAck)
    {
        // Synth ACK for current upload section — advance to next
        currentPatchId = msg.pid1;
        uploadSectionIndex++;
        std::cout << "[UPLOAD] ACK for section " << (uploadSectionIndex - 1)
                  << ", patchId=" << currentPatchId << std::endl;
        sendNextUploadSection();  // sends next or completes if all done
        return;
    }

    if (waitingForPatchAck)
    {
        waitingForPatchAck = false;
        collectingSections = true;
        currentPatchId = msg.pid1;  // Store for use in parameter changes
        DBG("Patch ACK for slot " + juce::String(pendingPatchSlot)
            + ", patchId=" + juce::String(currentPatchId) + " — sending GetPatch for all 13 sections");
        sendGetPatchMessages(currentPatchId, pendingPatchSlot);
        startPatchTimeout();
    }
}

void ConnectionManager::requestPatchList()
{
    std::cout << "[PATCHLIST] requestPatchList called, connected=" << isConnected() << std::endl;

    if (!isConnected())
        return;

    // Initialize patch list to 891 empty entries (9 banks × 99 positions)
    patchListNames.clear();
    patchListNames.resize(9 * 99, "");  // All initially empty

    fetchingPatchList = true;
    patchListLoaded = false;
    patchListSection = 0;
    patchListPosition = 0;
    patchListGeneration++;  // Invalidate old timeouts

    std::cout << "[PATCHLIST] Starting request: section=0 position=0" << std::endl;
    DBG("Requesting patch list from synth (891 patches)...");

    // Send first request: section 0, position 0
    GetPatchListMessage msg;
    msg.section = patchListSection;
    msg.position = patchListPosition;
    auto payload = msg.encode();

    std::cout << "[PATCHLIST] Payload (hex): ";
    for (auto byte : payload)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
    std::cout << std::dec << std::endl;

    protocol.sendMessage(NmCmd::PatchHandling, 0, payload, /*expectsReply=*/true, /*addChecksum=*/true);

    // Start timeout
    int generation = patchListGeneration;
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(patchListTimeoutMs, [this, generation, aliveFlag]()
    {
        if (!*aliveFlag) return;
        if (generation == patchListGeneration && fetchingPatchList)
        {
            std::cout << "[PATCHLIST] TIMEOUT - delivering partial results" << std::endl;
            DBG("Patch list timeout - delivering partial results");
            fetchingPatchList = false;
            patchListLoaded = true;
            if (patchListCallback)
                patchListCallback(patchListNames);
        }
    });
}

void ConnectionManager::onPatchListReceived(const AckMessage& msg)
{
    std::cout << "[PATCHLIST] onPatchListReceived called, fetchingPatchList=" << fetchingPatchList << std::endl;

    if (!fetchingPatchList)
    {
        std::cout << "[PATCHLIST] Ignoring - not fetching" << std::endl;
        return;
    }

    std::cout << "[PATCHLIST] ACK payload size: " << msg.payload.size() << std::endl;
    std::cout << "[PATCHLIST] ACK payload (hex): ";
    for (size_t i = 0; i < std::min(msg.payload.size(), size_t(20)); ++i)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)msg.payload[i] << " ";
    std::cout << std::dec << std::endl;

    // Parse the PatchListResponse from the ACK payload
    auto response = PatchListResponseMessage::decode(
        msg.payload.data(), msg.payload.size(),
        patchListSection, patchListPosition);

    std::cout << "[PATCHLIST] Parsed " << response.entries.size() << " entries" << std::endl;
    std::cout << "[PATCHLIST] nextSection=" << response.nextSection
              << " nextPosition=" << response.nextPosition << std::endl;

    // Store entries in the flat array
    for (const auto& entry : response.entries)
    {
        int index = entry.section * 99 + entry.position;
        if (index >= 0 && index < static_cast<int>(patchListNames.size()))
        {
            patchListNames[static_cast<size_t>(index)] = entry.name.empty() ? "" : entry.name;
            std::cout << "[PATCHLIST]   Entry: section=" << entry.section
                      << " pos=" << entry.position
                      << " name=\"" << entry.name << "\"" << std::endl;
        }
    }

    DBG("Patch list: received " + juce::String(response.entries.size())
        + " entries from section " + juce::String(patchListSection)
        + " position " + juce::String(patchListPosition));

    // Check if we're done
    if (response.nextSection < 0)
    {
        std::cout << "[PATCHLIST] COMPLETE! Total entries in array: " << patchListNames.size() << std::endl;
        DBG("Patch list complete!");
        fetchingPatchList = false;
        patchListLoaded = true;
        if (patchListCallback)
            patchListCallback(patchListNames);
        return;
    }

    // Continue with next request
    patchListSection = response.nextSection;
    patchListPosition = response.nextPosition;

    std::cout << "[PATCHLIST] Requesting next: section=" << patchListSection
              << " position=" << patchListPosition << std::endl;

    GetPatchListMessage nextMsg;
    nextMsg.section = patchListSection;
    nextMsg.position = patchListPosition;
    auto payload = nextMsg.encode();
    protocol.sendMessage(NmCmd::PatchHandling, 0, payload, /*expectsReply=*/true, /*addChecksum=*/true);
}

void ConnectionManager::sendGetPatchMessages(int patchId, int slot)
{
    auto msgs = GetPatchMessage::forAllSections(patchId);
    for (auto& m : msgs)
    {
        auto payload = m.encode();
        // GetPatch uses PatchModification format (same cc=0x17).
        // Responses come as PatchPacket (cc=0x1c-0x1f), not ACK,
        // so don't set expectsReply — let them all queue and send freely.
        protocol.sendMessage(NmCmd::PatchHandling, slot, payload,
                             /*expectsReply=*/false, /*addChecksum=*/true);
    }
}

void ConnectionManager::startPatchTimeout()
{
    int generation = patchTimeoutGeneration;

    // Hard timeout: absolute max wait for entire patch
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(patchTimeoutMs, [this, generation, aliveFlag]()
    {
        if (!*aliveFlag) return;
        if (generation == patchTimeoutGeneration && collectingSections && sectionsReceived < totalSections)
        {
            DBG("Patch hard timeout: received " + juce::String(sectionsReceived) + "/" + juce::String(totalSections)
                + " sections — parsing partial data");
            finalizePatch();
        }
    });
}

void ConnectionManager::startSectionStaleTimeout()
{
    int generation = patchTimeoutGeneration;
    int currentCount = sectionsReceived;

    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(sectionStaleMs, [this, generation, currentCount, aliveFlag]()
    {
        if (!*aliveFlag) return;
        // Fire if no new sections arrived since this timer was started
        if (generation == patchTimeoutGeneration && collectingSections
            && sectionsReceived == currentCount && sectionsReceived > 0
            && sectionsReceived < totalSections)
        {
            DBG("Patch stale timeout: no new sections for " + juce::String(sectionStaleMs) + "ms"
                + " (have " + juce::String(sectionsReceived) + "/" + juce::String(totalSections) + ")");
            finalizePatch();
        }
    });
}

void ConnectionManager::finalizePatch()
{
    collectingSections = false;

    // If we have a partial accumulator (section in progress), discard it
    sectionAccumulator.clear();

    if (patchSections.empty())
    {
        DBG("No patch sections to parse");
        return;
    }

    DBG(juce::String(sectionsReceived < totalSections ? "Partial" : "All") + " "
        + juce::String(patchSections.size()) + " sections — invoking parser");

    // Mark this slot as the current one
    currentSlot = pendingPatchSlot;

    if (patchDataCallback)
        patchDataCallback(patchSections);

    patchSections.clear();
    sectionsReceived = 0;
}

void ConnectionManager::onNMInfoReceived(const NMInfoMessage& msg)
{
    // Debug: log all NMInfo subcommands we receive
    if (msg.sc != 0x39 && msg.sc != 0x3a)  // skip Lights and Meters (too spammy)
        DBG("[NMInfo] sc=0x" + juce::String::toHexString(msg.sc) + " data=" + juce::String(static_cast<int>(msg.data.size())) + " bytes");

    if (msg.sc == 0x39 && msg.lightStartIndex >= 0)  // LightMessage
    {
        int base = msg.lightStartIndex;
        for (int i = 0; i < 20 && (base + i) < 128; ++i)
            globalLightValues[base + i] = msg.lightValues[i];
        if (lightMeterCallback)
            lightMeterCallback(globalLightValues, globalMeterValues);
    }

    if (msg.sc == 0x3a && msg.meterStartIndex >= 0)  // MeterMessage
    {
        int base = msg.meterStartIndex;
        for (int i = 0; i < 5; ++i)
        {
            if ((base + i*2)   < 128) globalMeterValues[base + i*2]   = msg.meterValuesB[i];
            if ((base + i*2+1) < 128) globalMeterValues[base + i*2+1] = msg.meterValuesA[i];
        }
        if (lightMeterCallback)
            lightMeterCallback(globalLightValues, globalMeterValues);
    }

    if (msg.sc == 0x05)  // VoiceCount
    {
        DBG("[DSP] VoiceCount received: " + juce::String(msg.voiceCount[0]) + " "
            + juce::String(msg.voiceCount[1]) + " " + juce::String(msg.voiceCount[2]) + " "
            + juce::String(msg.voiceCount[3]));
        if (voiceCountCallback)
            voiceCountCallback(msg.voiceCount);
    }

    if (msg.sc == 0x38)  // NewPatchInSlot
    {
        DBG("New patch in slot " + juce::String(msg.newPatchSlot) + " pid=" + juce::String(msg.newPatchPid));

        slotDetected = true;
        slotDetectGeneration++;  // Cancel any pending fallback timer

        // Update the patch ID from the synth's notification
        currentPatchId = msg.newPatchPid;

        // Auto-request the new patch data from the synth — unless suppressed.
        // pendingSyncEchoes_: decremented for each echo from a structural edit we sent.
        // suppressNewPatchInSlot_: set during upload-in-progress.
        // suppressNextAutoFetch: one-shot flag set after upload completes.
        // waitingForUploadAck: upload in progress — don't re-fetch.
        if (pendingSyncEchoes_ > 0)
        {
            pendingSyncEchoes_--;
            std::cout << "[SYNC] Consuming sync echo (remaining: " << pendingSyncEchoes_ << ")" << std::endl;
        }
        else if (suppressNewPatchInSlot_)
        {
            std::cout << "[UPLOAD] Ignoring NewPatchInSlot (upload suppress active)" << std::endl;
        }
        else if (suppressNextAutoFetch)
        {
            suppressNextAutoFetch = false;
            std::cout << "[UPLOAD] Skipping auto-fetch after upload (NewPatchInSlot)" << std::endl;
        }
        else if (isConnected() && !waitingForPatchAck && !collectingSections
                 && !waitingForUploadAck && msg.newPatchSlot >= 0)
        {
            if (suppressNextLocationClear)
                suppressNextLocationClear = false;
            else
            {
                lastLoadedSection = -1;
                lastLoadedPosition = -1;
            }
            requestPatch(msg.newPatchSlot);
        }
        else if (waitingForUploadAck)
        {
            std::cout << "[UPLOAD] Ignoring NewPatchInSlot during upload" << std::endl;
        }
    }

    if (msg.sc == 0x40 && msg.data.size() >= 4)  // KnobChange: physical knob turned on synth
    {
        // Payload identical to ParameterChange: section, module, parameter, value
        if (parameterChangeCallback)
            parameterChangeCallback(msg.data[0], msg.data[1], msg.data[2], msg.data[3]);
    }

    if (msg.sc == 0x7e)  // Error notification from synth
    {
        int errorCode = msg.data.empty() ? -1 : msg.data[0];
        std::cout << "*** SYNTH ERROR: sc=0x7e code=" << errorCode
                  << " (pid=" << msg.pid << ")" << std::endl;
        if (synthErrorCallback)
        {
            // Capture callback by value so it's safe even if ConnectionManager
            // is destroyed before the async fires.
            auto cb = synthErrorCallback;
            juce::MessageManager::callAsync([cb, errorCode]() { cb(errorCode); });
        }
    }

    if (msg.sc == 0x09 && !msg.data.empty())  // SlotActivated
    {
        int activeSlot = msg.data[0] & 0x03;
        std::cout << "[SLOT] Active slot changed to " << activeSlot << std::endl;

        currentSlot = activeSlot;
        slotDetected = true;
        slotDetectGeneration++;  // Cancel any pending fallback timer

        // Slot changed on synth — we don't know the bank location
        lastLoadedSection = -1;
        lastLoadedPosition = -1;
        suppressNextLocationClear = false;

        // Notify UI of slot change
        if (slotChangedCallback)
            slotChangedCallback(activeSlot);

        // Auto-load patch from the active slot (unless already loading or uploading)
        if (isConnected() && !waitingForPatchAck && !collectingSections && !waitingForUploadAck)
            requestPatch(activeSlot);
    }

    // Silently handle high-frequency messages (Lights, Meters, SlotsSelected)
    // sc=0x39 (Lights), sc=0x3a (Meters), sc=0x07 (SlotsSelected)
}

void ConnectionManager::onPatchPacketReceived(const PatchPacketMessage& msg)
{
    // In Java protocol, PatchMessage.isreply = true — patch packets unblock the send queue.
    // The synth may respond to some edit commands (NewModule) with a patch confirmation packet.
    if (ackedQueueWaiting)
    {
        ackedQueueWaiting = false;
        std::cout << "[QUEUE] PatchPacket received — unblocking queue ("
                  << ackedQueue.size() << " pending)" << std::endl;
        drainAckedQueue();
    }

    if (!collectingSections && !waitingForPatchAck)
        return;  // Not expecting patch data

    if (msg.isFirst)
        sectionAccumulator.clear();

    sectionAccumulator.insert(sectionAccumulator.end(), msg.patchData.begin(), msg.patchData.end());

    if (msg.isLast)
    {
        // Store completed section separately (each has independent 7-bit encoding)
        patchSections.push_back(std::move(sectionAccumulator));
        sectionAccumulator.clear();
        sectionsReceived++;

        DBG("Received section " + juce::String(sectionsReceived) + "/" + juce::String(totalSections)
            + " (" + juce::String(patchSections.back().size()) + " bytes)");

        if (sectionsReceived >= totalSections)
        {
            DBG("All " + juce::String(totalSections) + " sections received");
            finalizePatch();
        }
        else
        {
            // Reset the stale timer — if no more sections arrive within sectionStaleMs, finalize
            startSectionStaleTimeout();
        }
    }
}

void ConnectionManager::onError(const ErrorMessage& msg)
{
    // ErrorMessage.isreply = true in Java — also unblocks the send queue
    if (ackedQueueWaiting)
    {
        std::cout << "[QUEUE] Error from synth (code " << msg.errorCode
                  << ") — unblocking queue (" << ackedQueue.size() << " pending)" << std::endl;
        ackedQueueWaiting = false;
        drainAckedQueue();
    }
    setStatus(State::Disconnected,
              "Error from synth (code " + juce::String(msg.errorCode) + ")");
}

void ConnectionManager::setStatus(State state, const juce::String& message)
{
    status.state = state;
    status.message = message;

    if (statusCallback)
        statusCallback(status);
}

void ConnectionManager::startHandshakeTimeout()
{
    // Use a Timer via callAfterDelay for the 3-second handshake timeout
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(NmProtocol::timeoutMs, [this, aliveFlag]()
    {
        if (!*aliveFlag) return;
        if (status.state == State::Connecting)
        {
            DBG("ConnectionManager: handshake timeout");
            setStatus(State::Disconnected, "No response from synth (timeout)");
        }
    });
}

void ConnectionManager::cancelHandshakeTimeout()
{
    // The callAfterDelay lambda checks state, so transitioning out of
    // Connecting effectively cancels it.
}

void ConnectionManager::startSlotDetectionFallback()
{
    int generation = slotDetectGeneration;

    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(3000, [this, generation, aliveFlag]()
    {
        if (!*aliveFlag) return;
        // Only fire if no SlotActivated/NewPatchInSlot arrived and we're still connected
        if (generation == slotDetectGeneration && !slotDetected && isConnected()
            && !waitingForPatchAck && !collectingSections)
        {
            std::cout << "[SLOT] No SlotActivated received — defaulting to slot 0" << std::endl;
            requestPatch(0);
        }
    });
}

void ConnectionManager::copyPatchInBank(int srcSection, int srcPosition, int dstSection, int dstPosition)
{
    if (!isConnected())
        return;

    std::cout << "[COPY] Copying patch from (" << srcSection << "," << srcPosition
              << ") to (" << dstSection << "," << dstPosition << ")" << std::endl;

    // Use slot 3 as temporary slot for copy operation
    constexpr int tempSlot = 3;

    // Step 1: Load source patch to temp slot
    loadPatchFromBank(srcSection, srcPosition, tempSlot);

    // Step 2: After a brief delay, store from temp slot to destination
    // The delay allows the synth to load the patch into the slot
    auto aliveFlag = alive;
    juce::Timer::callAfterDelay(500, [this, tempSlot, dstSection, dstPosition, aliveFlag]()
    {
        if (!*aliveFlag) return;
        if (!isConnected())
            return;

        std::cout << "[COPY] Storing from slot " << tempSlot << " to bank ("
                  << dstSection << "," << dstPosition << ")" << std::endl;

        // Create and send StorePatch message
        auto msg = std::make_unique<StorePatchMessage>(tempSlot, dstSection, dstPosition);
        auto sysex = msg->toSysEx(tempSlot);
        sendRawSysEx(sysex);

        std::cout << "[COPY] Copy complete!" << std::endl;
    });
}

void ConnectionManager::movePatchInBank(int srcSection, int srcPosition, int dstSection, int dstPosition)
{
    // Move = Copy + Delete source
    // For now, just do the copy. Delete requires creating an empty patch which needs serialization.
    copyPatchInBank(srcSection, srcPosition, dstSection, dstPosition);

    std::cout << "[MOVE] Copy complete. NOTE: Source patch not deleted (requires serialization)" << std::endl;
}

void ConnectionManager::deletePatchInBank(int section, int position)
{
    // Delete by creating an empty patch and storing it to the location
    std::cout << "[DELETE] Deleting patch at bank " << section << " position " << position << std::endl;

    // TODO: Implement by:
    // 1. Creating an empty/init Patch object
    // 2. Serializing it with PatchSerializer
    // 3. Somehow loading it to a temp slot (requires AddModule/AddCable commands)
    // 4. Storing to the target location
    //
    // For now, this is not implemented because the protocol doesn't have a "SetPatch"
    // command - it only has AddModule, AddCable, etc. which would require implementing
    // ~10 different message types.

    std::cout << "[DELETE] Not implemented - protocol limitation" << std::endl;
}
