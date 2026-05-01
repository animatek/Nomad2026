#include "DeletePatchMessage.h"

DeletePatchMessage::DeletePatchMessage(int section, int position)
    : section_(section)
    , position_(position)
{
}

std::vector<uint8_t> DeletePatchMessage::toSysEx(int slot) const
{
    std::vector<uint8_t> msg;

    appendHeader(msg, 0x17, slot);

    msg.push_back(0x41);  // pp = PatchManagerCommand
    msg.push_back(0x0c);  // ssc = DeletePatch
    msg.push_back(static_cast<uint8_t>(section_ & 0x7F));
    msg.push_back(static_cast<uint8_t>(position_ & 0x7F));
    msg.push_back(0x00);  // PDL2 DeletePatch has one trailing 8-bit zero

    appendFooter(msg);
    return msg;
}
