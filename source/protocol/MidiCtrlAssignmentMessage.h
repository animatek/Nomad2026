#pragma once
#include <vector>
#include <cstdint>

/**
 * MidiCtrlAssignment / MidiCtrlAssignmentChange messages (cc=0x17)
 *
 * New assignment (sc=0x22):
 *   MidiCtrlAssignment := 0:1 section:7 0:1 module:7 0:1 parameter:7 0:1 midictrl:7
 *
 * Change/deassign (sc=0x23):
 *   MidiCtrlAssignmentChange := 0:1 prevmidictrl:7 ?NewMidiCtrlAssignmentPacket
 *   NewMidiCtrlAssignmentPacket := 0:1 0x22:7 MidiCtrlAssignment
 */
class MidiCtrlAssignmentMessage
{
public:
    // New assignment
    static std::vector<uint8_t> assign(int pid, int midiCtrl, int section, int module, int param, int slot);

    // Re-assignment
    static std::vector<uint8_t> reassign(int pid, int prevCtrl, int newCtrl, int section, int module, int param, int slot);

    // Deassign
    static std::vector<uint8_t> deassign(int pid, int prevCtrl, int slot);
};
