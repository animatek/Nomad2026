#pragma once

#include "Descriptors.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <memory>
#include <array>

class Parameter
{
public:
    Parameter(const ParameterDescriptor& desc)
        : descriptor(&desc), value(desc.defaultValue) {}

    const ParameterDescriptor* getDescriptor() const { return descriptor; }

    int getValue() const { return value; }
    void setValue(int v) { value = juce::jlimit(descriptor->minValue, descriptor->maxValue, v); }

    int getMorphGroup() const { return morphGroup; }
    void setMorphGroup(int g) { morphGroup = g; }

private:
    const ParameterDescriptor* descriptor;
    int value;
    int morphGroup = -1;  // -1 = none
};

class Connector
{
public:
    Connector(const ConnectorDescriptor& desc) : descriptor(&desc) {}
    const ConnectorDescriptor* getDescriptor() const { return descriptor; }

private:
    const ConnectorDescriptor* descriptor;
};

class Light
{
public:
    Light(const LightDescriptor& desc) : descriptor(&desc) {}
    const LightDescriptor* getDescriptor() const { return descriptor; }

    int getValue() const { return value; }
    void setValue(int v) { value = v; }

private:
    const LightDescriptor* descriptor;
    int value = 0;
};

struct Connection
{
    Connector* output = nullptr;
    Connector* input = nullptr;
};

class Module
{
public:
    static std::unique_ptr<Module> createFromDescriptor(const ModuleDescriptor& desc);

    const ModuleDescriptor* getDescriptor() const { return descriptor; }

    const juce::String& getTitle() const { return title; }
    void setTitle(const juce::String& t) { title = t; }

    juce::Point<int> getPosition() const { return position; }
    void setPosition(juce::Point<int> p) { position = p; }

    const std::vector<Parameter>& getParameters() const { return parameters; }
    std::vector<Parameter>& getParameters() { return parameters; }

    const std::vector<Connector>& getConnectors() const { return connectors; }
    std::vector<Connector>& getConnectors() { return connectors; }

    const std::vector<Light>& getLights() const { return lights; }
    std::vector<Light>& getLights() { return lights; }

    Parameter* getParameter(int index);
    Connector* getConnector(int index);

    int getContainerIndex() const { return containerIndex; }
    void setContainerIndex(int idx) { containerIndex = idx; }

private:
    Module() = default;

    const ModuleDescriptor* descriptor = nullptr;
    juce::String title;
    juce::Point<int> position { 0, 0 };
    int containerIndex = 0;  // slot index within the voice area
    std::vector<Parameter> parameters;
    std::vector<Connector> connectors;
    std::vector<Light> lights;
};

class ModuleContainer
{
public:
    Module* addModule(std::unique_ptr<Module> module);
    void removeModule(Module* module);

    bool canAdd(const ModuleDescriptor& desc) const;

    void addConnection(Connector* output, Connector* input);
    void removeConnection(Connector* output, Connector* input);

    Module* getModuleByIndex(int containerIndex);
    const Module* getModuleByIndex(int containerIndex) const;

    const std::vector<std::unique_ptr<Module>>& getModules() const { return modules; }
    const std::vector<Connection>& getConnections() const { return connections; }

private:
    std::vector<std::unique_ptr<Module>> modules;
    std::vector<Connection> connections;
};

struct PatchHeader
{
    int keyRangeMin = 0;
    int keyRangeMax = 127;
    int velRangeMin = 0;
    int velRangeMax = 127;
    int bendRange = 2;
    int portamentoTime = 0;
    bool portamento = false;
    int pedalMode = 0;
    int voices = 1;
    int octaveShift = 0;
    // Cable visibility flags
    bool cableVisRed = true;
    bool cableVisBlue = true;
    bool cableVisYellow = true;
    bool cableVisGray = true;
    bool cableVisGreen = true;
    bool cableVisPurple = true;
    bool cableVisWhite = true;
    // Section separator position
    int separatorPosition = 0;
};

struct MorphAssignment
{
    int section = 0;   // 0=common, 1=poly
    int module = 0;
    int param = 0;
    int morph = 0;     // 0-3
    int range = 0;     // signed 8-bit
};

struct KnobAssignment
{
    bool assigned = false;
    int section = 0;
    int module = 0;
    int param = 0;
};

struct CtrlAssignment
{
    int control = 0;
    int section = 0;
    int module = 0;
    int param = 0;
};

struct NoteSlot
{
    int note = 0;
    int attack = 0;
    int release = 0;
};

class Patch
{
public:
    Patch();

    const juce::String& getName() const { return name; }
    void setName(const juce::String& n) { name = n; }

    PatchHeader& getHeader() { return header; }
    const PatchHeader& getHeader() const { return header; }

    ModuleContainer& getPolyVoiceArea() { return polyVoiceArea; }
    ModuleContainer& getCommonArea() { return commonArea; }

    const ModuleContainer& getPolyVoiceArea() const { return polyVoiceArea; }
    const ModuleContainer& getCommonArea() const { return commonArea; }

    // PDL2 convention: section 0 = common, section 1 = poly
    ModuleContainer& getContainer(int section) { return section == 1 ? polyVoiceArea : commonArea; }

    // Morph map
    std::array<int, 4> morphValues = { 0, 0, 0, 0 };
    std::array<int, 4> morphKeyboard = { 0, 0, 0, 0 };
    std::vector<MorphAssignment> morphAssignments;

    // Knob map (23 knobs)
    std::array<KnobAssignment, 23> knobAssignments;

    // Control map (MIDI CC assignments)
    std::vector<CtrlAssignment> ctrlAssignments;

    // Custom parameter dump data (per-module custom values)
    // Stored as module containerIndex -> vector of parameter values
    struct CustomDumpEntry { int index; std::vector<int> values; };
    std::vector<CustomDumpEntry> polyCustomDump;
    std::vector<CustomDumpEntry> commonCustomDump;

    // Notes
    std::vector<NoteSlot> notes;

private:
    juce::String name { "Init Patch" };
    PatchHeader header;
    ModuleContainer polyVoiceArea;
    ModuleContainer commonArea;
};
