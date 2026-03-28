#include "MorphKeyboardAssignmentMessage.h"

MorphKeyboardAssignmentMessage::MorphKeyboardAssignmentMessage(int pid, int morph, int keyboard)
    : pid_(pid)
    , morph_(morph)
    , keyboard_(keyboard)
{
}

std::vector<uint8_t> MorphKeyboardAssignmentMessage::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    // Header: F0 33 [(0x17<<2)|slot] 06
    appendHeader(msg, 0x17, slot);

    // PatchModification prefix: 0:1 pid:7  0:1 sc:7(0x67)
    msg.push_back(static_cast<uint8_t>(pid_ & 0x7F));
    msg.push_back(0x67);  // sc = MorphKeyboardAssignment

    // MorphKeyboardAssignment payload:
    // 0:1 morph:7
    msg.push_back(static_cast<uint8_t>(morph_ & 0x7F));
    // 0:1 keyboard:7
    msg.push_back(static_cast<uint8_t>(keyboard_ & 0x7F));

    appendFooter(msg);
    return msg;
}
