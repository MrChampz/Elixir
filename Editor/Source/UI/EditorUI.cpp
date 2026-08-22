#include "EditorUI.h"
#include "EditorPanel.h"
#include "EditorStyles.h"

#include <Engine/GUI/Icon.h>
#include <Engine/Icon/IconManager.h>
#include <Engine/GUI/ScrollBox.h>

#include <algorithm>

namespace
{
    constexpr float MenuBarHeight = 32.0f;
    constexpr float TabBarHeight = 34.0f;
    constexpr float AssetBrowserHeight = 28.0f;

    constexpr float DropdownWidth = 160.0f;
    constexpr float DropdownRowHeight = 26.0f;
    constexpr float DropdownMaxVisibleRows = 6.0f;
    constexpr float DropdownPadding = 4.0f;

}

EditorUI::EditorUI(GUI::Manager* guiManager)
    : m_GUIManager(guiManager)
{
    Build();
}

void EditorUI::Build()
{
    m_Root = CreateRef<GUI::Canvas>();

    BuildMenuBar();
    BuildTabBar();

    m_ContentArea = CreateRef<GUI::Canvas>();
    m_Root->AddChild(m_ContentArea)
        .SetAnchors(GUI::SAnchors::StretchAll())
        .SetOffsets(0.0f, MenuBarHeight + TabBarHeight, 0.0f, -AssetBrowserHeight);

    BuildAssetBrowser();

    m_GUIManager->SetRoot(m_Root);
}

void EditorUI::BuildMenuBar()
{
    const auto& styles = EditorStyle::Get();
    m_MenuBar = CreateRef<GUI::HorizontalBox>();
    m_MenuBar->SetStyle(styles.MenuBar);
    m_MenuBar->SetPadding({ 10.0f, 0.0f });

    m_Root->AddChild(m_MenuBar)
        .SetAnchors({ 0.0f, 0.0f, 1.0f, 0.0f })
        .SetOffsets(0.0f, 0.0f, 0.0f, 0.0f)
        .SetPosition({ 0.0f, 0.0f })
        .SetSize({ 0.0f, MenuBarHeight });

    // Logo swatch: a plain colored square stands in for a real product mark.
    const auto logo = CreateRef<GUI::Canvas>();
    logo->SetSize({ 16.0f, 16.0f });
    auto logoStyle = styles.ToolbarButtonActive;
    logoStyle.Normal.Background.CornerRadius = glm::vec4(3.0f);
    logo->SetStyle(logoStyle);
    m_MenuBar->AddChild(logo).SetFixedSize(16.0f);

    const auto title = CreateRef<GUI::TextBlock>("Elixir Engine");
    title->SetColor(styles.TextPrimary);
    title->SetFontSize(12.0f);
    m_MenuBar->AddChild(title).SetMargin(GUI::SMargin(8.0f, 0.0f, 14.0f, 0.0f));

    m_MenuItems = CreateRef<GUI::HorizontalBox>();
    m_MenuBar->AddChild(m_MenuItems);

    const auto spacer = CreateRef<GUI::Canvas>();
    m_MenuBar->AddChild(spacer).SetFillSize();

    const auto branchPill = CreateRef<GUI::HorizontalBox>();
    branchPill->SetStyle(styles.BranchPill);
    branchPill->SetPadding(GUI::SMargin(10.0f, 4.0f));
    m_MenuBar->AddChild(branchPill);

    const auto branchIcon = CreateRef<GUI::Icon>();
    branchIcon->SetIcon(IconManager::Load("./Assets/Icons/git-branch.svg"));
    branchIcon->SetSize({ 11.0f, 11.0f });
    branchIcon->SetStyle(styles.SecondaryIcon);
    branchPill->AddChild(branchIcon).SetMargin(GUI::SMargin(0.0f, 0.0f, 5.0f, 0.0f));

    const auto branchLabel = CreateRef<GUI::TextBlock>("main");
    branchLabel->SetColor(styles.TextSecondary);
    branchLabel->SetFontSize(11.0f);
    branchPill->AddChild(branchLabel);

    const auto border = CreateRef<GUI::Canvas>();
    border->SetStyle(styles.MenuBorder);
    m_Root->AddChild(border)
        .SetAnchors({ 0.0f, 0.0f, 1.0f, 0.0f })
        .SetPosition({ 0.0f, MenuBarHeight - 1.0f })
        .SetSize({ 0.0f, 1.0f });
}

void EditorUI::BuildTabBar()
{
    const auto& styles = EditorStyle::Get();
    m_TabBar = CreateRef<GUI::HorizontalBox>();
    m_TabBar->SetStyle(styles.TabBar);
    m_TabBar->SetPadding({ 8.0f, 0.0f });

    m_Root->AddChild(m_TabBar)
        .SetAnchors({ 0.0f, 0.0f, 1.0f, 0.0f })
        .SetOffsets(0.0f, MenuBarHeight, 0.0f, MenuBarHeight)
        .SetPosition({ 0.0f, MenuBarHeight })
        .SetSize({ 0.0f, TabBarHeight });
}

void EditorUI::BuildAssetBrowser()
{
    const auto& styles = EditorStyle::Get();
    m_AssetBrowser = CreateRef<GUI::HorizontalBox>();
    m_AssetBrowser->SetStyle(styles.AssetBrowser);
    m_AssetBrowser->SetPadding({ 12.0f, 0.0f });

    // Stretches horizontally (like the menu/tab bars); pinned to the bottom edge via a
    // non-stretching vertical anchor with Alignment.y = 1 so the slot's pivot is its own
    // bottom edge, not its top - same trick used to bottom-anchor a fixed-height strip.
    m_Root->AddChild(m_AssetBrowser)
        .SetAnchors({ 0.0f, 1.0f, 1.0f, 1.0f })
        .SetOffsets(0.0f, 0.0f, 0.0f, 0.0f)
        .SetPosition({ 0.0f, 0.0f })
        .SetAlignment({ 0.0f, 1.0f })
        .SetSize({ 0.0f, AssetBrowserHeight });

    const auto title = CreateRef<GUI::TextBlock>("Project");
    title->SetColor(styles.TextPrimary);
    title->SetFontSize(12.0f);
    m_AssetBrowser->AddChild(title).SetMargin(GUI::SMargin(0.0f, 0.0f, 8.0f, 0.0f));

    const auto path = CreateRef<GUI::TextBlock>("Assets / Prefabs -- 6 items");
    path->SetColor(styles.TextSecondary);
    path->SetFontSize(11.0f);
    m_AssetBrowser->AddChild(path);
}

void EditorUI::AddMenuItem(const std::string& label)
{
    const auto& styles = EditorStyle::Get();
    const auto text = CreateRef<GUI::TextBlock>(label);
    text->SetColor(styles.TextSecondary);
    text->SetFontSize(12.0f);

    m_MenuItems->AddChild(text)
        .SetMargin(GUI::SMargin(9.0f, 0.0f));
}

void EditorUI::AddTab(const std::string& label, const bool active)
{
    // A small VerticalBox column [label, underline] rather than a plain TextBlock, so the
    // active tab gets a real accent-colored indicator bar instead of just a color change.
    const auto column = CreateRef<GUI::VerticalBox>();

    const auto text = CreateRef<GUI::TextBlock>(label);
    text->SetFontSize(12.0f);
    column->AddChild(text).SetMargin(GUI::SMargin(16.0f, 7.0f, 16.0f, 4.0f));

    const auto underline = CreateRef<GUI::Canvas>();
    // Explicit small size: without it Canvas's own default desired size (100x100) would
    // feed into the column's ComputeDesiredSize cross-axis max() below, widening the whole
    // tab well past the label - EHorizontalAlignment::Fill only overrides the underline's
    // own LAYOUT width, not what the column reports wanting in the first place.
    underline->SetSize({ 1.0f, 2.0f });
    column->AddChild(underline).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

    m_TabBar->AddChild(column).SetVerticalAlignment(GUI::EVerticalAlignment::Bottom);

    const int index = static_cast<int>(m_Tabs.size());
    m_Tabs.push_back({ text, underline });
    if (active) m_ActiveTabIndex = index;

    // Registering on the column (not the label/underline individually) works because an
    // unhandled press on a leaf child bubbles up to the nearest ancestor with a callback -
    // see Widget::HandleMouseDown's early-out when no callback is set.
    column->OnClick([this, index] { SetActiveTab(index); });

    SetActiveTab(m_ActiveTabIndex);
}

void EditorUI::SetActiveTab(const int index)
{
    const auto& styles = EditorStyle::Get();
    m_ActiveTabIndex = index;
    for (size_t i = 0; i < m_Tabs.size(); ++i)
    {
        const bool active = static_cast<int>(i) == index;
        m_Tabs[i].Label->SetColor(active ? styles.TextPrimary : styles.TextSecondary);
        m_Tabs[i].Underline->SetStyle(active ? styles.TabActiveIndicator : styles.Transparent);
    }
}

void EditorUI::AddDropdownMenu(const std::string& label, const std::vector<std::string>& items)
{
    const auto& styles = EditorStyle::Get();
    const auto text = CreateRef<GUI::TextBlock>(label);
    text->SetColor(styles.TextSecondary);
    text->SetFontSize(12.0f);

    m_MenuItems->AddChild(text)
        .SetMargin(GUI::SMargin(9.0f, 0.0f));

    // A plain TextBlock only starts consuming press/click events once a callback is
    // registered on it (Widget::HandleMouseDown's default stays Unhandled otherwise) - no
    // need for a Button here, so the trigger keeps looking like the other, static menu items.
    //
    // Captured weak: the callback lives inside text->m_OnClickCallback, so capturing text
    // itself by Ref would be a self-owning reference cycle (the widget would keep its own
    // click handler alive forever, and vice versa).
    const WeakRef<GUI::Widget> triggerWeak = text;
    text->OnClick([this, triggerWeak, items]
    {
        const auto trigger = triggerWeak.lock();
        if (!trigger) return;

        // At most one dropdown open at a time: dropping every layer above the root before
        // pushing the new one also makes clicking a different trigger while one is already
        // open switch straight to it, instead of stacking dropdowns.
        m_GUIManager->ClearPopups();
        m_GUIManager->PushPopup(BuildDropdownContent(items), trigger->GetGeometry());
    });
}

Ref<GUI::Widget> EditorUI::BuildDropdownContent(const std::vector<std::string>& items) const
{
    const auto& styles = EditorStyle::Get();
    // Overlay, not Canvas: Canvas::ComputeDesiredSize ignores its children and always
    // reports a fixed 800x600 fallback (it exists for absolute/anchored positioning, not
    // content-driven sizing), which is what made the popup balloon to that size regardless
    // of the ScrollBox inside it. Overlay's desired size is the max child size plus padding,
    // so the popup shrink-wraps to the ScrollBox's configured size instead.
    const auto panel = CreateRef<GUI::Overlay>();
    panel->SetStyle(styles.Popup);
    panel->SetPadding(GUI::SPadding(DropdownPadding));

    const auto scrollBox = CreateRef<GUI::ScrollBox>();
    const float visibleRows = std::min(static_cast<float>(items.size()), DropdownMaxVisibleRows);
    scrollBox->SetSize({ DropdownWidth, visibleRows * DropdownRowHeight });
    scrollBox->SetStyle(styles.ScrollBar);

    panel->AddChild(scrollBox)
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);

    const auto list = CreateRef<GUI::VerticalBox>();
    for (const auto& item : items)
    {
        const auto row = CreateRef<GUI::TextBlock>(item);
        row->SetColor(styles.TextPrimary);
        row->SetFontSize(13.0f);

        // Selecting an item closes the dropdown, same as a real menu would.
        row->OnClick([this] { m_GUIManager->PopPopup(); });

        list->AddChild(row)
            .SetFixedSize(DropdownRowHeight)
            .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
            .SetMargin(GUI::SMargin(8.0f, 0.0f));
    }

    scrollBox->SetContent(list);

    return panel;
}

void EditorUI::AddPanel(Scope<EditorPanel> panel)
{
    const auto widget = panel->Build();
    if (widget)
    {
        m_ContentArea->AddChild(widget)
            .SetAnchors(GUI::SAnchors::StretchAll());
    }

    m_Panels.push_back(std::move(panel));
}

void EditorUI::Update(const Timestep frameTime)
{
    for (auto& panel : m_Panels)
        panel->OnUpdate(frameTime);
}
