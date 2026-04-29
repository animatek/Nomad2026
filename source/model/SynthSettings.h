#pragma once

#include <string>

// Mirrors PDL2 patch.pdl2 / SynthSettings section (type=3) and the
// jnmprotocol2 SynthSettingsMessage parameter set.
struct SynthSettings
{
    // Core (always present)
    int  midiClockSource     = 0;    // 1 bit  (0=internal, 1=external)
    int  midiVelScaleMin     = 0;    // 7 bits (0-127)
    int  ledsActive          = 1;    // 1 bit
    int  midiVelScaleMax     = 127;  // 7 bits (0-127)
    int  midiClockBpm        = 120;  // 8 bits (raw, 0-255)
    int  localOn             = 1;    // 1 bit
    int  keyboardMode        = 0;    // 1 bit  (0=Active slot, 1=Selected slots)
    int  pedalPolarity       = 0;    // 1 bit
    int  globalSync          = 0;    // 5 bits
    int  masterTune          = 64;   // 8 bits (raw, 64=center)
    int  programChangeReceive= 1;    // 1 bit
    int  programChangeSend   = 1;    // 1 bit
    int  knobMode            = 0;    // 1 bit  (0=Immediate, 1=Hook)
    std::string name;                // up to 16 chars
    int  midiChannelSlot[4]  = {0, 1, 2, 3};   // 5 bits each (0-15 used)
    int  slotcount           = 4;    // 4 = Micro Modular, 1 = Nord Modular

    // Optional extended fields (parsed when present)
    bool hasExtended         = false;
    int  slotSelected[4]     = {1, 0, 0, 0};   // 1 bit each
    int  activeSlot          = 0;              // 2 bits
    int  slotVoiceCount[4]   = {4, 4, 4, 4};   // 8 bits each
};
