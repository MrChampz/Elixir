#include "ViewportPanel.h"

#include <Engine/GUI/Button.h>
#include <Engine/GUI/Checkbox.h>
#include <Engine/GUI/Icon.h>
#include <Engine/GUI/IconLibrary.h>
#include <Engine/GUI/ScrollBox.h>
#include <Engine/GUI/TextField.h>

#include <cstdio>

namespace
{
    // Same rough token stand-ins EditorUI uses, kept local since there's no shared theme
    // object yet.
    const GUI::SColor ColorPanelBg = { 0.071f, 0.075f, 0.082f, 0.9f };
    const GUI::SColor ColorPanelBorder = { 0.25f, 0.26f, 0.29f, 1.0f };
    const GUI::SColor ColorSectionHeaderBg = { 0.169f, 0.176f, 0.188f, 1.0f };
    const GUI::SColor ColorFieldBg = { 0.094f, 0.098f, 0.106f, 1.0f };
    const GUI::SColor ColorFieldBorder = { 0.224f, 0.231f, 0.251f, 1.0f };
    const GUI::SColor ColorTextPrimary = { 0.875f, 0.882f, 0.898f, 1.0f };
    const GUI::SColor ColorTextSecondary = { 0.616f, 0.627f, 0.659f, 1.0f };
    const GUI::SColor ColorAccent = { 0.208f, 0.455f, 0.941f, 1.0f };
    const GUI::SColor ColorAxisX = { 0.86f, 0.23f, 0.29f, 1.0f };
    const GUI::SColor ColorAxisY = { 0.37f, 0.68f, 0.40f, 1.0f };
    const GUI::SColor ColorAxisZ = { 0.33f, 0.54f, 0.97f, 1.0f };
    const GUI::SColor ColorToolModeOff = { 0.35f, 0.36f, 0.40f, 0.0f };
    const GUI::SColor ColorPlayOn = { 0.29f, 0.61f, 0.33f, 1.0f };
    const GUI::SColor ColorPlayOff = { 0.16f, 0.24f, 0.18f, 0.0f };
    const GUI::SColor ColorPauseOn = { 0.35f, 0.36f, 0.40f, 1.0f };
    const GUI::SColor ColorPauseOff = { 0.20f, 0.21f, 0.23f, 0.0f };

    constexpr float PanelHeaderHeight = 28.0f;
    constexpr float RowHeight = 24.0f;
    // Wide enough for the longest label ("Jump Height") - fixed so every row's field/value
    // column starts at the same X regardless of how long its own label happens to be.
    constexpr float LabelWidth = 76.0f;

    std::string FormatFloat(const float value)
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.2f", value);
        return buffer;
    }

    Ref<GUI::Button> CreateChromeButton(const GUI::SBrush& brush)
    {
        const auto button = CreateRef<GUI::Button>();
        auto style = GUI::GetDefaultStyles().GetWidgetStyle<GUI::SButtonStyle>();
        for (size_t index = 0; index < static_cast<size_t>(GUI::EStyleLayer::Count); ++index)
            style.Get(static_cast<GUI::EStyleLayer>(index)).Background = brush;
        button->SetStyle(style);
        return button;
    }

    Ref<GUI::Button> CreatePanelHeaderButton()
    {
        return CreateChromeButton({});
    }
}

Ref<GUI::Widget> ViewportPanel::Build()
{
    const auto root = CreateRef<GUI::Canvas>();
    // Neutral placeholder - the real scene render will fill this in later, so there's no
    // stand-in landscape here, just the floating chrome (toolbar, panels, stats overlay).
    root->SetBackgroundColor(GUI::EStyleLayer::Normal, { 0.0f, 0.0f, 0.0f, 0.0f });

    BuildToolbar(root);
    BuildHierarchyPanel(root);
    BuildInspectorPanel(root);
    BuildStatsOverlay(root);

    return root;
}

void ViewportPanel::BuildToolbar(const Ref<GUI::Canvas>& root)
{
    const auto panel = CreateRef<GUI::Overlay>();
    panel->SetBackgroundColor(GUI::EStyleLayer::Normal, { 0.118f, 0.122f, 0.133f, 0.78f });
    panel->SetOutline(GUI::EStyleLayer::Normal, { ColorPanelBorder, 1.0f });
    panel->SetCornerRadius(GUI::EStyleLayer::Normal, 8.0f);
    panel->SetPadding(GUI::SMargin(4.0f));

    const auto row = CreateRef<GUI::HorizontalBox>();
    panel->AddChild(row);

    // Move / Rotate / Scale mode swatches - clicking one makes it the active tool.
    const char* toolIconPaths[] = {
        "./Assets/Icons/move.svg",
        "./Assets/Icons/rotate.svg",
        "./Assets/Icons/scale.svg",
    };
    m_ToolModeSwatches.clear();
    m_ToolModeIcons.clear();
    for (int index = 0; index < 3; ++index)
    {
        const auto swatch = CreateRef<GUI::Canvas>();
        swatch->SetSize({ 24.0f, 24.0f });
        swatch->SetCornerRadius(GUI::EStyleLayer::Normal, 24.0f * 0.22f);
        row->AddChild(swatch).SetMargin(GUI::SMargin(0.0f, 0.0f, index < 2 ? 2.0f : 0.0f, 0.0f));
        m_ToolModeSwatches.push_back(swatch);

        const auto icon = CreateRef<GUI::Icon>();
        icon->SetIcon(GUI::IconLibrary::Load(toolIconPaths[index]));
        icon->SetSize({ 14.0f, 14.0f });
        swatch->AddChild(icon)
            .SetAnchors(GUI::SAnchors::MiddleCenter())
            .SetAlignment({ 0.5f, 0.5f })
            .SetSize({ 14.0f, 14.0f });
        m_ToolModeIcons.push_back(icon);

        swatch->OnClick([this, index] { SetActiveToolMode(index); });
    }

    const auto divider1 = CreateRef<GUI::Canvas>();
    divider1->SetSize({ 1.0f, 18.0f });
    divider1->SetBackgroundColor(GUI::EStyleLayer::Normal, ColorPanelBorder);
    row->AddChild(divider1).SetMargin(GUI::SMargin(8.0f, 0.0f));

    const auto pivot = CreateRef<GUI::Overlay>();
    pivot->SetBackgroundColor(GUI::EStyleLayer::Normal, {});
    pivot->SetOutline(GUI::EStyleLayer::Normal, { ColorPanelBorder, 1.0f });
    pivot->SetCornerRadius(GUI::EStyleLayer::Normal, 4.0f);
    pivot->SetPadding(GUI::SMargin(8.0f, 3.0f));
    row->AddChild(pivot);

    const auto pivotLabel = CreateRef<GUI::TextBlock>("Local");
    pivotLabel->SetColor(ColorTextPrimary);
    pivotLabel->SetFontSize(11.0f);
    pivot->AddChild(pivotLabel);

    const auto divider2 = CreateRef<GUI::Canvas>();
    divider2->SetSize({ 1.0f, 18.0f });
    divider2->SetBackgroundColor(GUI::EStyleLayer::Normal, ColorPanelBorder);
    row->AddChild(divider2).SetMargin(GUI::SMargin(8.0f, 0.0f));

    const auto play = CreateRef<GUI::Canvas>();
    play->SetSize({ 26.0f, 26.0f });
    play->SetCornerRadius(GUI::EStyleLayer::Normal, 6.0f);
    row->AddChild(play).SetMargin(GUI::SMargin(0.0f, 0.0f, 2.0f, 0.0f));
    m_PlayButton = play;
    m_PlayIcon = CreateRef<GUI::Icon>();
    m_PlayIcon->SetIcon(GUI::IconLibrary::Load("./Assets/Icons/play.svg"));
    m_PlayIcon->SetSize({ 13.0f, 13.0f });
    play->AddChild(m_PlayIcon)
        .SetAnchors(GUI::SAnchors::MiddleCenter())
        .SetAlignment({ 0.5f, 0.5f })
        .SetSize({ 13.0f, 13.0f });
    play->OnClick([this] { SetPlaying(true); });

    const auto pause = CreateRef<GUI::Canvas>();
    pause->SetSize({ 26.0f, 26.0f });
    pause->SetCornerRadius(GUI::EStyleLayer::Normal, 6.0f);
    row->AddChild(pause);
    m_PauseButton = pause;
    m_PauseIcon = CreateRef<GUI::Icon>();
    m_PauseIcon->SetIcon(GUI::IconLibrary::Load("./Assets/Icons/pause.svg"));
    m_PauseIcon->SetSize({ 13.0f, 13.0f });
    pause->AddChild(m_PauseIcon)
        .SetAnchors(GUI::SAnchors::MiddleCenter())
        .SetAlignment({ 0.5f, 0.5f })
        .SetSize({ 13.0f, 13.0f });
    pause->OnClick([this] { SetPlaying(false); });

    SetActiveToolMode(m_ActiveToolMode);
    SetPlaying(m_IsPlaying);

    root->AddChild(panel)
        .SetAnchors(GUI::SAnchors::TopCenter())
        .SetPosition({ 0.0f, 10.0f })
        .SetAlignment({ 0.5f, 0.0f });
}

void ViewportPanel::BuildHierarchyPanel(const Ref<GUI::Canvas>& root)
{
    const auto panel = CreateRef<GUI::Overlay>();
    panel->SetBackgroundColor(GUI::EStyleLayer::Normal, ColorPanelBg);
    panel->SetOutline(GUI::EStyleLayer::Normal, { ColorPanelBorder, 1.0f });
    panel->SetCornerRadius(GUI::EStyleLayer::Normal, 8.0f);

    root->AddChild(panel)
        .SetAnchors(GUI::SAnchors::TopLeft())
        .SetPosition({ 10.0f, 10.0f })
        .SetSize({ 220.0f, 260.0f });
    m_HierarchyPanel = panel;

    const auto column = CreateRef<GUI::VerticalBox>();
    panel->AddChild(column)
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);

    const auto header = CreatePanelHeaderButton();
    const auto headerContent = CreateRef<GUI::HorizontalBox>();
    headerContent->SetPadding({ 8.0f, 0.0f });
    header->SetContent(headerContent)
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    column->AddChild(header)
        .SetFixedSize(PanelHeaderHeight - 1.0f)
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    header->OnClick([this] { SetHierarchyPanelOpen(false); });

    const auto title = CreateRef<GUI::TextBlock>("Hierarchy");
    title->SetColor(ColorTextPrimary);
    title->SetFontSize(12.0f);
    const auto icon = CreateRef<GUI::Icon>();
    icon->SetIcon(GUI::IconLibrary::Load("./Assets/Icons/hierarchy.svg"));
    icon->SetSize({ 14.0f, 14.0f });
    icon->SetColor(GUI::EStyleLayer::Normal, ColorTextSecondary);
    headerContent->AddChild(icon).SetMargin(GUI::SMargin(0.0f, 0.0f, 6.0f, 0.0f));
    headerContent->AddChild(title);

    const auto headerSpacer = CreateRef<GUI::Canvas>();
    headerContent->AddChild(headerSpacer).SetFillSize();

    const auto closeIcon = CreateRef<GUI::Icon>();
    closeIcon->SetIcon(GUI::IconLibrary::Load("./Assets/Icons/close.svg"));
    closeIcon->SetSize({ 11.0f, 11.0f });
    closeIcon->SetColor(GUI::EStyleLayer::Normal, ColorTextSecondary);
    headerContent->AddChild(closeIcon);

    const auto headerBorder = CreateRef<GUI::Canvas>();
    headerBorder->SetBackgroundColor(GUI::EStyleLayer::Normal, ColorPanelBorder);
    column->AddChild(headerBorder)
        .SetFixedSize(1.0f)
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

    const auto scrollBox = CreateRef<GUI::ScrollBox>();
    scrollBox->SetScrollbarThickness(8.0f);
    column->AddChild(scrollBox)
        .SetFillSize()
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

    const auto list = CreateRef<GUI::VerticalBox>();
    list->SetPadding({ 4.0f, 6.0f });
    scrollBox->SetContent(list);

    struct SHierarchyItem { std::string Label; float Indent; };
    const SHierarchyItem items[] = {
        { "Directional Light", 0.0f },
        { "Ground", 0.0f },
        { "Player", 0.0f },
        { "Mesh", 18.0f },
        { "Terrain", 0.0f },
        { "Main Camera", 0.0f },
    };

    m_HierarchyRows.clear();
    for (size_t i = 0; i < std::size(items); ++i)
    {
        const auto& item = items[i];

        const auto row = CreateRef<GUI::Overlay>();
        row->SetPadding(GUI::SMargin(8.0f + item.Indent, 0.0f, 8.0f, 0.0f));
        row->SetCornerRadius(GUI::EStyleLayer::Normal, 4.0f);
        list->AddChild(row)
            .SetFixedSize(22.0f)
            .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

        const auto label = CreateRef<GUI::TextBlock>(item.Label);
        label->SetFontSize(12.0f);
        row->AddChild(label).SetHorizontalAlignment(GUI::EHorizontalAlignment::Left);

        m_HierarchyRows.push_back({ row, label });

        const int index = static_cast<int>(i);
        row->OnClick([this, index] { SetSelectedHierarchyRow(index); });
    }

    SetSelectedHierarchyRow(m_SelectedHierarchyIndex);

    GUI::SBrush panelToggleBrush;
    panelToggleBrush.Color = ColorPanelBg;
    panelToggleBrush.Outline = { ColorPanelBorder, 1.0f };
    panelToggleBrush.CornerRadius = glm::vec4(8.0f);
    const auto panelToggle = CreateChromeButton(panelToggleBrush);
    root->AddChild(panelToggle)
        .SetAnchors(GUI::SAnchors::TopLeft())
        .SetPosition({ 10.0f, 10.0f })
        .SetSize({ 32.0f, 32.0f });
    m_HierarchyPanelToggle = panelToggle;

    const auto panelToggleIcon = CreateRef<GUI::Icon>();
    panelToggleIcon->SetIcon(GUI::IconLibrary::Load("./Assets/Icons/hierarchy.svg"));
    panelToggleIcon->SetSize({ 14.0f, 14.0f });
    panelToggleIcon->SetColor(GUI::EStyleLayer::Normal, ColorTextSecondary);
    panelToggle->SetContent(panelToggleIcon);
    panelToggle->OnClick([this] { SetHierarchyPanelOpen(true); });
    SetHierarchyPanelOpen(true);
}

void ViewportPanel::BuildInspectorPanel(const Ref<GUI::Canvas>& root)
{
    const auto panel = CreateRef<GUI::Overlay>();
    panel->SetBackgroundColor(GUI::EStyleLayer::Normal, ColorPanelBg);
    panel->SetOutline(GUI::EStyleLayer::Normal, { ColorPanelBorder, 1.0f });
    panel->SetCornerRadius(GUI::EStyleLayer::Normal, 8.0f);

    // Right-anchored, stretched vertically (top:10, bottom:10 in the mock); horizontal is
    // non-stretching, so Position/Alignment place the panel's own right edge 10px in from
    // the viewport's right edge instead.
    root->AddChild(panel)
        .SetAnchors({ 1.0f, 0.0f, 1.0f, 1.0f })
        .SetOffsets(0.0f, 10.0f, 0.0f, -10.0f)
        .SetPosition({ -10.0f, 0.0f })
        .SetAlignment({ 1.0f, 0.0f })
        .SetSize({ 260.0f, 0.0f });
    m_InspectorPanel = panel;

    const auto column = CreateRef<GUI::VerticalBox>();
    panel->AddChild(column)
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);

    const auto header = CreatePanelHeaderButton();
    const auto headerContent = CreateRef<GUI::HorizontalBox>();
    headerContent->SetPadding({ 8.0f, 0.0f });
    header->SetContent(headerContent)
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    column->AddChild(header)
        .SetFixedSize(PanelHeaderHeight - 1.0f)
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    header->OnClick([this] { SetInspectorPanelOpen(false); });

    const auto title = CreateRef<GUI::TextBlock>("Inspector");
    title->SetColor(ColorTextPrimary);
    title->SetFontSize(12.0f);
    const auto icon = CreateRef<GUI::Icon>();
    icon->SetIcon(GUI::IconLibrary::Load("./Assets/Icons/inspector.svg"));
    icon->SetSize({ 13.0f, 13.0f });
    icon->SetColor(GUI::EStyleLayer::Normal, ColorTextSecondary);
    headerContent->AddChild(icon).SetMargin(GUI::SMargin(0.0f, 0.0f, 6.0f, 0.0f));
    headerContent->AddChild(title);

    const auto headerSpacer = CreateRef<GUI::Canvas>();
    headerContent->AddChild(headerSpacer).SetFillSize();

    const auto closeIcon = CreateRef<GUI::Icon>();
    closeIcon->SetIcon(GUI::IconLibrary::Load("./Assets/Icons/close.svg"));
    closeIcon->SetSize({ 11.0f, 11.0f });
    closeIcon->SetColor(GUI::EStyleLayer::Normal, ColorTextSecondary);
    headerContent->AddChild(closeIcon);

    const auto headerBorder = CreateRef<GUI::Canvas>();
    headerBorder->SetBackgroundColor(GUI::EStyleLayer::Normal, ColorPanelBorder);
    column->AddChild(headerBorder)
        .SetFixedSize(1.0f)
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

    const auto scrollBox = CreateRef<GUI::ScrollBox>();
    scrollBox->SetScrollbarThickness(8.0f);
    column->AddChild(scrollBox)
        .SetFillSize()
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

    const auto list = CreateRef<GUI::VerticalBox>();
    scrollBox->SetContent(list);

    // --- Object header: editable name field ---
    {
        const auto block = CreateRef<GUI::Overlay>();
        block->SetPadding({ 10.0f, 10.0f });
        list->AddChild(block).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

        const auto nameField = CreateRef<GUI::TextField>(m_HierarchyRows.empty() ? "Player" : m_HierarchyRows[m_SelectedHierarchyIndex].Label->GetText());
        nameField->SetBackgroundColor(GUI::EStyleLayer::Normal, ColorFieldBg);
        nameField->SetOutline(GUI::EStyleLayer::Normal, { ColorFieldBorder, 1.0f });
        nameField->SetCornerRadius(GUI::EStyleLayer::Normal, 4.0f);
        nameField->SetPadding({ 8.0f, 5.0f });
        nameField->SetTextColor(GUI::EStyleLayer::Normal, ColorTextPrimary);
        nameField->SetCursorColor(ColorTextPrimary);
        nameField->SetSelectionColor({ ColorAccent.R, ColorAccent.G, ColorAccent.B, 0.35f });
        nameField->SetFontSize(13.0f);
        block->AddChild(nameField).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    }
    AddInspectorRow(list, "Tag", m_Inspector.Tag);
    AddInspectorRow(list, "Layer", m_Inspector.Layer);

    // --- Transform ---
    AddInspectorSectionHeader(list, "Transform");
    AddInspectorVectorRow(list, "Position", m_Inspector.Position);
    AddInspectorVectorRow(list, "Rotation", m_Inspector.Rotation);
    AddInspectorVectorRow(list, "Scale", m_Inspector.Scale);

    // --- Mesh Renderer ---
    AddInspectorSectionHeader(list, "Mesh Renderer", &m_Inspector.MeshRendererEnabled);
    AddInspectorRow(list, "Mesh", m_Inspector.Mesh);
    AddInspectorRow(list, "Material", m_Inspector.Material);
    AddInspectorToggleRow(list, "Cast Shadows", m_Inspector.CastShadows);

    // --- Rigidbody ---
    AddInspectorSectionHeader(list, "Rigidbody", &m_Inspector.RigidbodyEnabled);
    AddInspectorRow(list, "Mass", m_Inspector.Mass, true);
    AddInspectorToggleRow(list, "Use Gravity", m_Inspector.UseGravity);

    // --- PlayerController (script) ---
    AddInspectorSectionHeader(list, "PlayerController (Script)", nullptr, "./Assets/Icons/script.svg");
    AddInspectorRow(list, "Move Speed", m_Inspector.MoveSpeed, true);
    AddInspectorRow(list, "Jump Height", m_Inspector.JumpHeight, true);
    AddInspectorToggleRow(list, "Ground Check", m_Inspector.GroundCheck);

    // --- Add Component ---
    {
        const auto block = CreateRef<GUI::Overlay>();
        block->SetPadding({ 10.0f, 12.0f });
        list->AddChild(block).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

        const auto addButton = CreateRef<GUI::Overlay>();
        addButton->SetBackgroundColor(GUI::EStyleLayer::Normal, ColorSectionHeaderBg);
        addButton->SetCornerRadius(GUI::EStyleLayer::Normal, 4.0f);
        addButton->SetPadding({ 12.0f, 6.0f });
        block->AddChild(addButton).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

        const auto label = CreateRef<GUI::TextBlock>("Add Component");
        label->SetColor(ColorTextPrimary);
        label->SetFontSize(13.0f);
        addButton->AddChild(label);

        // No real component registry to add to - a demo click still needs to visibly do
        // something, so it just flashes the button darker on press.
        const WeakRef<GUI::Widget> addButtonWeak = addButton;
        addButton->OnMouseDown([addButtonWeak]
        {
            if (const auto widget = addButtonWeak.lock())
                std::static_pointer_cast<GUI::Overlay>(widget)->SetBackgroundColor(GUI::EStyleLayer::Normal, ColorFieldBg);
        });
        addButton->OnMouseUp([addButtonWeak]
        {
            if (const auto widget = addButtonWeak.lock())
                std::static_pointer_cast<GUI::Overlay>(widget)->SetBackgroundColor(GUI::EStyleLayer::Normal, ColorSectionHeaderBg);
        });
    }

    GUI::SBrush panelToggleBrush;
    panelToggleBrush.Color = ColorPanelBg;
    panelToggleBrush.Outline = { ColorPanelBorder, 1.0f };
    panelToggleBrush.CornerRadius = glm::vec4(8.0f);
    const auto panelToggle = CreateChromeButton(panelToggleBrush);
    root->AddChild(panelToggle)
        .SetAnchors(GUI::SAnchors::TopRight())
        .SetPosition({ -10.0f, 10.0f })
        .SetAlignment({ 1.0f, 0.0f })
        .SetSize({ 32.0f, 32.0f });
    m_InspectorPanelToggle = panelToggle;

    const auto panelToggleIcon = CreateRef<GUI::Icon>();
    panelToggleIcon->SetIcon(GUI::IconLibrary::Load("./Assets/Icons/inspector.svg"));
    panelToggleIcon->SetSize({ 13.0f, 13.0f });
    panelToggleIcon->SetColor(GUI::EStyleLayer::Normal, ColorTextSecondary);
    panelToggle->SetContent(panelToggleIcon);
    panelToggle->OnClick([this] { SetInspectorPanelOpen(true); });
    SetInspectorPanelOpen(true);
}

void ViewportPanel::BuildStatsOverlay(const Ref<GUI::Canvas>& root)
{
    const auto panel = CreateRef<GUI::Overlay>();
    panel->SetBackgroundColor(GUI::EStyleLayer::Normal, { 0.118f, 0.122f, 0.133f, 0.55f });
    panel->SetOutline(GUI::EStyleLayer::Normal, { ColorPanelBorder, 1.0f });
    panel->SetCornerRadius(GUI::EStyleLayer::Normal, 5.0f);
    panel->SetPadding({ 10.0f, 6.0f });

    root->AddChild(panel)
        .SetAnchors(GUI::SAnchors::BottomLeft())
        .SetPosition({ 10.0f, -10.0f })
        .SetAlignment({ 0.0f, 1.0f });

    const auto column = CreateRef<GUI::VerticalBox>();
    panel->AddChild(column);

    const auto line1 = CreateRef<GUI::TextBlock>("60 FPS - 4.2ms");
    line1->SetColor({ 0.7f, 0.72f, 0.75f, 1.0f });
    line1->SetFontSize(11.0f);
    column->AddChild(line1);

    const auto line2 = CreateRef<GUI::TextBlock>("12,480 tris - 38 draw calls");
    line2->SetColor({ 0.7f, 0.72f, 0.75f, 1.0f });
    line2->SetFontSize(11.0f);
    column->AddChild(line2);
}

void ViewportPanel::SetActiveToolMode(const int index)
{
    m_ActiveToolMode = index;
    for (size_t i = 0; i < m_ToolModeSwatches.size(); ++i)
    {
        const auto canvas = std::static_pointer_cast<GUI::Canvas>(m_ToolModeSwatches[i]);
        const bool active = static_cast<int>(i) == index;
        canvas->SetBackgroundColor(GUI::EStyleLayer::Normal, active ? ColorAccent : ColorToolModeOff);
        m_ToolModeIcons[i]->SetColor(
            GUI::EStyleLayer::Normal,
            active ? GUI::SColor{ 1.0f, 1.0f, 1.0f, 1.0f } : ColorTextSecondary
        );
    }
}

void ViewportPanel::SetPlaying(const bool playing)
{
    m_IsPlaying = playing;
    std::static_pointer_cast<GUI::Canvas>(m_PlayButton)->SetBackgroundColor(GUI::EStyleLayer::Normal, playing ? ColorPlayOn : ColorPlayOff);
    std::static_pointer_cast<GUI::Canvas>(m_PauseButton)->SetBackgroundColor(GUI::EStyleLayer::Normal, playing ? ColorPauseOff : ColorPauseOn);
    m_PlayIcon->SetColor(GUI::EStyleLayer::Normal, playing ? GUI::SColor{ 1.0f, 1.0f, 1.0f, 1.0f } : ColorTextSecondary);
    m_PauseIcon->SetColor(GUI::EStyleLayer::Normal, playing ? ColorTextSecondary : GUI::SColor{ 1.0f, 1.0f, 1.0f, 1.0f });
}

void ViewportPanel::SetSelectedHierarchyRow(const int index)
{
    m_SelectedHierarchyIndex = index;
    for (size_t i = 0; i < m_HierarchyRows.size(); ++i)
    {
        const bool selected = static_cast<int>(i) == index;
        std::static_pointer_cast<GUI::Overlay>(m_HierarchyRows[i].Row)
            ->SetBackgroundColor(GUI::EStyleLayer::Normal, selected ? GUI::SColor{ ColorAccent.R, ColorAccent.G, ColorAccent.B, 0.28f } : GUI::SColor{});
        m_HierarchyRows[i].Label->SetColor(selected ? ColorTextPrimary : ColorTextSecondary);
    }
}

void ViewportPanel::SetHierarchyPanelOpen(const bool open)
{
    m_HierarchyPanel->SetVisibility(open ? GUI::EVisibility::Visible : GUI::EVisibility::Collapsed);
    m_HierarchyPanelToggle->SetVisibility(open ? GUI::EVisibility::Collapsed : GUI::EVisibility::Visible);
}

void ViewportPanel::SetInspectorPanelOpen(const bool open)
{
    m_InspectorPanel->SetVisibility(open ? GUI::EVisibility::Visible : GUI::EVisibility::Collapsed);
    m_InspectorPanelToggle->SetVisibility(open ? GUI::EVisibility::Collapsed : GUI::EVisibility::Visible);
}

void ViewportPanel::AddInspectorSectionHeader(
    const Ref<GUI::VerticalBox>& list,
    const std::string& name,
    bool* enabledValue,
    const char* iconPath
)
{
    const auto header = CreateRef<GUI::HorizontalBox>();
    header->SetBackgroundColor(GUI::EStyleLayer::Normal, ColorSectionHeaderBg);
    header->SetPadding({ 10.0f, 0.0f });
    list->AddChild(header)
        .SetFixedSize(PanelHeaderHeight)
        .SetMargin(GUI::SMargin(0.0f, 4.0f, 0.0f, 2.0f))
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

    const auto disclosureIcon = CreateRef<GUI::Icon>();
    disclosureIcon->SetIcon(GUI::IconLibrary::Load("./Assets/Icons/chevron-right.svg"));
    disclosureIcon->SetSize({ 9.0f, 9.0f });
    disclosureIcon->SetColor(GUI::EStyleLayer::Normal, ColorTextSecondary);
    header->AddChild(disclosureIcon).SetMargin(GUI::SMargin(0.0f, 0.0f, 6.0f, 0.0f));

    if (iconPath)
    {
        const auto icon = CreateRef<GUI::Icon>();
        icon->SetIcon(GUI::IconLibrary::Load(iconPath));
        icon->SetSize({ 13.0f, 13.0f });
        icon->SetColor(GUI::EStyleLayer::Normal, { 0.922f, 0.694f, 0.286f, 1.0f });
        header->AddChild(icon).SetMargin(GUI::SMargin(0.0f, 0.0f, 6.0f, 0.0f));
    }

    const auto label = CreateRef<GUI::TextBlock>(name);
    label->SetColor(ColorTextPrimary);
    label->SetFontSize(12.0f);
    header->AddChild(label);

    if (enabledValue)
    {
        const auto spacer = CreateRef<GUI::Canvas>();
        header->AddChild(spacer).SetFillSize();

        const auto checkbox = CreateRef<GUI::Checkbox>();
        checkbox->SetSize({ 13.0f, 13.0f });
        checkbox->SetCornerRadius(GUI::EStyleLayer::Normal, 3.0f);
        checkbox->SetCheckedColor(ColorAccent);
        checkbox->SetBackgroundColor(GUI::EStyleLayer::Normal, ColorFieldBg);
        checkbox->SetOutline(GUI::EStyleLayer::Normal, { ColorFieldBorder, 1.0f });
        checkbox->SetChecked(*enabledValue);
        checkbox->OnCheckedChanged([enabledValue](const bool checked) { *enabledValue = checked; });
        header->AddChild(checkbox).SetVerticalAlignment(GUI::EVerticalAlignment::Center);
    }
}

void ViewportPanel::AddInspectorRow(
    const Ref<GUI::VerticalBox>& list,
    const std::string& label,
    std::string& value,
    const bool monospace
)
{
    const auto row = CreateRef<GUI::HorizontalBox>();
    row->SetPadding({ 10.0f, 0.0f });
    list->AddChild(row)
        .SetFixedSize(RowHeight)
        .SetMargin(GUI::SMargin(0.0f, 2.0f))
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

    const auto labelText = CreateRef<GUI::TextBlock>(label);
    labelText->SetColor(ColorTextSecondary);
    labelText->SetFontSize(11.0f);
    row->AddChild(labelText)
        .SetFixedSize(LabelWidth)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Center);

    const auto field = CreateRef<GUI::TextField>(value);
    field->SetBackgroundColor(GUI::EStyleLayer::Normal, ColorFieldBg);
    field->SetOutline(GUI::EStyleLayer::Normal, { ColorFieldBorder, 1.0f });
    field->SetCornerRadius(GUI::EStyleLayer::Normal, 4.0f);
    field->SetPadding({ 8.0f, 4.0f });
    field->SetTextColor(GUI::EStyleLayer::Normal, ColorTextPrimary);
    field->SetCursorColor(ColorTextPrimary);
    field->SetSelectionColor({ ColorAccent.R, ColorAccent.G, ColorAccent.B, 0.35f });
    field->SetFontSize(monospace ? 11.0f : 12.0f);
    field->OnChange([&value](const std::string& text) { value = text; });
    // Vertical Fill (not Center) so the row's real height wins over TextField's own 30px
    // minimum desired size - see Widget::AlignVertically, Fill ignores childSize entirely.
    row->AddChild(field)
        .SetFillSize()
        .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
}

void ViewportPanel::AddInspectorToggleRow(const Ref<GUI::VerticalBox>& list, const std::string& label, bool& value)
{
    const auto row = CreateRef<GUI::HorizontalBox>();
    row->SetPadding({ 10.0f, 0.0f });
    list->AddChild(row)
        .SetFixedSize(RowHeight)
        .SetMargin(GUI::SMargin(0.0f, 2.0f))
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

    const auto checkbox = CreateRef<GUI::Checkbox>();
    checkbox->SetSize({ 13.0f, 13.0f });
    checkbox->SetCornerRadius(GUI::EStyleLayer::Normal, 3.0f);
    checkbox->SetCheckedColor(ColorAccent);
    checkbox->SetBackgroundColor(GUI::EStyleLayer::Normal, ColorFieldBg);
    checkbox->SetOutline(GUI::EStyleLayer::Normal, { ColorFieldBorder, 1.0f });
    checkbox->SetChecked(value);
    checkbox->OnCheckedChanged([&value](const bool checked) { value = checked; });
    row->AddChild(checkbox)
        .SetMargin(GUI::SMargin(0.0f, 0.0f, 8.0f, 0.0f))
        .SetVerticalAlignment(GUI::EVerticalAlignment::Center);

    const auto labelText = CreateRef<GUI::TextBlock>(label);
    labelText->SetColor(ColorTextPrimary);
    labelText->SetFontSize(12.0f);
    row->AddChild(labelText).SetVerticalAlignment(GUI::EVerticalAlignment::Center);
}

void ViewportPanel::AddInspectorVectorRow(
    const Ref<GUI::VerticalBox>& list,
    const std::string& label,
    glm::vec3& value
)
{
    const auto row = CreateRef<GUI::HorizontalBox>();
    row->SetPadding({ 10.0f, 0.0f });
    list->AddChild(row)
        .SetFixedSize(RowHeight)
        .SetMargin(GUI::SMargin(0.0f, 2.0f))
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

    const auto labelText = CreateRef<GUI::TextBlock>(label);
    labelText->SetColor(ColorTextSecondary);
    labelText->SetFontSize(11.0f);
    row->AddChild(labelText)
        .SetFixedSize(LabelWidth)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Center);

    const auto fields = CreateRef<GUI::HorizontalBox>();
    // Vertical Fill too, not just the default Center: otherwise fields' own desired height
    // (inflated by the TextField/outline budgeting inside each chip - see the Fill notes
    // above) wins over the row's real height instead of being clamped to it.
    row->AddChild(fields)
        .SetFillSize()
        .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);

    const auto addAxis = [&fields](const char* axis, const GUI::SColor& color, float& component)
    {
        const auto chip = CreateRef<GUI::Overlay>();
        chip->SetBackgroundColor(GUI::EStyleLayer::Normal, ColorFieldBg);
        chip->SetOutline(GUI::EStyleLayer::Normal, { ColorFieldBorder, 1.0f });
        chip->SetCornerRadius(GUI::EStyleLayer::Normal, 4.0f);
        // Vertical Fill all the way down (chip -> row -> field) so the field's nested
        // TextField never inflates anything above the row's actual allocated height - see
        // the Fill note above AddInspectorRow's field.
        fields->AddChild(chip)
            .SetFillSize()
            .SetMargin(GUI::SMargin(2.0f, 0.0f))
            .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);

        const auto row = CreateRef<GUI::HorizontalBox>();
        chip->AddChild(row)
            .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
            .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);

        const auto badge = CreateRef<GUI::Overlay>();
        badge->SetBackgroundColor(GUI::EStyleLayer::Normal, { color.R, color.G, color.B, 0.2f });
        badge->SetPadding({ 5.0f, 4.0f });
        row->AddChild(badge);

        const auto axisLabel = CreateRef<GUI::TextBlock>(axis);
        axisLabel->SetColor(color);
        axisLabel->SetFontSize(11.0f);
        badge->AddChild(axisLabel);

        // Transparent background: the chip built above already supplies the outline/bg -
        // this field just needs to be able to take focus and text input over it.
        const auto field = CreateRef<GUI::TextField>(FormatFloat(component));
        field->SetBackgroundColor(GUI::EStyleLayer::Normal, { 0.0f, 0.0f, 0.0f, 0.0f });
        field->SetTextColor(GUI::EStyleLayer::Normal, ColorTextPrimary);
        field->SetCursorColor(ColorTextPrimary);
        field->SetFontSize(11.0f);
        field->SetPadding({ 5.0f, 4.0f, 0.0f, 4.0f });
        field->OnChange([&component](const std::string& text)
        {
            try { component = std::stof(text); }
            catch (...) { /* leave the last valid value in place */ }
        });
        row->AddChild(field)
            .SetFillSize()
            .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    };

    addAxis("X", ColorAxisX, value.x);
    addAxis("Y", ColorAxisY, value.y);
    addAxis("Z", ColorAxisZ, value.z);
}
