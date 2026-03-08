#include "MoveModuleMessage.h"

MoveModuleMessage::MoveModuleMessage(int section, int moduleIndex, int xpos, int ypos)
    : section_(section)
    , moduleIndex_(moduleIndex)
    , xpos_(xpos)
    , ypos_(ypos)
{
}

std::vector<uint8_t> MoveModuleMessage::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    // Header: F0 33 <slot|06>
    appendHeader(msg, slot);

    // Command: PatchHandling (0x17)
    msg.push_back(0x17);

    // Subcommand: MoveModule (0x34)
    msg.push_back(0x34);

    // Payload (4 bytes) per PDL2 spec:
    // ModuleMove := 0:1 section:7 0:1 module:7 0:1 xpos:7 0:1 ypos:7

    // Byte 0: 0:1 section:7
    int byte0 = (section_ & 0x7F);

    // Byte 1: 0:1 module:7
    int byte1 = (moduleIndex_ & 0x7F);

    // Byte 2: 0:1 xpos:7
    int byte2 = (xpos_ & 0x7F);

    // Byte 3: 0:1 ypos:7
    int byte3 = (ypos_ & 0x7F);

    msg.push_back(byte0);
    msg.push_back(byte1);
    msg.push_back(byte2);
    msg.push_back(byte3);

    // Footer: checksum + F7
    appendFooter(msg);

    return msg;
}
