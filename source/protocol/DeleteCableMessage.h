#pragma once

#include "SysExMessage.h"
#include "../model/SignalType.h"

/**
 * DeleteCable message (cc=0x17, sc=0x51)
 * Removes a cable connection between two module connectors.
 *
 * Uses identical payload structure to NewCable.
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
