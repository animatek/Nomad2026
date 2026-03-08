#pragma once

#include "SysExMessage.h"

/**
 * MoveModule message (cc=0x17, sc=0x34)
 * Updates a module's position in the patch layout.
 *
 * PDL2 spec (midi.pdl2:354-358):
 *   MoveModule :=
 *     0:1 section:1 0:6
 *     0:1 module:7
 *     0:1 xpos:7
 *     0:1 ypos:7
 */
class MoveModuleMessage : public SysExMessage
{
public:
    /**
     * @param section Voice area (0=common, 1=poly)
     * @param moduleIndex Module index within section
     * @param xpos X position (grid units)
     * @param ypos Y position (grid units)
     */
    MoveModuleMessage(int section, int moduleIndex, int xpos, int ypos);

    std::vector<uint8_t> toSysEx(int slot) const override;

private:
    int section_;
    int moduleIndex_;
    int xpos_;
    int ypos_;
};
