#include "NmMessages.h"

// --- IAmMessage ---
// PDL2: IAm := 0:1 sender:7 0:1 versionHigh:7 0:1 versionLow:7
// cc=0x00 is in the SysEx header, NOT repeated in payload.
// No checksum.

std::vector<uint8_t> IAmMessage::encode() const
{
    return {
        static_cast<uint8_t>(sender & 0x7F),
        static_cast<uint8_t>(versionHigh & 0x7F),
        static_cast<uint8_t>(versionLow & 0x7F)
    };
}

IAmMessage IAmMessage::decode(const uint8_t* data, size_t length)
{
    IAmMessage msg;
    if (length < 3)
        return msg;

    msg.sender       = data[0];
    msg.versionHigh  = data[1];
    msg.versionLow   = data[2];

    // Synth response (sender=1) includes identification fields
    if (msg.sender == 1 && length >= 7)
    {
        // reserved = data[3]
        msg.serial = (data[4] << 7) | data[5];
        msg.deviceId = data[6];
    }

    return msg;
}

// --- ParameterChangeMessage ---
// PDL2: Parameter := 0:1 pid:7 0:1 sc:7 ... checksum
// cc=0x13 is in the SysEx header. Payload starts with pid, sc.
// ParameterChange subcommand: sc=0x40, then section, module, parameter, value.

std::vector<uint8_t> ParameterChangeMessage::encode() const
{
    return {
        static_cast<uint8_t>(0x00),               // pid
        static_cast<uint8_t>(0x40),               // sc = ParameterChange
        static_cast<uint8_t>(section & 0x7F),
        static_cast<uint8_t>(module & 0x7F),
        static_cast<uint8_t>(parameter & 0x7F),
        static_cast<uint8_t>(value & 0x7F)
    };
}

ParameterChangeMessage ParameterChangeMessage::decode(const uint8_t* data, size_t length)
{
    ParameterChangeMessage msg;
    // Payload: pid(1) + sc(1) + section + module + parameter + value [+ checksum]
    if (length < 6 || data[1] != 0x40)
        return msg;

    msg.section   = data[2];
    msg.module    = data[3];
    msg.parameter = data[4];
    msg.value     = data[5];
    return msg;
}

// --- AckMessage ---
// PDL2: ACK := 0:1 pid1:7 0:1 type:7 0:1 pid2:7

AckMessage AckMessage::decode(const uint8_t* data, size_t length)
{
    AckMessage msg;
    if (length < 3)
        return msg;

    msg.pid1 = data[0];
    msg.type = data[1];
    msg.pid2 = data[2];
    return msg;
}

// --- PatchMessage ---
// PDL2: PatchHandling uses cc=0x17, payload is the patch-specific data.

std::vector<uint8_t> PatchMessage::encode() const
{
    std::vector<uint8_t> payload;
    payload.push_back(static_cast<uint8_t>(slot & 0x7F));
    payload.insert(payload.end(), patchData.begin(), patchData.end());
    return payload;
}

PatchMessage PatchMessage::decode(const uint8_t* data, size_t length)
{
    PatchMessage msg;
    if (length < 1)
        return msg;

    msg.slot = data[0];
    if (length > 1)
        msg.patchData.assign(data + 1, data + length);
    return msg;
}

// --- ErrorMessage ---

ErrorMessage ErrorMessage::decode(const uint8_t* data, size_t length)
{
    ErrorMessage msg;
    if (length >= 1)
        msg.errorCode = data[0];
    return msg;
}

// --- NMInfoMessage ---
// PDL2: NMInfo := 0:1 pid:7 0:1 sc:7 [subcommand data] checksum

NMInfoMessage NMInfoMessage::decode(const uint8_t* payload, size_t length)
{
    NMInfoMessage msg;
    if (length < 3)  // pid + sc + checksum minimum
        return msg;

    msg.pid = payload[0];
    msg.sc  = payload[1];

    // Data is between sc and checksum (last byte)
    size_t dataStart = 2;
    size_t dataEnd = length - 1;  // checksum is last byte
    if (dataEnd > dataStart)
        msg.data.assign(payload + dataStart, payload + dataEnd);

    switch (msg.sc)
    {
        case 0x05:  // VoiceCount: 4 slots
            if (msg.data.size() >= 4)
            {
                for (int i = 0; i < 4; ++i)
                    msg.voiceCount[i] = msg.data[static_cast<size_t>(i)];
            }
            break;

        case 0x38:  // NewPatchInSlot: slot + pid
            if (msg.data.size() >= 2)
            {
                msg.newPatchSlot = msg.data[0];
                msg.newPatchPid  = msg.data[1];
            }
            break;

        default:
            break;
    }

    return msg;
}

// --- RequestPatchMessage ---
// PDL2: PatchHandling -> PatchCommand(pp=0x41) -> PatchManagerCommand(ssc=0x35) -> RequestPatch (empty)
// Sent with cc=0x17, has checksum

std::vector<uint8_t> RequestPatchMessage::encode() const
{
    return {
        0x41,   // pp = PatchManagerCommand
        0x35    // ssc = RequestPatch
    };
}

// --- GetPatchMessage ---
// PDL2: PatchModification := 0:1 pid:7 0:1 sc:7 [GetPatchPartExtra: 0:1 payload:7] checksum
// Sent with cc=0x17, uses PatchModification branch (not PatchCommand)

std::vector<uint8_t> GetPatchMessage::encode() const
{
    // SC codes and optional payload bytes per section (from Java GetPatchMessage.java)
    struct SectionDef { uint8_t sc; int payload; }; // payload=-1 means none
    static const SectionDef defs[NumSections] = {
        { 0x20, 0x28 },  // Header
        { 0x4b, 0x01 },  // PolyModule
        { 0x4b, 0x00 },  // CommonModule
        { 0x53, 0x01 },  // PolyCable
        { 0x53, 0x00 },  // CommonCable
        { 0x4c, 0x01 },  // PolyParameter
        { 0x4c, 0x00 },  // CommonParameter
        { 0x66, -1   },  // MorphMap
        { 0x63, -1   },  // KnobMap
        { 0x61, -1   },  // ControlMap
        { 0x4e, 0x01 },  // PolyNameDump
        { 0x4e, 0x00 },  // CommonNameDump
        { 0x68, -1   },  // Note
    };

    auto& def = defs[section];
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(pid & 0x7F));
    data.push_back(def.sc);
    if (def.payload >= 0)
        data.push_back(static_cast<uint8_t>(def.payload));
    return data;
}

std::vector<GetPatchMessage> GetPatchMessage::forAllSections(int pid)
{
    std::vector<GetPatchMessage> msgs;
    msgs.reserve(NumSections);
    for (int i = 0; i < NumSections; ++i)
    {
        GetPatchMessage m;
        m.pid = pid;
        m.section = static_cast<Section>(i);
        msgs.push_back(m);
    }
    return msgs;
}

// --- PatchPacketMessage ---
// PDL2: PatchPacket := 0:1 command:1 pid:6 [embedded_stream] checksum
// cc=0x1c-0x1f, bit0 of cc=first, bit1=last

PatchPacketMessage PatchPacketMessage::decode(int cc, const uint8_t* payload, size_t length)
{
    PatchPacketMessage msg;
    msg.isFirst = (cc & 1) != 0;
    msg.isLast  = ((cc >> 1) & 1) != 0;

    if (length < 2)  // header byte + checksum minimum
        return msg;

    msg.command = (payload[0] >> 6) & 1;
    msg.pid     = payload[0] & 0x3F;

    // Data between header and checksum (last byte)
    if (length > 2)
        msg.patchData.assign(payload + 1, payload + length - 1);

    return msg;
}
