#pragma once

#include "SysExMessage.h"

/**
 * StorePatch message (cc=0x17, pp=0x41, sc=0x0b)
 * Saves the current patch to permanent synth memory.
 *
 * PDL2 spec (midi.pdl2):
 *   StorePatch :=
 *     0:1 slot:7 0:1 section:7 0:1 position:7
 */
class StorePatchMessage : public SysExMessage
{
public:
    /**
     * @param slot Device slot (0-3)
     * @param section Voice area (0=common, 1=poly)
     * @param position Bank position (0-99 for user banks)
     */
    StorePatchMessage(int slot, int section, int position);

    std::vector<uint8_t> toSysEx(int slot) const override;

private:
    int slot_;
    int section_;
    int position_;
};
