#pragma once

#include "SysExMessage.h"

/**
 * MorphKeyboardAssignment message (cc=0x17, PatchModification sc=0x67)
 * Assigns a morph group knob to keyboard velocity or note.
 *
 * PDL2 spec (midi.pdl2):
 *   MorphKeyboardAssignment :=
 *     0:1 morph:7  0:1 keyboard:7  ?NextMorphKeyboardAssignment$data
 *
 * keyboard values: 0=disable, 1=velocity, 2=note
 */
class MorphKeyboardAssignmentMessage : public SysExMessage
{
public:
    static constexpr int KEYBOARD_DISABLE  = 0;
    static constexpr int KEYBOARD_VELOCITY = 1;
    static constexpr int KEYBOARD_NOTE     = 2;

    /**
     * @param pid       Patch ID
     * @param morph     Morph group (0-3)
     * @param keyboard  Keyboard assignment (0=disable, 1=velocity, 2=note)
     */
    MorphKeyboardAssignmentMessage(int pid, int morph, int keyboard);

    std::vector<uint8_t> toSysEx(int slot) const override;

private:
    int pid_;
    int morph_;
    int keyboard_;
};
