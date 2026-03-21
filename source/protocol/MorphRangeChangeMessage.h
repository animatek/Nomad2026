#pragma once

#include "SysExMessage.h"

/**
 * MorphRangeChange message (cc=0x13, sc=0x43)
 * Sets the morph range (span + direction) for a parameter.
 * Used for "Zero Morph": send span=0, direction=0.
 *
 * PDL2 spec (midi.pdl2):
 *   cc=0x13, sc=0x43
 *   MorphRangeChange :=
 *     0:1 section:7  0:1 module:7  0:1 parameter:7
 *     0:1 span:7  0:1 direction:7
 */
class MorphRangeChangeMessage : public SysExMessage
{
public:
    /**
     * @param pid       Patch ID (from ACK response)
     * @param section   Voice area (0=common, 1=poly)
     * @param moduleIdx Module index within section
     * @param paramIdx  Parameter index within module
     * @param span      Morph range span (0-127)
     * @param direction Morph direction (0=positive, 1=negative)
     */
    MorphRangeChangeMessage(int pid, int section, int moduleIdx, int paramIdx, int span, int direction);

    std::vector<uint8_t> toSysEx(int slot) const override;

private:
    int pid_;
    int section_;
    int moduleIdx_;
    int paramIdx_;
    int span_;
    int direction_;
};
