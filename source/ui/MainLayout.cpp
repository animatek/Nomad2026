#include "MainLayout.h"

MainLayout::MainLayout(ModuleDescriptions& /*moduleDescs*/)
{
    // Slot tabs (1-4)
    auto tabColour = juce::Colour(0xff2a2a4a);
    slotTabs.addTab("Slot 1", tabColour, -1);
    slotTabs.addTab("Slot 2", tabColour, -1);
    slotTabs.addTab("Slot 3", tabColour, -1);
    slotTabs.addTab("Slot 4", tabColour, -1);
    slotTabs.setColour(juce::TabbedButtonBar::tabOutlineColourId,  juce::Colour(0xff333355));
    slotTabs.setColour(juce::TabbedButtonBar::frontOutlineColourId, juce::Colour(0xff5555aa));

    addAndMakeVisible(slotTabs);
    addAndMakeVisible(headerBar);
    addAndMakeVisible(inspectorPanel);
    addAndMakeVisible(canvasComponent);
    addAndMakeVisible(patchBrowserPanel);
    addAndMakeVisible(statusBar);
    addAndMakeVisible(resizerBar1);
    addAndMakeVisible(resizerBar2);

    // Layout: [inspector | bar | canvas | bar | patchBrowser]
    layoutManager.setItemLayout(0, 150, 350, 210);   // inspector (left)
    layoutManager.setItemLayout(1, 4, 4, 4);          // resizer
    layoutManager.setItemLayout(2, 200, -1.0, -0.6);  // canvas (most space)
    layoutManager.setItemLayout(3, 4, 4, 4);          // resizer
    layoutManager.setItemLayout(4, 150, 400, 220);    // patch browser (right)
}

void MainLayout::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));
}

void MainLayout::resized()
{
    auto area = getLocalBounds();

    statusBar.setBounds(area.removeFromBottom(statusBarHeight));
    slotTabs.setBounds(area.removeFromTop(slotTabHeight));
    headerBar.setBounds(area.removeFromTop(headerBarHeight));

    juce::Component* comps[] = {
        &inspectorPanel, &resizerBar1, &canvasComponent, &resizerBar2, &patchBrowserPanel
    };
    layoutManager.layOutComponents(comps, 5,
                                   area.getX(), area.getY(),
                                   area.getWidth(), area.getHeight(),
                                   false, true);
}
