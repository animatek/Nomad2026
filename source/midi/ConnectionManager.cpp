#include "ConnectionManager.h"

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
    }
}

void ConnectionManager::requestPatch(int slot)
{
    if (!isConnected())
        return;

    waitingForPatchAck = true;
    pendingPatchSlot = slot;
    patchPacketsReceived = 0;
    patchAccumulator.clear();

    RequestPatchMessage req;
    req.slot = slot;
    auto payload = req.encode();
    protocol.sendMessage(NmCmd::PatchHandling, slot, payload, /*expectsReply=*/true, /*addChecksum=*/true);

    DBG("Requesting patch from slot " + juce::String(slot));
}

void ConnectionManager::onAckReceived(const AckMessage& msg)
{
    DBG("ACK received: pid1=" + juce::String(msg.pid1)
        + " type=0x" + juce::String::toHexString(msg.type)
        + " pid2=" + juce::String(msg.pid2));

    if (waitingForPatchAck)
    {
        waitingForPatchAck = false;
        int patchId = msg.pid1;
        DBG("Patch ACK for slot " + juce::String(pendingPatchSlot)
            + ", patchId=" + juce::String(patchId) + " — sending GetPatch for all 13 sections");
        sendGetPatchMessages(patchId, pendingPatchSlot);
    }
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

void ConnectionManager::onNMInfoReceived(const NMInfoMessage& msg)
{
    if (msg.sc == 0x05 && voiceCountCallback)  // VoiceCount
        voiceCountCallback(msg.voiceCount);

    if (msg.sc == 0x38)  // NewPatchInSlot
        DBG("New patch in slot " + juce::String(msg.newPatchSlot) + " pid=" + juce::String(msg.newPatchPid));
}

void ConnectionManager::onPatchPacketReceived(const PatchPacketMessage& msg)
{
    if (msg.isFirst)
        patchAccumulator.clear();

    patchAccumulator.insert(patchAccumulator.end(), msg.patchData.begin(), msg.patchData.end());

    if (msg.isLast)
    {
        DBG("Received complete patch data: " + juce::String(patchAccumulator.size()) + " bytes");
        if (patchDataCallback)
            patchDataCallback(patchAccumulator);
        patchAccumulator.clear();
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
