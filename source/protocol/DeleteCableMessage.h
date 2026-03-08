#pragma once

#include "SysExMessage.h"
#include "../model/SignalType.h"

/**
 * CableDelete message (cc=0x17, sc=0x51)
 * Removes a cable connection between two module connectors.
 *
 * PDL2 spec (midi.pdl2):
 *   CableDelete :=
 *     0:1 1:6 section:1
 *     0:1 module1:7 0:1 type1:1 connector1:6
 *     0:1 module2:7 0:1 type2:1 connector2:6
 */
class DeleteCableMessage : public SysExMessage
{
public:
    DeleteCableMessage(int section, SignalType color,
                      int module1, bool isOutput1, int connector1,
                      int module2, bool isOutput2, int connector2);

    std::vector<uint8_t> toSysEx(int slot) const override;

private:
    int section_;
    SignalType color_;
    int module1_, module2_;
    bool isOutput1_, isOutput2_;
    int connector1_, connector2_;
};
