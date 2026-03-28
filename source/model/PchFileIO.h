#pragma once

#include "Patch.h"
#include "ModuleDescriptions.h"
#include <juce_core/juce_core.h>
#include <memory>

class PchFileIO
{
public:
    explicit PchFileIO(const ModuleDescriptions& descs);

    std::unique_ptr<Patch> readFile(const juce::File& file);
    bool writeFile(const Patch& patch, const juce::File& file);

private:
    // Reader helpers
    void parseHeader(const juce::StringArray& lines, Patch& patch);
    void parseModuleDump(const juce::StringArray& lines, Patch& patch);
    void parseCurrentNoteDump(const juce::StringArray& lines, Patch& patch);
    void parseCableDump(const juce::StringArray& lines, Patch& patch);
    void parseParameterDump(const juce::StringArray& lines, Patch& patch);
    void parseMorphMapDump(const juce::StringArray& lines, Patch& patch);
    void parseKeyboardAssignment(const juce::StringArray& lines, Patch& patch);
    void parseKnobMapDump(const juce::StringArray& lines, Patch& patch);
    void parseCtrlMapDump(const juce::StringArray& lines, Patch& patch);
    void parseCustomDump(const juce::StringArray& lines, Patch& patch);
    void parseNameDump(const juce::StringArray& lines, Patch& patch);

    // Writer helpers
    void writeHeader(juce::String& out, const Patch& patch);
    void writeModuleDump(juce::String& out, const ModuleContainer& container, int voiceAreaId);
    void writeCurrentNoteDump(juce::String& out, const Patch& patch);
    void writeCableDump(juce::String& out, const ModuleContainer& container, int voiceAreaId);
    void writeParameterDump(juce::String& out, const ModuleContainer& container, int voiceAreaId);
    void writeMorphMapDump(juce::String& out, const Patch& patch);
    void writeKeyboardAssignment(juce::String& out, const Patch& patch);
    void writeKnobMapDump(juce::String& out, const Patch& patch);
    void writeCtrlMapDump(juce::String& out, const Patch& patch);
    void writeCustomDump(juce::String& out, const Patch& patch, const ModuleContainer& container, int voiceAreaId);
    void writeNameDump(juce::String& out, const ModuleContainer& container, int voiceAreaId);
    void writeNotes(juce::String& out, const Patch& patch);

    static juce::StringArray tokenize(const juce::String& line);

    const ModuleDescriptions& descs;
};
