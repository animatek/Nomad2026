#include "SysExMessage.h"

uint8_t SysExMessage::calculateChecksum(const std::vector<uint8_t>& bytes)
{
    uint32_t sum = 0;
    for (auto byte : bytes)
        sum += byte;
    return static_cast<uint8_t>(sum % 128);
}

void SysExMessage::appendHeader(std::vector<uint8_t>& msg, int slot)
{
    msg.push_back(0xF0);
    msg.push_back(0x33);
    // Embed slot (bits 0-1) with command class 0x06 (bits 2-7)
    msg.push_back(((slot & 0x03) << 0) | (0x06 << 2));
}

void SysExMessage::appendFooter(std::vector<uint8_t>& msg)
{
    // Checksum includes all bytes from F0 through last payload byte
    uint8_t checksum = calculateChecksum(msg);
    msg.push_back(checksum);
    msg.push_back(0xF7);
}
