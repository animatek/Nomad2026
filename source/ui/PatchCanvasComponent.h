#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/Patch.h"
#include "../model/ModuleDescriptions.h"
#include "../model/ThemeData.h"
#include "QuickAddPopup.h"
#include <set>
#include <vector>
#include <map>
#include <random>

class PatchCanvas : public juce::Component,
                     public juce::DragAndDropTarget
{
public:
    // Callback types
    using ParameterChangeCallback = std::function<void(int section, int moduleId, int parameterId, int value)>;
    using ModuleDropCallback = std::function<void(int typeId, int section, int gridX, int gridY, const juce::String& name)>;
    using DeleteModuleCallback = std::function<void(int section, Module* module)>;
    using RenameModuleCallback = std::function<void(int section, Module* module, const juce::String& newName)>;
    using ModuleSelectedCallback = std::function<void(Module* module, int section)>;
    // section, moduleId, paramId, morphGroup (0-3, or -1=disable)
    using MorphAssignCallback = std::function<void(int section, int moduleId, int paramId, int morphGroup)>;
    // section, moduleId, paramId, span, direction
    using MorphRangeChangeCallback = std::function<void(int section, int moduleId, int paramId, int span, int direction)>;
    // section, moduleId, paramId, knobIndex (0-22, or -1=disable)
    using KnobAssignCallback = std::function<void(int section, int moduleId, int paramId, int knobIndex)>;
    // section, moduleId, paramId, midiCC (0-127, or -1=disable)
    using MidiCtrlAssignCallback = std::function<void(int section, int moduleId, int paramId, int midiCC)>;

    PatchCanvas();
    ~PatchCanvas();

    void shakeCables();

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;

    void setPatch(Patch* p, const ModuleDescriptions* md, const ThemeData* td = nullptr);

    // Callbacks
    void setParameterChangeCallback(ParameterChangeCallback cb) { parameterChangeCallback = std::move(cb); }
    void setModuleDropCallback(ModuleDropCallback cb) { moduleDropCallback = std::move(cb); }
    void setDeleteModuleCallback(DeleteModuleCallback cb) { deleteModuleCallback = std::move(cb); }
    void setRenameModuleCallback(RenameModuleCallback cb) { renameModuleCallback = std::move(cb); }
    void setModuleSelectedCallback(ModuleSelectedCallback cb) { moduleSelectedCallback = std::move(cb); }
    void setMorphAssignCallback(MorphAssignCallback cb) { morphAssignCallback = std::move(cb); }
    void setMorphRangeChangeCallback(MorphRangeChangeCallback cb) { morphRangeChangeCallback = std::move(cb); }
    void setKnobAssignCallback(KnobAssignCallback cb) { knobAssignCallback = std::move(cb); }
    void setMidiCtrlAssignCallback(MidiCtrlAssignCallback cb) { midiCtrlAssignCallback = std::move(cb); }

    // DragAndDropTarget interface
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragMove(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    // Check if a specific parameter is currently being dragged by the user
    bool isDragging(int section, int moduleId, int parameterId) const;

    // Matches original Java editor: 255px per column, 15px per row
    static constexpr int gridX = 255;               // pixels per grid column (module width)
    static constexpr int gridY = 15;                // pixels per grid row (height unit)
    static constexpr int canvasWidth = 255 * 40;    // 40 columns
    static constexpr int canvasHeight = 15 * 256;   // legacy full height (unused when section-split)
    static constexpr int sectionHeight = 15 * 128;  // height of one voice area (128 rows)
    static constexpr int polyAreaOffsetY = 0;

    // Set which section this canvas renders (1=poly, 0=common).
    // Must be called before setPatch(). Resizes canvas to sectionHeight.
    void setSection(int s);

private:
    void paintModules(juce::Graphics& g, const ModuleContainer& container, int yOffset);
    void paintCables(juce::Graphics& g, const ModuleContainer& container, int yOffset);
    juce::Rectangle<int> getModuleBounds(const Module& m, int yOffset) const;
    juce::Point<int> getConnectorPosition(const Module& m, const Connector& conn, int yOffset) const;

    // Theme-aware painting helpers
    void paintModuleThemed(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme, const ModuleContainer& container);
    void paintModuleBackground(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintConnectors(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme, const ModuleContainer& container);
    void paintLabels(juce::Graphics& g, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintKnobs(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintButtons(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintSliders(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintTextDisplays(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintLights(juce::Graphics& g, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintCustomDisplays(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintModuleFallback(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds);

    // Find parameter value by component-id (e.g. "p1")
    // Non-const overload returns mutable Parameter* (used in mouseDown drag setup)
    Parameter* findParameter(Module& m, const juce::String& componentId);
    const Parameter* findParameter(const Module& m, const juce::String& componentId) const;
    // Find theme connector by component-id (e.g. "c1")
    const ThemeConnector* findThemeConnector(const ModuleTheme& theme, const juce::String& componentId) const;

    Patch* patch = nullptr;
    const ModuleDescriptions* moduleDescs = nullptr;
    const ThemeData* themeData = nullptr;
    int mySection = -1;  // -1=both (legacy), 0=common, 1=poly

    // Dragging state
    struct DragState
    {
        enum Type { None, Knob, Slider, Button, ModuleMove, MultiModuleMove, CableCreate, RubberBand, MorphRange } type = None;
        Module* module = nullptr;
        Parameter* parameter = nullptr;
        Connector* sourceConnector = nullptr;  // CableCreate: source connector
        int section = 0;  // 0=common, 1=poly (PDL2/Java convention)
        juce::Point<int> startPos;
        int startValue = 0;
        int lastSentValue = -1;  // Track last value sent to avoid duplicates
        juce::int64 lastSendTime = 0;  // Rate limiting for real-time sends
        int dragOffsetX = 0, dragOffsetY = 0;  // ModuleMove: pixel offset from module origin
    };
    DragState dragState;
    ParameterChangeCallback parameterChangeCallback;
    ModuleDropCallback moduleDropCallback;
    DeleteModuleCallback deleteModuleCallback;
    RenameModuleCallback renameModuleCallback;
    ModuleSelectedCallback moduleSelectedCallback;
    MorphAssignCallback morphAssignCallback;
    MorphRangeChangeCallback morphRangeChangeCallback;
    KnobAssignCallback knobAssignCallback;
    MidiCtrlAssignCallback midiCtrlAssignCallback;

    // Module drop preview
    bool showModuleDropPreview = false;
    int dropPreviewTypeId = 0;
    int dropPreviewSection = 0;
    int dropPreviewGridX = 0;
    int dropPreviewGridY = 0;

    // Cable creation preview
    juce::Point<int> cablePreviewEnd;
    bool showCablePreview = false;

    // Multi-module selection
    struct SelectedModule { Module* module = nullptr; int section = 0; };
    std::vector<SelectedModule> selection;
    // For multi-move: store initial positions of all selected modules
    struct ModuleMoveState { Module* module = nullptr; int section = 0; juce::Point<int> startGridPos; };
    std::vector<ModuleMoveState> multiMoveState;

    // Rubber-band selection rect (pixel coords, during drag)
    juce::Rectangle<int> rubberBandRect;
    bool showRubberBand = false;

    // Clipboard for copy/paste
    struct ClipboardEntry
    {
        int typeIndex = 0;
        juce::String name;
        int section = 0;
        juce::Point<int> gridPos;
        std::vector<int> paramValues;
        // Cable: (srcClipIdx, srcConnIdx, dstClipIdx, dstConnIdx)
        // stored separately below
    };
    struct ClipboardCable
    {
        int srcModuleClipIdx = 0;   // index into clipboard entries
        int srcConnectorIdx = 0;    // connector index within source module
        int dstModuleClipIdx = 0;
        int dstConnectorIdx = 0;
    };
    std::vector<ClipboardEntry> clipboard;
    std::vector<ClipboardCable> clipboardCables;

    // Parameter context menu (right-click on knob/slider/button)
    void showParameterContextMenu(Module& m, int section, Parameter& param);

    // Selection helpers
    bool isSelected(const Module* m) const;
    void clearSelection();
    void selectModule(Module* m, int section, bool addToSelection = false);
    void updateRubberBandSelection(juce::Rectangle<int> rect);
    void showSelectionContextMenu();
    void deleteSelection();
    void duplicateSelection(bool withCables);
    void copySelectionToClipboard();
    void pasteFromClipboard(juce::Point<int> mousePos);

    // Legacy single-module selection (kept for compatibility during move)
    Module* selectedModule = nullptr;
    int selectedSection = -1;

    // Connector hit-testing helpers
    struct ConnectorHit { Module* module = nullptr; Connector* connector = nullptr; int section = 0; };
    ConnectorHit findConnectorAt(juce::Point<int> pos);
    Connector* findConnectorByComponentId(Module& m, const juce::String& componentId);

    // Module overlap prevention
    bool isPositionFree(const ModuleContainer& container, const Module* exclude, int gx, int gy, int height) const;
    int findNearestFreeY(const ModuleContainer& container, const Module* exclude, int gx, int targetY, int height) const;

    static constexpr int paramSendIntervalMs = 50;  // Min interval between param sends during drag

    QuickAddPopup* activeQuickAdd = nullptr;  // nullptr when no popup open

    // Cable shake: per-connection random sag offsets for visual redistribution
    std::map<std::pair<const Connector*, const Connector*>, float> cableSagOffsets;

    // Check if a connector has a hidden (filtered) cable attached
    bool hasHiddenCable(const Connector& conn, const ModuleContainer& container) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchCanvas)
};

class PatchCanvasComponent : public juce::Component
{
public:
    PatchCanvasComponent();

    void resized() override;

    void setPatch(Patch* p, const ModuleDescriptions* md, const ThemeData* td = nullptr);

    // --- Callbacks forwarded to both section canvases ---

    void setParameterChangeCallback(PatchCanvas::ParameterChangeCallback cb)
    {
        polyCanvas.setParameterChangeCallback(cb);
        commonCanvas.setParameterChangeCallback(std::move(cb));
    }

    void setModuleDropCallback(PatchCanvas::ModuleDropCallback cb)
    {
        polyCanvas.setModuleDropCallback(cb);
        commonCanvas.setModuleDropCallback(std::move(cb));
    }

    void setDeleteModuleCallback(PatchCanvas::DeleteModuleCallback cb)
    {
        polyCanvas.setDeleteModuleCallback(cb);
        commonCanvas.setDeleteModuleCallback(std::move(cb));
    }

    void setRenameModuleCallback(PatchCanvas::RenameModuleCallback cb)
    {
        polyCanvas.setRenameModuleCallback(cb);
        commonCanvas.setRenameModuleCallback(std::move(cb));
    }

    void setModuleSelectedCallback(PatchCanvas::ModuleSelectedCallback cb)
    {
        polyCanvas.setModuleSelectedCallback(cb);
        commonCanvas.setModuleSelectedCallback(std::move(cb));
    }

    void setMorphAssignCallback(PatchCanvas::MorphAssignCallback cb)
    {
        polyCanvas.setMorphAssignCallback(cb);
        commonCanvas.setMorphAssignCallback(std::move(cb));
    }

    void setMorphRangeChangeCallback(PatchCanvas::MorphRangeChangeCallback cb)
    {
        polyCanvas.setMorphRangeChangeCallback(cb);
        commonCanvas.setMorphRangeChangeCallback(std::move(cb));
    }

    void setKnobAssignCallback(PatchCanvas::KnobAssignCallback cb)
    {
        polyCanvas.setKnobAssignCallback(cb);
        commonCanvas.setKnobAssignCallback(std::move(cb));
    }

    void setMidiCtrlAssignCallback(PatchCanvas::MidiCtrlAssignCallback cb)
    {
        polyCanvas.setMidiCtrlAssignCallback(cb);
        commonCanvas.setMidiCtrlAssignCallback(std::move(cb));
    }

    void repaintCanvas()
    {
        polyCanvas.repaint();
        commonCanvas.repaint();
    }

    void shakeCables()
    {
        polyCanvas.shakeCables();
        commonCanvas.shakeCables();
    }

    bool isDragging(int section, int moduleId, int parameterId) const
    {
        return polyCanvas.isDragging(section, moduleId, parameterId)
            || commonCanvas.isDragging(section, moduleId, parameterId);
    }

private:
    // Poly (section 1) area — top panel
    juce::Viewport polyViewport;
    PatchCanvas polyCanvas;

    // Common (section 0) area — bottom panel
    juce::Viewport commonViewport;
    PatchCanvas commonCanvas;

    // Draggable divider between the two panels
    juce::StretchableLayoutManager layout;
    juce::StretchableLayoutResizerBar resizerBar { &layout, 1, false };

    static constexpr int labelHeight = 18;   // "POLY" / "COMMON" header strip
    static constexpr int resizerThick = 6;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchCanvasComponent)
};
