#include "PatchSynchronizer.h"
#include "../protocol/NewCableMessage.h"
#include "../protocol/DeleteCableMessage.h"
#include "../protocol/MoveModuleMessage.h"
#include <juce_core/juce_core.h>

PatchSynchronizer::PatchSynchronizer(Patch& patch, ConnectionManager& connMgr)
    : patch_(patch), connMgr_(connMgr)
{
    enable();
}

PatchSynchronizer::~PatchSynchronizer()
{
    disable();
}

void PatchSynchronizer::enable()
{
    if (enabled_)
        return;

    // Register cable event callbacks
    patch_.getPolyVoiceArea().setCableAddedCallback(
        [this](Connector* out, Connector* in) { onCableAdded(1, out, in); });
    patch_.getPolyVoiceArea().setCableRemovedCallback(
        [this](Connector* out, Connector* in) { onCableRemoved(1, out, in); });

    patch_.getCommonArea().setCableAddedCallback(
        [this](Connector* out, Connector* in) { onCableAdded(0, out, in); });
    patch_.getCommonArea().setCableRemovedCallback(
        [this](Connector* out, Connector* in) { onCableRemoved(0, out, in); });

    // Register module moved callbacks for all modules
    auto registerModuleMoved = [this](ModuleContainer& container, int section) {
        for (auto& modPtr : container.getModules())
        {
            modPtr->setModuleMovedCallback([this, section](Module* m, int oldX, int oldY) {
                onModuleMoved(section, m, oldX, oldY);
            });
        }
    };

    registerModuleMoved(patch_.getPolyVoiceArea(), 1);
    registerModuleMoved(patch_.getCommonArea(), 0);

    enabled_ = true;
    DBG("PatchSynchronizer enabled");
}

void PatchSynchronizer::disable()
{
    if (!enabled_)
        return;

    // Clear callbacks
    patch_.getPolyVoiceArea().setCableAddedCallback(nullptr);
    patch_.getPolyVoiceArea().setCableRemovedCallback(nullptr);
    patch_.getCommonArea().setCableAddedCallback(nullptr);
    patch_.getCommonArea().setCableRemovedCallback(nullptr);

    // Clear module callbacks
    for (auto& m : patch_.getPolyVoiceArea().getModules())
        m->setModuleMovedCallback(nullptr);
    for (auto& m : patch_.getCommonArea().getModules())
        m->setModuleMovedCallback(nullptr);

    enabled_ = false;
    DBG("PatchSynchronizer disabled");
}

void PatchSynchronizer::onCableAdded(int section, Connector* output, Connector* input)
{
    if (!enabled_ || !connMgr_.isConnected())
        return;

    auto& container = patch_.getContainer(section);

    // Find modules
    Module* outModule = findModuleForConnector(container, output);
    Module* inModule = findModuleForConnector(container, input);

    if (!outModule || !inModule)
    {
        DBG("PatchSynchronizer::onCableAdded - could not find modules");
        return;
    }

    // Get indices
    int outModIdx = outModule->getContainerIndex();
    int inModIdx = inModule->getContainerIndex();
    int outConnIdx = getConnectorIndex(*outModule, output);
    int inConnIdx = getConnectorIndex(*inModule, input);

    bool outIsOutput = output->getDescriptor()->isOutput;
    bool inIsOutput = input->getDescriptor()->isOutput;
    SignalType color = output->getDescriptor()->signalType;

    // Create and send message
    NewCableMessage msg(section, color,
                       outModIdx, outIsOutput, outConnIdx,
                       inModIdx, inIsOutput, inConnIdx);

    auto sysex = msg.toSysEx(connMgr_.getCurrentSlot());
    connMgr_.sendRawSysEx(sysex);

    // Debug logging with hex dump
    juce::String hexDump;
    for (auto byte : sysex)
        hexDump += juce::String::toHexString((int)byte).paddedLeft('0', 2) + " ";

    DBG("Sent CableInsert: section=" + juce::String(section)
        + " color=" + juce::String((int)color)
        + " modules=" + juce::String(outModIdx) + "->" + juce::String(inModIdx)
        + " types=" + juce::String(outIsOutput ? "out" : "in") + "->" + juce::String(inIsOutput ? "out" : "in")
        + " connectors=" + juce::String(outConnIdx) + "->" + juce::String(inConnIdx));
    DBG("  SysEx: " + hexDump);
}

void PatchSynchronizer::onCableRemoved(int section, Connector* output, Connector* input)
{
    if (!enabled_ || !connMgr_.isConnected())
        return;

    auto& container = patch_.getContainer(section);

    // Find modules
    Module* outModule = findModuleForConnector(container, output);
    Module* inModule = findModuleForConnector(container, input);

    if (!outModule || !inModule)
    {
        DBG("PatchSynchronizer::onCableRemoved - could not find modules");
        return;
    }

    // Get indices
    int outModIdx = outModule->getContainerIndex();
    int inModIdx = inModule->getContainerIndex();
    int outConnIdx = getConnectorIndex(*outModule, output);
    int inConnIdx = getConnectorIndex(*inModule, input);

    bool outIsOutput = output->getDescriptor()->isOutput;
    bool inIsOutput = input->getDescriptor()->isOutput;
    SignalType color = output->getDescriptor()->signalType;

    // Create and send message
    DeleteCableMessage msg(section, color,
                          outModIdx, outIsOutput, outConnIdx,
                          inModIdx, inIsOutput, inConnIdx);

    auto sysex = msg.toSysEx(connMgr_.getCurrentSlot());
    connMgr_.sendRawSysEx(sysex);

    // Debug logging with hex dump
    juce::String hexDump;
    for (auto byte : sysex)
        hexDump += juce::String::toHexString((int)byte).paddedLeft('0', 2) + " ";

    DBG("Sent CableDelete: section=" + juce::String(section)
        + " modules=" + juce::String(outModIdx) + "->" + juce::String(inModIdx)
        + " connectors=" + juce::String(outConnIdx) + "->" + juce::String(inConnIdx));
    DBG("  SysEx: " + hexDump);
}

void PatchSynchronizer::onModuleMoved(int section, Module* module, int oldX, int oldY)
{
    if (!enabled_ || !connMgr_.isConnected())
        return;

    int moduleIdx = module->getContainerIndex();
    auto pos = module->getPosition();

    MoveModuleMessage msg(section, moduleIdx, pos.x, pos.y);
    auto sysex = msg.toSysEx(connMgr_.getCurrentSlot());
    connMgr_.sendRawSysEx(sysex);

    // Debug logging with hex dump
    juce::String hexDump;
    for (auto byte : sysex)
        hexDump += juce::String::toHexString((int)byte).paddedLeft('0', 2) + " ";

    DBG("Sent ModuleMove: section=" + juce::String(section)
        + " module=" + juce::String(moduleIdx)
        + " pos=(" + juce::String(pos.x) + "," + juce::String(pos.y) + ")");
    DBG("  SysEx: " + hexDump);
}

int PatchSynchronizer::getModuleIndex(const ModuleContainer& container, const Module* m) const
{
    return m->getContainerIndex();
}

int PatchSynchronizer::getConnectorIndex(const Module& m, const Connector* c) const
{
    return c->getDescriptor()->index;
}

Module* PatchSynchronizer::findModuleForConnector(const ModuleContainer& container, Connector* conn) const
{
    for (auto& modPtr : container.getModules())
    {
        for (auto& c : modPtr->getConnectors())
        {
            if (&c == conn)
                return modPtr.get();
        }
    }
    return nullptr;
}
