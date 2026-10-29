#pragma once

#include <Engine.h>

#include <string>
#include <vector>

class EditorPanel;
class ContentBrowserPanel;

// Owns the editor's root widget tree and the set of registered content panels.
// Builds a stretch-to-fill menu bar plus a content area panels dock into, and
// installs itself as the GUI manager's root. This is the foundation the
// Hierarchy/Inspector/Viewport/... panels will build on.
class EditorUI
{
public:
    explicit EditorUI(GUI::Manager* guiManager);
    /** @brief Releases global panels after detaching their widgets. */
    ~EditorUI();
    /** @brief Installs the shared content drawer above the document panels. */
    void SetContentBrowser(Scope<ContentBrowserPanel> panel);
    /** @brief Returns whether the drawer covers the pointer and should block viewport input. */
    bool IsContentBrowserHovered() const;
    /** @brief Closes the content drawer on a click outside it and its toggle button. */
    bool DismissContentBrowserOutside(const glm::vec2& point);

    // Adds a labeled item to the menu bar (e.g. "File", "Edit", "Window").
    void AddMenuItem(const std::string& label);

    // Adds a menu item that, when clicked, opens a scrollable dropdown of `items` anchored
    // below itself - a worked example of GUI::Manager's popup layer stack (PushPopup, click-
    // outside dismissal, always-on-top z-order) together with GUI::ScrollBox for the case
    // where `items` doesn't fit the dropdown's fixed height.
    void AddDropdownMenu(const std::string& label, const std::vector<std::string>& items);

    /** @brief Adds a tab that switches visibility of panels registered for its index. */
    void AddTab(const std::string& label, bool active);
    /** @brief Opens a document panel in a new active tab and returns its index. */
    int AddDocument(Scope<EditorPanel> panel, const std::string& title, std::function<void(int)> closeRequested);
    /** @brief Removes a document and returns to Scene when closing the active tab. */
    void CloseDocument(int index);
    /** @brief Activates a document tab or the main Scene tab. */
    void ActivateTab(int index);
    /** @brief Updates the document label and unsaved marker. */
    void SetTabTitle(int index, const std::string& title);

    /** @brief Docks a panel in a tab; a negative index keeps it visible in all tabs. */
    void AddPanel(Scope<EditorPanel> panel, int tabIndex = -1);

    /** @brief Updates the project browser's path and selection summary. */
    void SetAssetBrowserStatus(const std::string& status);

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

    // Switches the active tab and updates panel visibility.
    void SetActiveTab(int index);
    // Sizes the tab to its label and close control, up to the document width limit.
    void UpdateTabWidth(int index);

    GUI::Manager* m_GUIManager;

    Ref<GUI::Canvas> m_Root;
    Ref<GUI::HorizontalBox> m_MenuBar;
    Ref<GUI::HorizontalBox> m_MenuItems;
    Ref<GUI::HorizontalBox> m_TabBar;
    Ref<GUI::Canvas> m_ContentArea;
    Ref<GUI::HorizontalBox> m_AssetBrowser;
    Ref<GUI::TextBlock> m_AssetBrowserStatus;
    Ref<GUI::Button> m_ContentBrowserButton;
    Scope<ContentBrowserPanel> m_ContentBrowser;

    struct STab
    {
        Ref<GUI::Widget> Root;
        Ref<GUI::HorizontalBox> Header;
        Ref<GUI::TextBlock> Label;
        Ref<GUI::Canvas> Underline;
        GUI::LayoutSlot* Slot;
        float CloseWidth = 0;
    };
    std::vector<STab> m_Tabs;
    int m_ActiveTabIndex = 0;

    struct SPanel
    {
        Scope<EditorPanel> Panel;
        Ref<GUI::Widget> Widget;
        int TabIndex = -1;
    };
    std::vector<SPanel> m_Panels;
};
