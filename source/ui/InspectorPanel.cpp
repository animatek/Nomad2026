#include "InspectorPanel.h"

InspectorPanel::InspectorPanel()
{
    // Status label for loading state
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    statusLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    addAndMakeVisible(statusLabel);

    // TreeView for patch browser
    treeView = std::make_unique<juce::TreeView>();
    treeView->setColour(juce::TreeView::backgroundColourId, juce::Colour(0xff1e1e3a));
    treeView->setColour(juce::TreeView::linesColourId, juce::Colour(0xff3a3a5a));
    treeView->setDefaultOpenness(false);  // Banks start collapsed
    treeView->setIndentSize(20);
    addAndMakeVisible(*treeView);

    setLoadingState(false);
}

void InspectorPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e3a));
}

void InspectorPanel::resized()
{
    auto bounds = getLocalBounds();

    if (isLoading || !rootItem)
    {
        statusLabel.setBounds(bounds);
        treeView->setVisible(false);
    }
    else
    {
        statusLabel.setVisible(false);
        treeView->setBounds(bounds);
    }
}

void InspectorPanel::setPatchList(const std::vector<std::string>& names)
{
    setLoadingState(false);
    rebuildTree(names);
}

void InspectorPanel::setLoadingState(bool loading)
{
    isLoading = loading;

    if (loading)
    {
        statusLabel.setText("Loading patches from synth...", juce::dontSendNotification);
        statusLabel.setVisible(true);
        treeView->setVisible(false);
    }
    else
    {
        statusLabel.setVisible(false);
        treeView->setVisible(true);
    }

    resized();
}

void InspectorPanel::rebuildTree(const std::vector<std::string>& names)
{
    // Create root item
    rootItem = std::make_unique<PatchTreeItem>("Synth Patches");

    // Expected: 891 entries (9 banks × 99 positions)
    // names[section * 99 + position]

    for (int section = 0; section < 9; ++section)
    {
        // Create bank node (e.g., "Bank 1 (101-199)")
        int displaySection = section + 1;
        juce::String bankName = "Bank " + juce::String(displaySection)
                              + " (" + juce::String(displaySection * 100 + 1)
                              + "-" + juce::String(displaySection * 100 + 99) + ")";

        auto* bankItem = new PatchTreeItem(bankName, section, -1);

        // Add patches within this bank
        for (int position = 0; position < 99; ++position)
        {
            int index = section * 99 + position;
            if (index >= static_cast<int>(names.size()))
                break;

            // Display location (e.g., "101: PatchName" or "101: --")
            int displayLocation = displaySection * 100 + position + 1;
            juce::String patchName = names[static_cast<size_t>(index)];
            if (patchName.isEmpty())
                patchName = "--";

            juce::String itemName = juce::String(displayLocation).paddedLeft('0', 3)
                                  + ": " + patchName;

            auto* patchItem = new PatchTreeItem(itemName, section, position);
            bankItem->addSubItem(patchItem);
        }

        rootItem->addSubItem(bankItem);
    }

    treeView->setRootItem(rootItem.get());
    treeView->setRootItemVisible(false);  // Don't show "Synth Patches" root
}

// --- PatchTreeItem implementation ---

InspectorPanel::PatchTreeItem::PatchTreeItem(const juce::String& name, int sec, int pos)
    : itemName(name), section(sec), position(pos)
{
}

bool InspectorPanel::PatchTreeItem::mightContainSubItems()
{
    // Bank nodes (section >= 0, position == -1) can contain patches
    return (section >= 0 && position == -1);
}

void InspectorPanel::PatchTreeItem::paintItem(juce::Graphics& g, int width, int height)
{
    juce::Colour textColor = juce::Colour(0xffcccccc);

    // Bank nodes in bold
    if (section >= 0 && position == -1)
    {
        g.setColour(textColor);
        g.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
    }
    else
    {
        g.setColour(textColor);
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
    }

    g.drawText(itemName, 4, 0, width - 4, height, juce::Justification::centredLeft, true);
}

void InspectorPanel::PatchTreeItem::itemClicked(const juce::MouseEvent& e)
{
    // TODO: On double-click, load this patch
    // For now, just toggle open/close for banks
    if (section >= 0 && position == -1)
    {
        setOpen(!isOpen());
    }
}
