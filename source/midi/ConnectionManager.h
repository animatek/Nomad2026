#pragma once

#include "NmProtocol.h"
#include "MidiDeviceManager.h"
#include <functional>

class ConnectionManager : public NmProtocol::Listener
{
public:
    ConnectionManager();
    ~ConnectionManager() override;

    enum class State
    {
        Disconnected,
        Connecting,
        Connected
    };

    struct Status
    {
        State state = State::Disconnected;
        juce::String message = "Disconnected";
        int synthVersionHigh = 0;
        int synthVersionLow = 0;
    };

    // Connection management
    bool connect(const juce::String& inputId, const juce::String& outputId);
    void disconnect();

    bool isConnected() const { return status.state == State::Connected; }
    const Status& getStatus() const { return status; }

    // Synth commands
    void requestPatch(int slot = 0);

    // Device enumeration (delegates to MidiDeviceManager)
    static juce::Array<juce::MidiDeviceInfo> getAvailableInputDevices();
    static juce::Array<juce::MidiDeviceInfo> getAvailableOutputDevices();

    // Callbacks
    using StatusCallback = std::function<void(const Status&)>;
    void setStatusCallback(StatusCallback cb) { statusCallback = std::move(cb); }

    using VoiceCountCallback = std::function<void(const int voiceCounts[4])>;
    void setVoiceCountCallback(VoiceCountCallback cb) { voiceCountCallback = std::move(cb); }

    using PatchDataCallback = std::function<void(const std::vector<uint8_t>& data)>;
    void setPatchDataCallback(PatchDataCallback cb) { patchDataCallback = std::move(cb); }

    NmProtocol& getProtocol() { return protocol; }

private:
    // NmProtocol::Listener
    void onIAmReceived(const IAmMessage& msg) override;
    void onAckReceived(const AckMessage& msg) override;
    void onNMInfoReceived(const NMInfoMessage& msg) override;
    void onPatchPacketReceived(const PatchPacketMessage& msg) override;
    void onError(const ErrorMessage& msg) override;

    void setStatus(State state, const juce::String& message);
    void startHandshakeTimeout();
    void cancelHandshakeTimeout();
    void sendGetPatchMessages(int patchId, int slot);

    NmProtocol protocol;
    std::unique_ptr<MidiDeviceManager> midiDevice;
    Status status;
    StatusCallback statusCallback;
    VoiceCountCallback voiceCountCallback;
    PatchDataCallback patchDataCallback;

    // Patch request state
    bool waitingForPatchAck = false;
    int pendingPatchSlot = 0;
    int patchPacketsReceived = 0;

    // Accumulate PatchPacket stream
    std::vector<uint8_t> patchAccumulator;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConnectionManager)
};
