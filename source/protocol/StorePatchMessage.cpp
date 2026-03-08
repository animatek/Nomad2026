#include "StorePatchMessage.h"

StorePatchMessage::StorePatchMessage(int slot, int section, int position)
    : slot_(slot)
    , section_(section)
    , position_(position)
{
}

std::vector<uint8_t> StorePatchMessage::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    // Header: F0 33 <slot|06>
    appendHeader(msg, slot);

    // Command: PatchHandling (0x17)
    msg.push_back(0x17);

    // Subcommand prefix: pp=0x41
    msg.push_back(0x41);

    // Subcommand: StorePatch (0x0b)
    msg.push_back(0x0b);

    // Payload (2 bytes):
    // Byte 0: slot:2 section:1 0:5
    int byte0 = ((slot_ & 0x03) << 6) | ((section_ & 0x01) << 5);

    // Byte 1: position:7 0:1
    int byte1 = ((position_ & 0x7F) << 1);

    msg.push_back(byte0);
    msg.push_back(byte1);

    // Footer: checksum + F7
    appendFooter(msg);

    return msg;
}
