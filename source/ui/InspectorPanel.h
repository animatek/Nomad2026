#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <string>

class InspectorPanel : public juce::Component
{
public:
    InspectorPanel();

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Update the patch list from the synth
    void setPatchList(const std::vector<std::string>& names);
    void setLoadingState(bool loading);

private:
    class PatchTreeItem : public juce::TreeViewItem
    {
    public:
        PatchTreeItem(const juce::String& name, int section = -1, int position = -1);

        bool mightContainSubItems() override;
        void paintItem(juce::Graphics& g, int width, int height) override;
        void itemClicked(const juce::MouseEvent& e) override;

    private:
        juce::String itemName;
        int section;   // -1 for root/bank nodes
        int position;  // -1 for root/bank nodes
    };

    std::unique_ptr<juce::TreeView> treeView;
    std::unique_ptr<PatchTreeItem> rootItem;
    juce::Label statusLabel;
    bool isLoading = false;

    void rebuildTree(const std::vector<std::string>& names);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InspectorPanel)
};
