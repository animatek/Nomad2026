#pragma once

#include "SysExMessage.h"
#include "../model/SignalType.h"

/**
 * NewCable message (cc=0x17, sc=0x50)
 * Adds a cable connection between two module connectors.
 *
 * PDL2 spec (midi.pdl2:382-393):
 *   NewCable :=
 *     0:1 section:1 0:1 color:4 0:1
 *     0:1 module1:7 0:1 type1:2 0:1 connector1:6 0:1
 *     0:1 module2:7 0:1 type2:2 0:1 connector2:6 0:1
 */
class NewCableMessage : public SysExMessage
{
public:
    /**
     * @param section Voice area (0=common, 1=poly)
     * @param color Signal type/cable color
     * @param module1 First module index
     * @param isOutput1 True if connector1 is an output
     * @param connector1 Connector index within module1
     * @param module2 Second module index
     * @param isOutput2 True if connector2 is an output
     * @param connector2 Connector index within module2
     */
    NewCableMessage(int section, SignalType color,
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
