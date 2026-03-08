#pragma once

#include <vector>
#include <cstdint>

/**
 * Base class for all SysEx protocol messages.
 * Handles common operations like checksum calculation and 7-bit encoding.
 */
class SysExMessage
{
public:
    virtual ~SysExMessage() = default;

    /**
     * Construct the full SysEx message (F0 ... F7).
     * @param slot Device slot (0-3)
     * @return Complete SysEx message bytes
     */
    virtual std::vector<uint8_t> toSysEx(int slot = 0) const = 0;

protected:
    /**
     * Calculate checksum for SysEx message.
     * IMPORTANT: Checksum includes ALL bytes from F0 through last payload byte.
     * @param bytes Message bytes (including F0, excluding checksum and F7)
     * @return Checksum value (sum % 128)
     */
    static uint8_t calculateChecksum(const std::vector<uint8_t>& bytes);

    /**
     * Helper: Create SysEx header [F0 33 <slot|06>]
     * The header byte embeds slot (bits 0-1) and command class 0x06.
     */
    static void appendHeader(std::vector<uint8_t>& msg, int slot);

    /**
     * Helper: Append checksum and F7 terminator.
     */
    static void appendFooter(std::vector<uint8_t>& msg);
};
