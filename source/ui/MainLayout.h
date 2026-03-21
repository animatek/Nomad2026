#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PatchCanvasComponent.h"
#include "InspectorPanel.h"
#include "PatchBrowserPanel.h"
#include "StatusBar.h"
#include "PatchHeaderBar.h"
#include "../model/ModuleDescriptions.h"

class MainLayout : public juce::Component,
                   public juce::DragAndDropContainer
{
public:
    MainLayout(ModuleDescriptions& moduleDescs);

    void paint(juce::Graphics& g) override;
    void resized() override;

    PatchCanvasComponent& getCanvas()      { return canvasComponent; }
    InspectorPanel&       getInspector()   { return inspectorPanel; }
    PatchBrowserPanel&    getPatchBrowser() { return patchBrowserPanel; }
    StatusBar&            getStatusBar()   { return statusBar; }
    PatchHeaderBar&       getHeaderBar()   { return headerBar; }

private:
    InspectorPanel    inspectorPanel;     // left  — module parameter inspector
    PatchCanvasComponent canvasComponent; // centre — patch canvas
    PatchBrowserPanel patchBrowserPanel;  // right  — synth patch browser
    StatusBar         statusBar;
    PatchHeaderBar    headerBar;

    // Slot tabs
    juce::TabbedButtonBar slotTabs { juce::TabbedButtonBar::TabsAtTop };

    // Stretchable layout: [inspector | bar | canvas | bar | patchBrowser]
    juce::StretchableLayoutManager layoutManager;
    juce::StretchableLayoutResizerBar resizerBar1 { &layoutManager, 1, true };
    juce::StretchableLayoutResizerBar resizerBar2 { &layoutManager, 3, true };

    static constexpr int statusBarHeight = 24;
    static constexpr int slotTabHeight   = 28;
    static constexpr int headerBarHeight = 48;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainLayout)
};
