#pragma once

#include "NmProtocol.h"
#include "MidiDeviceManager.h"
#include <functional>

class Patch;

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
    void loadPatchFromBank(int section, int position, int targetSlot = -1);  // -1 = use current slot
    void uploadPatch(int slot, const Patch& patch);  // Upload full patch to synth working slot
    void sendParameter(int section, int moduleId, int parameterId, int value);
    void sendPatchTitle(const juce::String& title);  // Change patch name in current slot (not saved to flash)
    void sendRawSysEx(const std::vector<uint8_t>& sysex);

    // Bank operations (high-level)
    void copyPatchInBank(int srcSection, int srcPosition, int dstSection, int dstPosition);
    void movePatchInBank(int srcSection, int srcPosition, int dstSection, int dstPosition);
    void deletePatchInBank(int section, int position);

    int getCurrentSlot() const;
    int getCurrentPatchId() const { return currentPatchId; }

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

    // Called when synth sends an error notification (sc=0x7e)
    using SynthErrorCallback = std::function<void(int errorCode)>;
    void setSynthErrorCallback(SynthErrorCallback cb) { synthErrorCallback = std::move(cb); }

    // Called when synth ACKs an uploadPatch() — safe to send StorePatch now
    using UploadCompleteCallback = std::function<void()>;
    void setUploadCompleteCallback(UploadCompleteCallback cb) { uploadCompleteCallback = std::move(cb); }

    // Patch list management
    using PatchListCallback = std::function<void(const std::vector<std::string>& names)>;
    void setPatchListCallback(PatchListCallback cb) { patchListCallback = std::move(cb); }
    void requestPatchList();  // Start loading all 891 patch names from synth
    const std::vector<std::string>& getPatchList() const { return patchListNames; }
    bool isPatchListLoaded() const { return patchListLoaded; }

    NmProtocol& getProtocol() { return protocol; }

private:
    // NmProtocol::Listener
    void onIAmReceived(const IAmMessage& msg) override;
    void onParameterChanged(const ParameterChangeMessage& msg) override;
    void onAckReceived(const AckMessage& msg) override;
    void onPatchListReceived(const AckMessage& msg) override;
    void onNMInfoReceived(const NMInfoMessage& msg) override;
    void onPatchPacketReceived(const PatchPacketMessage& msg) override;
    void onError(const ErrorMessage& msg) override;

    void setStatus(State state, const juce::String& message);
    void startHandshakeTimeout();
    void cancelHandshakeTimeout();
    void startSlotDetectionFallback();
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
    SynthErrorCallback synthErrorCallback;
    UploadCompleteCallback uploadCompleteCallback;

    // Patch request state
    bool waitingForPatchAck = false;
    bool collectingSections = false;
    bool waitingForUploadAck = false;  // True while waiting for synth ACK after uploadPatch
    // Sequential upload state: send one section at a time, wait for ACK between each
    std::vector<std::vector<uint8_t>> uploadSections;  // serialized PDL2 sections
    std::vector<uint8_t> buildUploadSysEx(int sectionIndex, int numSections, int slot);
    void sendNextUploadSection();
    int uploadSlot = 0;
    int uploadSectionIndex = 0;
    int pendingPatchSlot = 0;
    int currentSlot = 0;  // Track which slot is currently loaded
    int currentPatchId = 0;  // Track the patch ID from ACK (used in parameter changes)
    int patchPacketsReceived = 0;

    // Accumulate PatchPacket stream — each completed section stored separately
    std::vector<uint8_t> sectionAccumulator;              // current section being assembled
    std::vector<std::vector<uint8_t>> patchSections;      // completed sections
    int sectionsReceived = 0;
    static constexpr int totalSections = 13;
    // patchTimeoutMs: absolute upper bound for the full 13-section fetch.
    // 8 s chosen empirically — a slow USB-MIDI round trip for 13 sections is ~2-3 s;
    // 8 s gives headroom for sluggish hosts without hanging the UI indefinitely.
    static constexpr int patchTimeoutMs = 8000;
    // sectionStaleMs: if no new section arrives within this window, the transfer
    // is considered stalled (synth dropped a packet).  2 s > worst observed gap.
    static constexpr int sectionStaleMs = 2000;
    int patchTimeoutGeneration = 0;  // Incremented on each new request to invalidate old timeouts

    // Slot detection: synth sends SlotActivated after handshake
    bool slotDetected = false;
    int slotDetectGeneration = 0;  // Invalidate fallback timer when slot is detected

    // Patch list retrieval state
    bool fetchingPatchList = false;
    bool patchListLoaded = false;
    int patchListSection = 0;      // Current section (0-8) being requested
    int patchListPosition = 0;     // Current position (0-98) being requested
    int patchListGeneration = 0;   // Invalidate old timeouts
    std::vector<std::string> patchListNames;  // 891 entries (9 banks × 99 positions)
    PatchListCallback patchListCallback;
    // patchListTimeoutMs: 891 patches × one request/response each.
    // Measured at ~8-9 s on a real G1; 10 s allows for occasional retransmits.
    static constexpr int patchListTimeoutMs = 10000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConnectionManager)
};
