#include "MainLayout.h"

MainLayout::MainLayout(ModuleDescriptions& moduleDescs)
{
    // Set up module browser
    browserPanel.setModuleDescriptions(&moduleDescs);

    // Slot tabs (1-4)
    auto tabColour = juce::Colour(0xff2a2a4a);
    slotTabs.addTab("Slot 1", tabColour, -1);
    slotTabs.addTab("Slot 2", tabColour, -1);
    slotTabs.addTab("Slot 3", tabColour, -1);
    slotTabs.addTab("Slot 4", tabColour, -1);
    slotTabs.setColour(juce::TabbedButtonBar::tabOutlineColourId, juce::Colour(0xff333355));
    slotTabs.setColour(juce::TabbedButtonBar::frontOutlineColourId, juce::Colour(0xff5555aa));

    addAndMakeVisible(slotTabs);
    addAndMakeVisible(headerBar);
    addAndMakeVisible(browserPanel);
    addAndMakeVisible(canvasComponent);
    addAndMakeVisible(inspectorPanel);
    addAndMakeVisible(statusBar);
    addAndMakeVisible(resizerBar1);
    addAndMakeVisible(resizerBar2);

    // Layout: [browser | bar | canvas | bar | inspector]
    // Items: 0=browser, 1=resizer, 2=canvas, 3=resizer, 4=inspector
    layoutManager.setItemLayout(0, 150, 400, 220);   // browser panel
    layoutManager.setItemLayout(1, 4, 4, 4);          // resizer bar
    layoutManager.setItemLayout(2, 200, -1.0, -0.6);  // canvas (gets most space)
    layoutManager.setItemLayout(3, 4, 4, 4);          // resizer bar
    layoutManager.setItemLayout(4, 150, 400, 220);    // inspector panel
}

void MainLayout::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));
}

void MainLayout::resized()
{
    auto area = getLocalBounds();

    // Status bar at bottom
    statusBar.setBounds(area.removeFromBottom(statusBarHeight));

    // Slot tabs above the main panels
    slotTabs.setBounds(area.removeFromTop(slotTabHeight));

    // Patch header bar below slot tabs
    headerBar.setBounds(area.removeFromTop(headerBarHeight));

    // Lay out the 3 panels + 2 resizer bars
    juce::Component* comps[] = {
        &browserPanel, &resizerBar1, &canvasComponent, &resizerBar2, &inspectorPanel
    };
    layoutManager.layOutComponents(comps, 5,
                                   area.getX(), area.getY(),
                                   area.getWidth(), area.getHeight(),
                                   false, true);
}
