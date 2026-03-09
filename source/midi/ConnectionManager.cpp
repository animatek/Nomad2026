#include "ConnectionManager.h"
#include <iostream>

ConnectionManager::ConnectionManager()
{
    protocol.addListener(this);
}

ConnectionManager::~ConnectionManager()
{
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

void ConnectionManager::sendRawSysEx(const std::vector<uint8_t>& sysex)
{
    if (!isConnected() || !midiDevice)
        return;

    // Send the raw SysEx bytes directly via the MIDI device
    // This bypasses the NmProtocol queue system, for use by PatchSynchronizer
    midiDevice->sendSysEx(sysex);
}

void ConnectionManager::onAckReceived(const AckMessage& msg)
{
    DBG("ACK received: pid1=" + juce::String(msg.pid1)
        + " type=0x" + juce::String::toHexString(msg.type)
        + " pid2=" + juce::String(msg.pid2));

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

    DBG("Requesting patch list from synth (891 patches)...");

    // Send first request: section 0, position 0
    GetPatchListMessage msg;
    msg.section = patchListSection;
    msg.position = patchListPosition;
    auto payload = msg.encode();
    protocol.sendMessage(NmCmd::PatchHandling, 0, payload, /*expectsReply=*/true, /*addChecksum=*/true);

    // Start timeout
    int generation = patchListGeneration;
    juce::Timer::callAfterDelay(patchListTimeoutMs, [this, generation]()
    {
        if (generation == patchListGeneration && fetchingPatchList)
        {
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
    if (!fetchingPatchList)
        return;

    // Parse the PatchListResponse from the ACK payload
    auto response = PatchListResponseMessage::decode(
        msg.payload.data(), msg.payload.size(),
        patchListSection, patchListPosition);

    // Store entries in the flat array
    for (const auto& entry : response.entries)
    {
        int index = entry.section * 99 + entry.position;
        if (index >= 0 && index < static_cast<int>(patchListNames.size()))
        {
            patchListNames[static_cast<size_t>(index)] = entry.name.empty() ? "" : entry.name;
        }
    }

    DBG("Patch list: received " + juce::String(response.entries.size())
        + " entries from section " + juce::String(patchListSection)
        + " position " + juce::String(patchListPosition));

    // Check if we're done
    if (response.nextSection < 0)
    {
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
    juce::Timer::callAfterDelay(patchTimeoutMs, [this, generation]()
    {
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

    juce::Timer::callAfterDelay(sectionStaleMs, [this, generation, currentCount]()
    {
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
    if (msg.sc == 0x05 && voiceCountCallback)  // VoiceCount
        voiceCountCallback(msg.voiceCount);

    if (msg.sc == 0x38)  // NewPatchInSlot
    {
        DBG("New patch in slot " + juce::String(msg.newPatchSlot) + " pid=" + juce::String(msg.newPatchPid));

        slotDetected = true;
        slotDetectGeneration++;  // Cancel any pending fallback timer

        // Auto-request the new patch data from the synth
        // Don't interrupt an in-progress collection
        if (isConnected() && !waitingForPatchAck && !collectingSections && msg.newPatchSlot >= 0)
            requestPatch(msg.newPatchSlot);
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
        DBG("*** SYNTH ERROR: sc=0x7e code=" + juce::String(errorCode)
            + " (pid=" + juce::String(msg.pid) + ")");
    }

    if (msg.sc == 0x09 && !msg.data.empty())  // SlotActivated
    {
        int activeSlot = msg.data[0] & 0x03;
        std::cout << "[SLOT] Active slot changed to " << activeSlot << std::endl;

        currentSlot = activeSlot;
        slotDetected = true;
        slotDetectGeneration++;  // Cancel any pending fallback timer

        // Auto-load patch from the active slot (unless already loading)
        if (isConnected() && !waitingForPatchAck && !collectingSections)
            requestPatch(activeSlot);
    }

    // Silently handle high-frequency messages (Lights, Meters, SlotsSelected)
    // sc=0x39 (Lights), sc=0x3a (Meters), sc=0x07 (SlotsSelected)
}

void ConnectionManager::onPatchPacketReceived(const PatchPacketMessage& msg)
{
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
    juce::Timer::callAfterDelay(NmProtocol::timeoutMs, [this]()
    {
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

    juce::Timer::callAfterDelay(3000, [this, generation]()
    {
        // Only fire if no SlotActivated/NewPatchInSlot arrived and we're still connected
        if (generation == slotDetectGeneration && !slotDetected && isConnected()
            && !waitingForPatchAck && !collectingSections)
        {
            std::cout << "[SLOT] No SlotActivated received — defaulting to slot 0" << std::endl;
            requestPatch(0);
        }
    });
}
