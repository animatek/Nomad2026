#pragma once

#include "Descriptors.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <memory>

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

private:
    Module() = default;

    const ModuleDescriptor* descriptor = nullptr;
    juce::String title;
    juce::Point<int> position { 0, 0 };
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

    const std::vector<std::unique_ptr<Module>>& getModules() const { return modules; }
    const std::vector<Connection>& getConnections() const { return connections; }

private:
    std::vector<std::unique_ptr<Module>> modules;
    std::vector<Connection> connections;
};

class Patch
{
public:
    Patch();

    const juce::String& getName() const { return name; }
    void setName(const juce::String& n) { name = n; }

    ModuleContainer& getPolyVoiceArea() { return polyVoiceArea; }
    ModuleContainer& getCommonArea() { return commonArea; }

    const ModuleContainer& getPolyVoiceArea() const { return polyVoiceArea; }
    const ModuleContainer& getCommonArea() const { return commonArea; }

    ModuleContainer& getContainer(int section) { return section == 0 ? polyVoiceArea : commonArea; }

private:
    juce::String name { "Init Patch" };
    ModuleContainer polyVoiceArea;
    ModuleContainer commonArea;
};
