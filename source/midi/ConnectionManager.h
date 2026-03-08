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
    void sendParameter(int section, int moduleId, int parameterId, int value);
    void sendRawSysEx(const std::vector<uint8_t>& sysex);

    int getCurrentSlot() const;

    // Device enumeration (delegates to MidiDeviceManager)
    static juce::Array<juce::MidiDeviceInfo> getAvailableInputDevices();
    static juce::Array<juce::MidiDeviceInfo> getAvailableOutputDevices();

    // Callbacks
    using StatusCallback = std::function<void(const Status&)>;
    void setStatusCallback(StatusCallback cb) { statusCallback = std::move(cb); }

    using VoiceCountCallback = std::function<void(const int voiceCounts[4])>;
    void setVoiceCountCallback(VoiceCountCallback cb) { voiceCountCallback = std::move(cb); }

    using PatchDataCallback = std::function<void(const std::vector<std::vector<uint8_t>>& sections)>;
    void setPatchDataCallback(PatchDataCallback cb) { patchDataCallback = std::move(cb); }

    // Called when synth sends a parameter change (knob turned on hardware)
    using ParameterChangeCallback = std::function<void(int section, int moduleId, int parameterId, int value)>;
    void setParameterChangeCallback(ParameterChangeCallback cb) { parameterChangeCallback = std::move(cb); }

    NmProtocol& getProtocol() { return protocol; }

private:
    // NmProtocol::Listener
    void onIAmReceived(const IAmMessage& msg) override;
    void onParameterChanged(const ParameterChangeMessage& msg) override;
    void onAckReceived(const AckMessage& msg) override;
    void onNMInfoReceived(const NMInfoMessage& msg) override;
    void onPatchPacketReceived(const PatchPacketMessage& msg) override;
    void onError(const ErrorMessage& msg) override;

    void setStatus(State state, const juce::String& message);
    void startHandshakeTimeout();
    void cancelHandshakeTimeout();
    void sendGetPatchMessages(int patchId, int slot);
    void startPatchTimeout();
    void startSectionStaleTimeout();
    void finalizePatch();

    NmProtocol protocol;
    std::unique_ptr<MidiDeviceManager> midiDevice;
    Status status;
    StatusCallback statusCallback;
    VoiceCountCallback voiceCountCallback;
    PatchDataCallback patchDataCallback;
    ParameterChangeCallback parameterChangeCallback;

    // Patch request state
    bool waitingForPatchAck = false;
    bool collectingSections = false;
    int pendingPatchSlot = 0;
    int currentSlot = 0;  // Track which slot is currently loaded
    int currentPatchId = 0;  // Track the patch ID from ACK (used in parameter changes)
    int patchPacketsReceived = 0;

    // Accumulate PatchPacket stream — each completed section stored separately
    std::vector<uint8_t> sectionAccumulator;              // current section being assembled
    std::vector<std::vector<uint8_t>> patchSections;      // completed sections
    int sectionsReceived = 0;
    static constexpr int totalSections = 13;
    static constexpr int patchTimeoutMs = 8000;   // Hard timeout: 8 seconds max for entire patch
    static constexpr int sectionStaleMs = 2000;   // Stale timeout: 2 seconds since last section received
    int patchTimeoutGeneration = 0;  // Incremented on each new request to invalidate old timeouts

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConnectionManager)
};
