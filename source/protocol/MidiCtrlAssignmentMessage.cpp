#include "MidiCtrlAssignmentMessage.h"

static std::vector<uint8_t> buildSysEx(int cc, int slot, int pid, const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> msg;
    msg.push_back(0xF0);
    msg.push_back(0x33);
    msg.push_back(static_cast<uint8_t>(((cc & 0x1F) << 2) | (slot & 0x03)));
    msg.push_back(0x06);
    msg.push_back(static_cast<uint8_t>(pid & 0x7F));

    msg.insert(msg.end(), payload.begin(), payload.end());

    uint32_t sum = 0;
    for (auto b : msg) sum += b;
    msg.push_back(static_cast<uint8_t>(sum % 128));
    msg.push_back(0xF7);
    return msg;
}

// sc=0x22: MidiCtrlAssignment := 0:1 section:7 0:1 module:7 0:1 parameter:7 0:1 midictrl:7
std::vector<uint8_t> MidiCtrlAssignmentMessage::assign(int pid, int midiCtrl, int section, int module, int param, int slot)
{
    std::vector<uint8_t> payload;
    payload.push_back(0x22);  // sc
    payload.push_back(static_cast<uint8_t>(section & 0x7F));
    payload.push_back(static_cast<uint8_t>(module & 0x7F));
    payload.push_back(static_cast<uint8_t>(param & 0x7F));
    payload.push_back(static_cast<uint8_t>(midiCtrl & 0x7F));
    return buildSysEx(0x17, slot, pid, payload);
}

std::vector<uint8_t> MidiCtrlAssignmentMessage::reassign(int pid, int prevCtrl, int newCtrl, int section, int module, int param, int slot)
{
    std::vector<uint8_t> payload;
    payload.push_back(0x23);  // sc
    payload.push_back(static_cast<uint8_t>(prevCtrl & 0x7F));
    // NewMidiCtrlAssignmentPacket
    payload.push_back(0x22);  // marker
    payload.push_back(static_cast<uint8_t>(section & 0x7F));
    payload.push_back(static_cast<uint8_t>(module & 0x7F));
    payload.push_back(static_cast<uint8_t>(param & 0x7F));
    payload.push_back(static_cast<uint8_t>(newCtrl & 0x7F));
    return buildSysEx(0x17, slot, pid, payload);
}

std::vector<uint8_t> MidiCtrlAssignmentMessage::deassign(int pid, int prevCtrl, int slot)
{
    std::vector<uint8_t> payload;
    payload.push_back(0x23);  // sc
    payload.push_back(static_cast<uint8_t>(prevCtrl & 0x7F));
    return buildSysEx(0x17, slot, pid, payload);
}
