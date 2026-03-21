#pragma once

#include "SysExMessage.h"

/**
 * MorphAssignment message (cc=0x17, PatchModification sc=0x64)
 * Assigns a parameter to a morph group (0-3).
 *
 * PDL2 spec (midi.pdl2):
 *   PatchModification := 0:1 pid:7 0:1 sc:7 ...
 *   MorphAssignment :=
 *     0:1 0x01:6 section:1  0:1 module:7  0:1 parameter:7
 *     0:1 morph:7  0:1 0x0:7  0:1 0x0:7
 */
class MorphAssignmentMessage : public SysExMessage
{
public:
    /**
     * @param pid       Patch ID (from ACK response)
     * @param section   Voice area (0=common, 1=poly)
     * @param moduleIdx Module index within section
     * @param paramIdx  Parameter index within module
     * @param morph     Morph group (0-3)
     */
    MorphAssignmentMessage(int pid, int section, int moduleIdx, int paramIdx, int morph);

    std::vector<uint8_t> toSysEx(int slot) const override;

private:
    int pid_;
    int section_;
    int moduleIdx_;
    int paramIdx_;
    int morph_;
};
