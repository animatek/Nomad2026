#include "NewCableMessage.h"

NewCableMessage::NewCableMessage(int section, SignalType color,
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

std::vector<uint8_t> NewCableMessage::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    // Header: F0 33 <slot|06>
    appendHeader(msg, slot);

    // Command: PatchHandling (0x17)
    msg.push_back(0x17);

    // Subcommand: NewCable (0x50)
    msg.push_back(0x50);

    // Payload (5 bytes, PDL2 bit-packed):
    // Byte 0: 0:1 section:1 0:1 color:4 0:1
    int byte0 = ((section_ & 0x01) << 6) | ((static_cast<int>(color_) & 0x0F) << 1);

    // Byte 1: 0:1 module1:7
    int byte1 = (module1_ & 0x7F);

    // Byte 2: 0:1 type1:2 0:1 connector1:6 (type: 0=input, 1=output)
    int type1 = isOutput1_ ? 1 : 0;
    int byte2 = ((type1 & 0x03) << 5) | (connector1_ & 0x1F);

    // Byte 3: 0:1 module2:7
    int byte3 = (module2_ & 0x7F);

    // Byte 4: 0:1 type2:2 0:1 connector2:6
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
