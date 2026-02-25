#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ModuleBrowserPanel.h"
#include "PatchCanvasComponent.h"
#include "InspectorPanel.h"
#include "StatusBar.h"
#include "../model/ModuleDescriptions.h"

class MainLayout : public juce::Component
{
public:
    MainLayout(ModuleDescriptions& moduleDescs);

    void paint(juce::Graphics& g) override;
    void resized() override;

    ModuleBrowserPanel& getBrowser() { return browserPanel; }
    PatchCanvasComponent& getCanvas() { return canvasComponent; }
    InspectorPanel& getInspector() { return inspectorPanel; }
    StatusBar& getStatusBar() { return statusBar; }

private:
    ModuleBrowserPanel browserPanel;
    PatchCanvasComponent canvasComponent;
    InspectorPanel inspectorPanel;
    StatusBar statusBar;

    // Slot tabs
    juce::TabbedButtonBar slotTabs { juce::TabbedButtonBar::TabsAtTop };

    // Stretchable layout for the 3 panels
    juce::StretchableLayoutManager layoutManager;
    juce::StretchableLayoutResizerBar resizerBar1 { &layoutManager, 1, true };
    juce::StretchableLayoutResizerBar resizerBar2 { &layoutManager, 3, true };

    static constexpr int statusBarHeight = 24;
    static constexpr int slotTabHeight = 28;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainLayout)
};
