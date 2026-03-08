#pragma once

#include "../model/Patch.h"
#include "../midi/ConnectionManager.h"

/**
 * PatchSynchronizer monitors a Patch object for changes and sends
 * corresponding SysEx messages to the synthesizer in real-time.
 *
 * This implements the "live editing" behavior where every cable add/delete,
 * module move, etc. is immediately reflected on the synth.
 *
 * NOTE: This does NOT save to permanent flash - use StorePatchMessage for that.
 */
class PatchSynchronizer
{
public:
    PatchSynchronizer(Patch& patch, ConnectionManager& connMgr);
    ~PatchSynchronizer();

    void enable();
    void disable();
    bool isEnabled() const { return enabled_; }

private:
    // Event handlers
    void onCableAdded(int section, Connector* output, Connector* input);
    void onCableRemoved(int section, Connector* output, Connector* input);
    void onModuleMoved(int section, Module* module, int oldX, int oldY);

    // Helper: find module's container index
    int getModuleIndex(const ModuleContainer& container, const Module* m) const;

    // Helper: find connector's index within its module
    int getConnectorIndex(const Module& m, const Connector* c) const;

    // Helper: find module that owns a connector
    Module* findModuleForConnector(const ModuleContainer& container, Connector* conn) const;

    Patch& patch_;
    ConnectionManager& connMgr_;
    bool enabled_ = false;
};
