#pragma once

#include "SysExMessage.h"

/**
 * DeletePatch message (cc=0x17, pp=0x41, sc=0x0c)
 *
 * Deletes/clears a patch from synth flash bank storage.
 */
class DeletePatchMessage : public SysExMessage
{
public:
    DeletePatchMessage(int section, int position);

    std::vector<uint8_t> toSysEx(int slot = 0) const override;

private:
    int section_ = 0;
    int position_ = 0;
};
