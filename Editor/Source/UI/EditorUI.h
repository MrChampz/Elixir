#pragma once

#include <Engine.h>

#include <string>
#include <vector>

class EditorPanel;

// Owns the editor's root widget tree and the set of registered content panels.
// Builds a stretch-to-fill menu bar plus a content area panels dock into, and
// installs itself as the GUI manager's root. This is the foundation the
// Hierarchy/Inspector/Viewport/... panels will build on.
class EditorUI
{
public:
    explicit EditorUI(GUI::Manager* guiManager);

    // Adds a labeled item to the menu bar (e.g. "File", "Edit", "Window").
    void AddMenuItem(const std::string& label);

    // Adds a menu item that, when clicked, opens a scrollable dropdown of `items` anchored
    // below itself - a worked example of GUI::Manager's popup layer stack (PushPopup, click-
    // outside dismissal, always-on-top z-order) together with GUI::ScrollBox for the case
    // where `items` doesn't fit the dropdown's fixed height.
    void AddDropdownMenu(const std::string& label, const std::vector<std::string>& items);

    // Adds a labeled tab to the tab bar (e.g. "Scene", "Game"). Purely decorative for now -
    // `active` only controls its initial look, there's no click-to-switch wiring yet.
    void AddTab(const std::string& label, bool active);

    // Registers a panel, builds its widget, and docks it into the content area.
    void AddPanel(Scope<EditorPanel> panel);

    void Update(Timestep frameTime);

private:
    void Build();
    void BuildMenuBar();
    void BuildTabBar();
    void BuildAssetBrowser();

    // Builds the popup content for AddDropdownMenu: a background panel containing a
    // ScrollBox, itself containing one row per item. Selecting a row (or clicking outside)
    // closes the popup.
    Ref<GUI::Widget> BuildDropdownContent(const std::vector<std::string>& items) const;

    // Repaints every tab's label/underline for whichever one is now active. Purely visual -
    // there's no per-tab content to swap yet.
    void SetActiveTab(int index);

    GUI::Manager* m_GUIManager;

    Ref<GUI::Canvas> m_Root;
    Ref<GUI::HorizontalBox> m_MenuBar;
    Ref<GUI::HorizontalBox> m_MenuItems;
    Ref<GUI::HorizontalBox> m_TabBar;
    Ref<GUI::Canvas> m_ContentArea;
    Ref<GUI::HorizontalBox> m_AssetBrowser;

    struct STab
    {
        Ref<GUI::TextBlock> Label;
        Ref<GUI::Canvas> Underline;
    };
    std::vector<STab> m_Tabs;
    int m_ActiveTabIndex = 0;

    std::vector<Scope<EditorPanel>> m_Panels;
};
