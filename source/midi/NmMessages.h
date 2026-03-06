#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <cstdint>

// Message command IDs (cc field in SysEx header)
namespace NmCmd
{
    constexpr uint8_t IAm             = 0x00;
    constexpr uint8_t ParameterChange = 0x13;
    constexpr uint8_t NMInfo          = 0x14;
    constexpr uint8_t ACK             = 0x16;
    constexpr uint8_t PatchHandling   = 0x17;
    // PatchPacket uses cc=0x1c-0x1f, bits encode first/last flags
    constexpr uint8_t PatchPacketBase = 0x1c;
}

struct IAmMessage
{
    int sender = 0;       // 0 = PC, 1 = Synth
    int versionHigh = 3;
    int versionLow = 3;
    int serial = -1;
    int deviceId = -1;

    std::vector<uint8_t> encode() const;
    static IAmMessage decode(const uint8_t* data, size_t length);
};

struct ParameterChangeMessage
{
    int pid = 0;         // patch ID from ACK
    int section = 0;     // 0 = common voice area, 1 = poly voice area
    int module = 0;
    int parameter = 0;
    int value = 0;

    std::vector<uint8_t> encode() const;
    static ParameterChangeMessage decode(const uint8_t* data, size_t length);
};

struct AckMessage
{
    int pid1 = 0;
    int type = 0;
    int pid2 = 0;

    static AckMessage decode(const uint8_t* data, size_t length);
};

struct PatchMessage
{
    int slot = 0;
    std::vector<uint8_t> patchData;

    std::vector<uint8_t> encode() const;
    static PatchMessage decode(const uint8_t* data, size_t length);
};

struct ErrorMessage
{
    int errorCode = 0;

    static ErrorMessage decode(const uint8_t* data, size_t length);
};

// NMInfo (cc=0x14): status messages from synth
// PDL2: 0:1 pid:7 0:1 sc:7 [subcommand data] checksum
struct NMInfoMessage
{
    int pid = 0;
    int sc = 0;
    std::vector<uint8_t> data;  // subcommand-specific data (after pid+sc, before checksum)

    // VoiceCount (sc=0x05): 4 voice counts, one per slot
    int voiceCount[4] = {0, 0, 0, 0};
    // NewPatchInSlot (sc=0x38)
    int newPatchSlot = -1;
    int newPatchPid = -1;

    static NMInfoMessage decode(const uint8_t* payload, size_t length);
};

// RequestPatch: sent via PatchHandling (cc=0x17)
// PDL2: pp=0x41, ssc=0x35, no additional data, with checksum
struct RequestPatchMessage
{
    int slot = 0;

    std::vector<uint8_t> encode() const;
};

// GetPatch: sent via PatchHandling (cc=0x17) using PatchModification format
// PDL2: pid:7 sc:7 [payload:7] checksum
// Requests a specific patch section from the synth after receiving ACK with patchId
struct GetPatchMessage
{
    enum Section
    {
        Header = 0,
        PolyModule, CommonModule,
        PolyCable, CommonCable,
        PolyParameter, CommonParameter,
        MorphMap, KnobMap, ControlMap,
        PolyNameDump, CommonNameDump,
        Note,
        NumSections
    };

    int pid = 0;         // patch ID from ACK
    Section section = Header;

    std::vector<uint8_t> encode() const;

    // Generate all 13 GetPatch messages for a given pid
    static std::vector<GetPatchMessage> forAllSections(int pid);
};

// PatchPacket (cc=0x1c-0x1f): patch data stream from synth
// cc bits: bit0=first, bit1=last
struct PatchPacketMessage
{
    bool isFirst = false;
    bool isLast = false;
    int command = 0;    // 0:1 command:1 pid:6
    int pid = 0;
    std::vector<uint8_t> patchData;

    static PatchPacketMessage decode(int cc, const uint8_t* payload, size_t length);
};
