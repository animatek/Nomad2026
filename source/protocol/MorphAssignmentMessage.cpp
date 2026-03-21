#include "MorphAssignmentMessage.h"

MorphAssignmentMessage::MorphAssignmentMessage(int pid, int section, int moduleIdx, int paramIdx, int morph)
    : pid_(pid)
    , section_(section)
    , moduleIdx_(moduleIdx)
    , paramIdx_(paramIdx)
    , morph_(morph)
{
}

std::vector<uint8_t> MorphAssignmentMessage::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    // Header: F0 33 [(0x17<<2)|slot] 06
    appendHeader(msg, 0x17, slot);

    // PatchModification prefix: 0:1 pid:7  0:1 sc:7(0x64)
    msg.push_back(static_cast<uint8_t>(pid_ & 0x7F));
    msg.push_back(0x64);  // sc = MorphAssignment

    // MorphAssignment payload:
    // 0:1 0x01:6 section:1  => byte = (0x01 << 1) | section
    msg.push_back(static_cast<uint8_t>((0x01 << 1) | (section_ & 0x01)));
    // 0:1 module:7
    msg.push_back(static_cast<uint8_t>(moduleIdx_ & 0x7F));
    // 0:1 parameter:7
    msg.push_back(static_cast<uint8_t>(paramIdx_ & 0x7F));
    // 0:1 morph:7
    msg.push_back(static_cast<uint8_t>(morph_ & 0x7F));
    // 0:1 0x0:7
    msg.push_back(0x00);
    // 0:1 0x0:7
    msg.push_back(0x00);

    appendFooter(msg);
    return msg;
}
