#include "MorphRangeChangeMessage.h"

MorphRangeChangeMessage::MorphRangeChangeMessage(int pid, int section, int moduleIdx, int paramIdx, int span, int direction)
    : pid_(pid)
    , section_(section)
    , moduleIdx_(moduleIdx)
    , paramIdx_(paramIdx)
    , span_(span)
    , direction_(direction)
{
}

std::vector<uint8_t> MorphRangeChangeMessage::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    // Header: F0 33 [(0x13<<2)|slot] 06
    appendHeader(msg, 0x13, slot);

    // PDL2: Parameter := 0:1 pid:7  0:1 sc:7  ...
    // pid first (same as ParameterChangeMessage)
    msg.push_back(static_cast<uint8_t>(pid_ & 0x7F));
    // sc = 0x43 (MorphRangeChange)
    msg.push_back(0x43);

    // MorphRangeChange data:
    msg.push_back(static_cast<uint8_t>(section_ & 0x7F));
    msg.push_back(static_cast<uint8_t>(moduleIdx_ & 0x7F));
    msg.push_back(static_cast<uint8_t>(paramIdx_ & 0x7F));
    msg.push_back(static_cast<uint8_t>(span_ & 0x7F));
    msg.push_back(static_cast<uint8_t>(direction_ & 0x7F));

    appendFooter(msg);
    return msg;
}
