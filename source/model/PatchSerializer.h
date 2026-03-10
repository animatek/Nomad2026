#pragma once

#include "Patch.h"
#include "BitStreamWriter.h"
#include <vector>
#include <cstdint>
#include <string>

/**
 * PatchSerializer - Serializes a Patch object to 7-bit MIDI binary format.
 *
 * CURRENT LIMITATION: Only serializes "Init" patches (empty patches with default settings).
 * This is sufficient for Delete operation and basic Rename (though rename loses patch content).
 *
 * Full serialization (preserving modules, cables, parameters) requires implementing all
 * section serializers which is ~400 lines of code.
 */
class PatchSerializer
{
public:
    PatchSerializer() = default;

    /**
     * Serialize a patch to the 13 sections required by the Nord Modular protocol.
     *
     * LIMITATION: Currently only serializes empty/init patches. All modules, cables,
     * and parameters are discarded. Only the patch name and basic header settings
     * are preserved.
     *
     * @param patch The patch to serialize
     * @return Vector of 13 sections (7-bit MIDI encoded binary data)
     */
    std::vector<std::vector<uint8_t>> serialize(const Patch& patch);

private:
    // Serialize individual sections
    std::vector<uint8_t> serializeHeaderAndName(const Patch& patch);
    std::vector<uint8_t> serializeModuleDump(int section);  // section: 0=common, 1=poly
    std::vector<uint8_t> serializeCableDump(int section);
    std::vector<uint8_t> serializeParameterDump(int section);
    std::vector<uint8_t> serializeMorphMap(const Patch& patch);
    std::vector<uint8_t> serializeKnobMap(const Patch& patch);
    std::vector<uint8_t> serializeControlMap(const Patch& patch);
    std::vector<uint8_t> serializeNameDump(int section);  // Custom module names (currently empty)
    std::vector<uint8_t> serializeNoteDump(const Patch& patch);
};
