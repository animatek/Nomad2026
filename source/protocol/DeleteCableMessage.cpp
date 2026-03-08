#include "DeleteCableMessage.h"

DeleteCableMessage::DeleteCableMessage(int section, SignalType color,
                                       int module1, bool isOutput1, int connector1,
                                       int module2, bool isOutput2, int connector2)
    : section_(section)
    , color_(color)
    , module1_(module1)
    , module2_(module2)
    , isOutput1_(isOutput1)
    , isOutput2_(isOutput2)
    , connector1_(connector1)
    , connector2_(connector2)
{
}

std::vector<uint8_t> DeleteCableMessage::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    // Header: F0 33 <slot|06>
    appendHeader(msg, slot);

    // Command: PatchHandling (0x17)
    msg.push_back(0x17);

    // Subcommand: DeleteCable (0x51)
    msg.push_back(0x51);

    // Payload (identical structure to NewCable)
    int byte0 = ((section_ & 0x01) << 6) | ((static_cast<int>(color_) & 0x0F) << 1);
    int byte1 = (module1_ & 0x7F);
    int type1 = isOutput1_ ? 1 : 0;
    int byte2 = ((type1 & 0x03) << 5) | (connector1_ & 0x1F);
    int byte3 = (module2_ & 0x7F);
    int type2 = isOutput2_ ? 1 : 0;
    int byte4 = ((type2 & 0x03) << 5) | (connector2_ & 0x1F);

    msg.push_back(byte0);
    msg.push_back(byte1);
    msg.push_back(byte2);
    msg.push_back(byte3);
    msg.push_back(byte4);

    // Footer: checksum + F7
    appendFooter(msg);

    return msg;
}
