#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/Patch.h"
#include "../model/ModuleDescriptions.h"
#include "../model/ThemeData.h"

class PatchCanvas : public juce::Component
{
public:
    PatchCanvas();

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    void setPatch(Patch* p, const ModuleDescriptions* md, const ThemeData* td = nullptr);

    // Callback for parameter changes
    using ParameterChangeCallback = std::function<void(int section, int moduleId, int parameterId, int value)>;
    void setParameterChangeCallback(ParameterChangeCallback cb) { parameterChangeCallback = std::move(cb); }

    // Check if a specific parameter is currently being dragged by the user
    bool isDragging(int section, int moduleId, int parameterId) const;

    // Matches original Java editor: 255px per column, 15px per row
    static constexpr int gridX = 255;         // pixels per grid column (module width)
    static constexpr int gridY = 15;          // pixels per grid row (height unit)
    static constexpr int canvasWidth = 255 * 40;    // 40 columns visible
    static constexpr int canvasHeight = 15 * 256;   // 256 rows
    static constexpr int polyAreaOffsetY = 0;        // poly starts at y=0

private:
    void paintModules(juce::Graphics& g, const ModuleContainer& container, int yOffset);
    void paintCables(juce::Graphics& g, const ModuleContainer& container, int yOffset);
    juce::Rectangle<int> getModuleBounds(const Module& m, int yOffset) const;
    juce::Point<int> getConnectorPosition(const Module& m, const Connector& conn, int yOffset) const;

    // Theme-aware painting helpers
    void paintModuleThemed(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintModuleBackground(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintConnectors(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintLabels(juce::Graphics& g, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintKnobs(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintButtons(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintSliders(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintTextDisplays(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintLights(juce::Graphics& g, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintCustomDisplays(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds, const ModuleTheme& theme);
    void paintModuleFallback(juce::Graphics& g, const Module& m, juce::Rectangle<int> bounds);

    // Find parameter value by component-id (e.g. "p1")
    const Parameter* findParameter(const Module& m, const juce::String& componentId) const;
    // Find theme connector by component-id (e.g. "c1")
    const ThemeConnector* findThemeConnector(const ModuleTheme& theme, const juce::String& componentId) const;

    Patch* patch = nullptr;
    const ModuleDescriptions* moduleDescs = nullptr;
    const ThemeData* themeData = nullptr;

    // Dragging state
    struct DragState
    {
        enum Type { None, Knob, Slider, Button, ModuleMove, CableCreate } type = None;
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

    // Cable creation preview
    juce::Point<int> cablePreviewEnd;
    bool showCablePreview = false;

    // Connector hit-testing helpers
    struct ConnectorHit { Module* module = nullptr; Connector* connector = nullptr; int section = 0; };
    ConnectorHit findConnectorAt(juce::Point<int> pos);
    Connector* findConnectorByComponentId(Module& m, const juce::String& componentId);

    // Module overlap prevention
    bool isPositionFree(const ModuleContainer& container, const Module* exclude, int gx, int gy, int height) const;
    int findNearestFreeY(const ModuleContainer& container, const Module* exclude, int gx, int targetY, int height) const;

    static constexpr int paramSendIntervalMs = 50;  // Min interval between param sends during drag

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchCanvas)
};

class PatchCanvasComponent : public juce::Component
{
public:
    PatchCanvasComponent();

    void resized() override;

    void setPatch(Patch* p, const ModuleDescriptions* md, const ThemeData* td = nullptr);

    void setParameterChangeCallback(PatchCanvas::ParameterChangeCallback cb)
    {
        canvas.setParameterChangeCallback(std::move(cb));
    }

    void repaintCanvas() { canvas.repaint(); }

    bool isDragging(int section, int moduleId, int parameterId) const
    {
        return canvas.isDragging(section, moduleId, parameterId);
    }

private:
    juce::Viewport viewport;
    PatchCanvas canvas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchCanvasComponent)
};
