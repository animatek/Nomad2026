#include "PatchSerializer.h"
#include <juce_core/juce_core.h>

std::vector<std::vector<uint8_t>> PatchSerializer::serialize(const Patch& patch)
{
    std::vector<std::vector<uint8_t>> sections;
    sections.reserve(13);

    // Section 0: Header + PatchName2 (combined in one PatchPacket)
    sections.push_back(serializeHeaderAndName(patch));

    // Section 1: Poly ModuleDump
    sections.push_back(serializeModuleDump(1));

    // Section 2: Common ModuleDump
    sections.push_back(serializeModuleDump(0));

    // Section 3: Poly CableDump
    sections.push_back(serializeCableDump(1));

    // Section 4: Common CableDump
    sections.push_back(serializeCableDump(0));

    // Section 5: Poly ParameterDump
    sections.push_back(serializeParameterDump(1));

    // Section 6: Common ParameterDump
    sections.push_back(serializeParameterDump(0));

    // Section 7: MorphMap
    sections.push_back(serializeMorphMap(patch));

    // Section 8: KnobMap
    sections.push_back(serializeKnobMap(patch));

    // Section 9: ControlMap
    sections.push_back(serializeControlMap(patch));

    // Section 10: Poly NameDump (custom module names)
    sections.push_back(serializeNameDump(1));

    // Section 11: Common NameDump
    sections.push_back(serializeNameDump(0));

    // Section 12: NoteDump
    sections.push_back(serializeNoteDump(patch));

    DBG("PatchSerializer: serialized patch \"" + patch.getName() + "\" to "
        + juce::String(sections.size()) + " sections");

    return sections;
}

std::vector<uint8_t> PatchSerializer::serializeHeaderAndName(const Patch& patch)
{
    BitStreamWriter bs;

    // Header section (type 33)
    bs.writeBits(33, 8);  // section type

    const auto& h = patch.getHeader();
    bs.writeBits(h.keyRangeMin, 7);
    bs.writeBits(h.keyRangeMax, 7);
    bs.writeBits(h.velRangeMin, 7);
    bs.writeBits(h.velRangeMax, 7);
    bs.writeBits(h.bendRange, 5);
    bs.writeBits(h.portamentoTime, 7);
    bs.writeBits(h.portamento ? 1 : 0, 1);
    bs.writeBits(h.pedalMode, 1);
    bs.writeBits(h.voices - 1, 5);  // stored 0-based
    bs.writeBits(0, 2);  // unknown2
    bs.writeBits(h.separatorPosition, 12);
    bs.writeBits(h.octaveShift, 3);
    bs.writeBits(h.cableVisRed ? 1 : 0, 1);
    bs.writeBits(h.cableVisBlue ? 1 : 0, 1);
    bs.writeBits(h.cableVisYellow ? 1 : 0, 1);
    bs.writeBits(h.cableVisGray ? 1 : 0, 1);
    bs.writeBits(h.cableVisGreen ? 1 : 0, 1);
    bs.writeBits(h.cableVisPurple ? 1 : 0, 1);
    bs.writeBits(h.cableVisWhite ? 1 : 0, 1);
    bs.writeBits(h.voiceRetriggerCommon, 1);
    bs.writeBits(h.voiceRetriggerPoly, 1);
    bs.writeBits(0, 4);  // unknown3
    bs.writeBits(0, 3);  // unknown4

    bs.alignToByte();

    // PatchName2 section (type 39) - no padding
    bs.writeBits(39, 8);  // section type
    bs.writeString16(patch.getName().toStdString());
    bs.alignToByte();

    return bs.toMidiBytes();
}

std::vector<uint8_t> PatchSerializer::serializeModuleDump(int section)
{
    BitStreamWriter bs;

    // ModuleDump section (type 74)
    bs.writeBits(74, 8);  // section type
    bs.writeBits(section, 1);  // 0=common, 1=poly
    bs.writeBits(0, 7);  // count: 0 modules

    bs.alignToByte();
    return bs.toMidiBytes();
}

std::vector<uint8_t> PatchSerializer::serializeCableDump(int section)
{
    BitStreamWriter bs;

    // CableDump section (type 82)
    bs.writeBits(82, 8);  // section type
    bs.writeBits(section, 1);  // 0=common, 1=poly
    bs.writeBits(0, 9);  // count: 0 cables

    bs.alignToByte();
    return bs.toMidiBytes();
}

std::vector<uint8_t> PatchSerializer::serializeParameterDump(int section)
{
    BitStreamWriter bs;

    // ParameterDump section (type 77)
    bs.writeBits(77, 8);  // section type
    bs.writeBits(section, 1);  // 0=common, 1=poly
    bs.writeBits(0, 7);  // count: 0 parameters

    bs.alignToByte();
    return bs.toMidiBytes();
}

std::vector<uint8_t> PatchSerializer::serializeMorphMap(const Patch& patch)
{
    BitStreamWriter bs;

    // MorphMap section (type 101)
    bs.writeBits(101, 8);  // section type

    // 4 morphs: value, keyboard mapping
    for (int i = 0; i < 4; ++i)
    {
        bs.writeBits(patch.morphValues[i], 7);
        bs.writeBits(patch.morphKeyboard[i], 2);
    }

    // Morph assignments count
    bs.writeBits(patch.morphAssignments.size(), 8);

    // Write each assignment
    for (const auto& ma : patch.morphAssignments)
    {
        bs.writeBits(ma.section, 1);
        bs.writeBits(ma.module, 8);
        bs.writeBits(ma.param, 7);
        bs.writeBits(ma.morph, 2);
        bs.writeBits(ma.range, 8);  // signed 8-bit
    }

    bs.alignToByte();
    return bs.toMidiBytes();
}

std::vector<uint8_t> PatchSerializer::serializeKnobMap(const Patch& patch)
{
    BitStreamWriter bs;

    // KnobMapDump section (type 98)
    bs.writeBits(98, 8);  // section type

    // 23 knobs
    for (int i = 0; i < 23; ++i)
    {
        const auto& ka = patch.knobAssignments[i];
        if (ka.assigned)
        {
            bs.writeBits(1, 1);  // assigned
            bs.writeBits(ka.section, 1);
            bs.writeBits(ka.module, 8);
            bs.writeBits(ka.param, 7);
        }
        else
        {
            bs.writeBits(0, 1);  // not assigned
        }
    }

    bs.alignToByte();
    return bs.toMidiBytes();
}

std::vector<uint8_t> PatchSerializer::serializeControlMap(const Patch& patch)
{
    BitStreamWriter bs;

    // ControlMapDump section (type 96)
    bs.writeBits(96, 8);  // section type
    bs.writeBits(patch.ctrlAssignments.size(), 8);  // count

    for (const auto& ca : patch.ctrlAssignments)
    {
        bs.writeBits(ca.control, 8);
        bs.writeBits(ca.section, 1);
        bs.writeBits(ca.module, 8);
        bs.writeBits(ca.param, 7);
    }

    bs.alignToByte();
    return bs.toMidiBytes();
}

std::vector<uint8_t> PatchSerializer::serializeNameDump(int section)
{
    BitStreamWriter bs;

    // NameDump section (type 90) - custom module names
    bs.writeBits(90, 8);  // section type
    bs.writeBits(section, 1);  // 0=common, 1=poly
    bs.writeBits(0, 7);  // count: 0 custom names

    bs.alignToByte();
    return bs.toMidiBytes();
}

std::vector<uint8_t> PatchSerializer::serializeNoteDump(const Patch& patch)
{
    BitStreamWriter bs;

    // NoteDump section (type 105)
    bs.writeBits(105, 8);  // section type
    bs.writeBits(patch.notes.size(), 8);  // count

    for (const auto& note : patch.notes)
    {
        bs.writeBits(note.note, 8);
        bs.writeBits(note.attack, 8);
        bs.writeBits(note.release, 8);
    }

    bs.alignToByte();
    return bs.toMidiBytes();
}
