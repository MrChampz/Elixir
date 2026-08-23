#include "ViewportPanel.h"
#include "../EditorStyles.h"

#include <Engine/GUI/Button.h>
#include <Engine/GUI/Checkbox.h>
#include <Engine/GUI/Icon.h>
#include <Engine/Icon/IconManager.h>
#include <Engine/GUI/ScrollBox.h>
#include <Engine/GUI/TextField.h>

#include <cstdio>

namespace
{
    const EditorStyle::SChromeStyles& Styles()
    {
        return EditorStyle::Get();
    }

    constexpr float PanelHeaderHeight = 28.0f;
    constexpr float RowHeight = 24.0f;
    // Wide enough for the longest label ("Jump Height") - fixed so every row's field/value
    // column starts at the same X regardless of how long its own label happens to be.
    constexpr float LabelWidth = 76.0f;

    template<typename TValue>
    AnimationCurve<TValue> MakeCurve(
        const TValue& from,
        const TValue& to,
        const float delay,
        const float duration
    )
    {
        AnimationCurve<TValue> curve;
        curve.AddKey({ .Time = 0.0f, .Value = from, .Interpolation = EKeyframeInterpolation::EaseOut });
        if (delay > 0.0f)
            curve.AddKey({ .Time = delay, .Value = from, .Interpolation = EKeyframeInterpolation::EaseOut });
        curve.AddKey({ .Time = delay + duration, .Value = to });
        return curve;
    }

    std::string FormatFloat(const float value)
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.2f", value);
        return buffer;
    }

    Ref<GUI::Button> CreateChromeButton(const GUI::SButtonStyle& style)
    {
        const auto button = CreateRef<GUI::Button>();
        button->SetStyle(style);
        return button;
    }

    Ref<GUI::Button> CreatePanelHeaderButton()
    {
        return CreateChromeButton(Styles().PanelHeaderButton);
    }
}

Ref<GUI::Widget> ViewportPanel::Build()
{
    const auto root = CreateRef<GUI::Canvas>();
    // Neutral placeholder - the real scene render will fill this in later, so there's no
    // stand-in landscape here, just the floating chrome (toolbar, panels, stats overlay).
    root->SetStyle(Styles().Transparent);

    BuildToolbar(root);
    BuildHierarchyPanel(root);
    BuildInspectorPanel(root);
    BuildStatsOverlay(root);

    return root;
}

void ViewportPanel::OnUpdate(const Timestep frameTime)
{
    if (m_HierarchyPanelAnimation) m_HierarchyPanelAnimation->Update(frameTime);
    if (m_HierarchyPanelToggleAnimation) m_HierarchyPanelToggleAnimation->Update(frameTime);
    if (m_InspectorPanelAnimation) m_InspectorPanelAnimation->Update(frameTime);
    if (m_InspectorPanelToggleAnimation) m_InspectorPanelToggleAnimation->Update(frameTime);
}

void ViewportPanel::BuildToolbar(const Ref<GUI::Canvas>& root)
{
    const auto panel = CreateRef<GUI::Overlay>();
    panel->SetStyle(Styles().Toolbar);
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
        swatch->SetStyle(Styles().ToolbarButtonInactive);
        row->AddChild(swatch).SetMargin(GUI::SMargin(0.0f, 0.0f, index < 2 ? 2.0f : 0.0f, 0.0f));
        m_ToolModeSwatches.push_back(swatch);

        const auto icon = CreateRef<GUI::Icon>();
        icon->SetIcon(IconManager::Load(toolIconPaths[index]));
        icon->SetSize({ 14.0f, 14.0f });
        icon->SetStyle(Styles().SecondaryIcon);
        swatch->AddChild(icon)
            .SetAnchors(GUI::SAnchors::MiddleCenter())
            .SetAlignment({ 0.5f, 0.5f })
            .SetSize({ 14.0f, 14.0f });
        m_ToolModeIcons.push_back(icon);

        swatch->OnClick([this, index] { SetActiveToolMode(index); });
    }

    const auto divider1 = CreateRef<GUI::Canvas>();
    divider1->SetSize({ 1.0f, 18.0f });
    divider1->SetStyle(Styles().PanelHeaderBorder);
    row->AddChild(divider1).SetMargin(GUI::SMargin(8.0f, 0.0f));

    const auto pivot = CreateRef<GUI::Overlay>();
    pivot->SetStyle(Styles().InspectorField);
    pivot->SetPadding(GUI::SMargin(8.0f, 3.0f));
    row->AddChild(pivot);

    const auto pivotLabel = CreateRef<GUI::TextBlock>("Local");
    pivotLabel->SetColor(Styles().TextPrimary);
    pivotLabel->SetFontSize(11.0f);
    pivot->AddChild(pivotLabel);

    const auto divider2 = CreateRef<GUI::Canvas>();
    divider2->SetSize({ 1.0f, 18.0f });
    divider2->SetStyle(Styles().PanelHeaderBorder);
    row->AddChild(divider2).SetMargin(GUI::SMargin(8.0f, 0.0f));

    const auto play = CreateRef<GUI::Canvas>();
    play->SetSize({ 26.0f, 26.0f });
    play->SetStyle(Styles().PlayButtonInactive);
    row->AddChild(play).SetMargin(GUI::SMargin(0.0f, 0.0f, 2.0f, 0.0f));
    m_PlayButton = play;
    m_PlayIcon = CreateRef<GUI::Icon>();
    m_PlayIcon->SetIcon(IconManager::Load("./Assets/Icons/play.svg"));
    m_PlayIcon->SetSize({ 13.0f, 13.0f });
    m_PlayIcon->SetStyle(Styles().SecondaryIcon);
    play->AddChild(m_PlayIcon)
        .SetAnchors(GUI::SAnchors::MiddleCenter())
        .SetAlignment({ 0.5f, 0.5f })
        .SetSize({ 13.0f, 13.0f });
    play->OnClick([this] { SetPlaying(true); });

    const auto pause = CreateRef<GUI::Canvas>();
    pause->SetSize({ 26.0f, 26.0f });
    pause->SetStyle(Styles().PauseButtonInactive);
    row->AddChild(pause);
    m_PauseButton = pause;
    m_PauseIcon = CreateRef<GUI::Icon>();
    m_PauseIcon->SetIcon(IconManager::Load("./Assets/Icons/pause.svg"));
    m_PauseIcon->SetSize({ 13.0f, 13.0f });
    m_PauseIcon->SetStyle(Styles().SecondaryIcon);
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
    panel->SetStyle(Styles().Panel);

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
    title->SetColor(Styles().TextPrimary);
    title->SetFontSize(12.0f);
    const auto icon = CreateRef<GUI::Icon>();
    icon->SetIcon(IconManager::Load("./Assets/Icons/hierarchy.svg"));
    icon->SetSize({ 14.0f, 14.0f });
    icon->SetStyle(Styles().SecondaryIcon);
    headerContent->AddChild(icon).SetMargin(GUI::SMargin(0.0f, 0.0f, 6.0f, 0.0f));
    headerContent->AddChild(title);

    const auto headerSpacer = CreateRef<GUI::Canvas>();
    headerContent->AddChild(headerSpacer).SetFillSize();

    const auto closeIcon = CreateRef<GUI::Icon>();
    closeIcon->SetIcon(IconManager::Load("./Assets/Icons/close.svg"));
    closeIcon->SetSize({ 11.0f, 11.0f });
    closeIcon->SetStyle(Styles().SecondaryIcon);
    headerContent->AddChild(closeIcon);

    const auto headerBorder = CreateRef<GUI::Canvas>();
    headerBorder->SetStyle(Styles().PanelHeaderBorder);
    column->AddChild(headerBorder)
        .SetFixedSize(1.0f)
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

    const auto scrollBox = CreateRef<GUI::ScrollBox>();
    scrollBox->SetStyle(Styles().ScrollBar);
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
        row->SetStyle(Styles().Transparent);
        row->SetPadding(GUI::SMargin(8.0f + item.Indent, 0.0f, 8.0f, 0.0f));
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

    const auto panelToggle = CreateChromeButton(Styles().PanelToggleButton);
    root->AddChild(panelToggle)
        .SetAnchors(GUI::SAnchors::TopLeft())
        .SetPosition({ 10.0f, 10.0f })
        .SetSize({ 32.0f, 32.0f });
    m_HierarchyPanelToggle = panelToggle;

    const auto panelToggleIcon = CreateRef<GUI::Icon>();
    panelToggleIcon->SetIcon(IconManager::Load("./Assets/Icons/hierarchy.svg"));
    panelToggleIcon->SetSize({ 14.0f, 14.0f });
    panelToggleIcon->SetStyle(Styles().SecondaryIcon);
    panelToggle->SetContent(panelToggleIcon);
    panelToggle->OnClick([this] { SetHierarchyPanelOpen(true); });
    m_HierarchyPanelAnimation = CreateScope<GUI::WidgetAnimation>(m_HierarchyPanel);
    m_HierarchyPanelToggleAnimation = CreateScope<GUI::WidgetAnimation>(m_HierarchyPanelToggle);
    SetHierarchyPanelOpen(true, false);
}

void ViewportPanel::BuildInspectorPanel(const Ref<GUI::Canvas>& root)
{
    const auto panel = CreateRef<GUI::Overlay>();
    panel->SetStyle(Styles().Panel);

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
    title->SetColor(Styles().TextPrimary);
    title->SetFontSize(12.0f);
    const auto icon = CreateRef<GUI::Icon>();
    icon->SetIcon(IconManager::Load("./Assets/Icons/inspector.svg"));
    icon->SetSize({ 13.0f, 13.0f });
    icon->SetStyle(Styles().SecondaryIcon);
    headerContent->AddChild(icon).SetMargin(GUI::SMargin(0.0f, 0.0f, 6.0f, 0.0f));
    headerContent->AddChild(title);

    const auto headerSpacer = CreateRef<GUI::Canvas>();
    headerContent->AddChild(headerSpacer).SetFillSize();

    const auto closeIcon = CreateRef<GUI::Icon>();
    closeIcon->SetIcon(IconManager::Load("./Assets/Icons/close.svg"));
    closeIcon->SetSize({ 11.0f, 11.0f });
    closeIcon->SetStyle(Styles().SecondaryIcon);
    headerContent->AddChild(closeIcon);

    const auto headerBorder = CreateRef<GUI::Canvas>();
    headerBorder->SetStyle(Styles().PanelHeaderBorder);
    column->AddChild(headerBorder)
        .SetFixedSize(1.0f)
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

    const auto scrollBox = CreateRef<GUI::ScrollBox>();
    scrollBox->SetStyle(Styles().ScrollBar);
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
        nameField->SetStyle(Styles().TextField);
        nameField->SetPadding({ 8.0f, 5.0f });
        nameField->SetCursorColor(Styles().TextPrimary);
        nameField->SetSelectionColor({ Styles().Accent.R, Styles().Accent.G, Styles().Accent.B, 0.35f });
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
        addButton->SetStyle(Styles().PanelSection);
        addButton->SetPadding({ 12.0f, 6.0f });
        block->AddChild(addButton).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

        const auto label = CreateRef<GUI::TextBlock>("Add Component");
        label->SetColor(Styles().TextPrimary);
        label->SetFontSize(13.0f);
        addButton->AddChild(label);

        const WeakRef<GUI::Widget> addButtonWeak = addButton;
        addButton->OnMouseDown([addButtonWeak]
        {
            if (const auto widget = addButtonWeak.lock())
                std::static_pointer_cast<GUI::Overlay>(widget)->SetStyle(Styles().InspectorField);
        });
        addButton->OnMouseUp([addButtonWeak]
        {
            if (const auto widget = addButtonWeak.lock())
                std::static_pointer_cast<GUI::Overlay>(widget)->SetStyle(Styles().PanelSection);
        });
    }

    const auto panelToggle = CreateChromeButton(Styles().PanelToggleButton);
    root->AddChild(panelToggle)
        .SetAnchors(GUI::SAnchors::TopRight())
        .SetPosition({ -10.0f, 10.0f })
        .SetAlignment({ 1.0f, 0.0f })
        .SetSize({ 32.0f, 32.0f });
    m_InspectorPanelToggle = panelToggle;

    const auto panelToggleIcon = CreateRef<GUI::Icon>();
    panelToggleIcon->SetIcon(IconManager::Load("./Assets/Icons/inspector.svg"));
    panelToggleIcon->SetSize({ 13.0f, 13.0f });
    panelToggleIcon->SetStyle(Styles().SecondaryIcon);
    panelToggle->SetContent(panelToggleIcon);
    panelToggle->OnClick([this] { SetInspectorPanelOpen(true); });
    m_InspectorPanelAnimation = CreateScope<GUI::WidgetAnimation>(m_InspectorPanel);
    m_InspectorPanelToggleAnimation = CreateScope<GUI::WidgetAnimation>(m_InspectorPanelToggle);
    SetInspectorPanelOpen(true, false);
}

void ViewportPanel::BuildStatsOverlay(const Ref<GUI::Canvas>& root)
{
    const auto panel = CreateRef<GUI::Overlay>();
    panel->SetStyle(Styles().StatsOverlay);
    panel->SetPadding({ 10.0f, 6.0f });

    root->AddChild(panel)
        .SetAnchors(GUI::SAnchors::BottomLeft())
        .SetPosition({ 10.0f, -10.0f })
        .SetAlignment({ 0.0f, 1.0f });

    const auto column = CreateRef<GUI::VerticalBox>();
    panel->AddChild(column);

    const auto line1 = CreateRef<GUI::TextBlock>("60 FPS - 4.2ms");
    line1->SetColor(Styles().TextSecondary);
    line1->SetFontSize(11.0f);
    column->AddChild(line1);

    const auto line2 = CreateRef<GUI::TextBlock>("12,480 tris - 38 draw calls");
    line2->SetColor(Styles().TextSecondary);
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
        canvas->SetStyle(active ? Styles().ToolbarButtonActive : Styles().ToolbarButtonInactive);
        m_ToolModeIcons[i]->SetStyle(active ? Styles().PrimaryIcon : Styles().SecondaryIcon);
    }
}

void ViewportPanel::SetPlaying(const bool playing)
{
    m_IsPlaying = playing;
    std::static_pointer_cast<GUI::Canvas>(m_PlayButton)->SetStyle(
        playing ? Styles().PlayButtonActive : Styles().PlayButtonInactive
    );
    std::static_pointer_cast<GUI::Canvas>(m_PauseButton)->SetStyle(
        playing ? Styles().PauseButtonInactive : Styles().PauseButtonActive
    );
    m_PlayIcon->SetStyle(playing ? Styles().PrimaryIcon : Styles().SecondaryIcon);
    m_PauseIcon->SetStyle(playing ? Styles().SecondaryIcon : Styles().PrimaryIcon);
}

void ViewportPanel::SetSelectedHierarchyRow(const int index)
{
    m_SelectedHierarchyIndex = index;
    for (size_t i = 0; i < m_HierarchyRows.size(); ++i)
    {
        const bool selected = static_cast<int>(i) == index;
        std::static_pointer_cast<GUI::Overlay>(m_HierarchyRows[i].Row)
            ->SetStyle(selected ? Styles().Selection : Styles().Transparent);
        m_HierarchyRows[i].Label->SetColor(selected ? Styles().TextPrimary : Styles().TextSecondary);
    }
}

void ViewportPanel::SetHierarchyPanelOpen(const bool open, const bool animate)
{
    if (animate)
    {
        AnimatePanel(m_HierarchyPanel, *m_HierarchyPanelAnimation, open, { -12.0f, 0.0f });
        AnimatePanelToggle(m_HierarchyPanelToggle, *m_HierarchyPanelToggleAnimation, open);
    }
    else
    {
        m_HierarchyPanelAnimation->Stop();
        m_HierarchyPanelToggleAnimation->Stop();
        m_HierarchyPanel->SetOpacity(1.0f);
        m_HierarchyPanel->SetRenderOffset({});
        m_HierarchyPanel->SetVisibility(open ? GUI::EVisibility::Visible : GUI::EVisibility::Collapsed);
        m_HierarchyPanelToggle->SetOpacity(1.0f);
        m_HierarchyPanelToggle->SetRenderOffset({});
        m_HierarchyPanelToggle->SetVisibility(open ? GUI::EVisibility::Collapsed : GUI::EVisibility::Visible);
    }
}

void ViewportPanel::SetInspectorPanelOpen(const bool open, const bool animate)
{
    if (animate)
    {
        AnimatePanel(m_InspectorPanel, *m_InspectorPanelAnimation, open, { 12.0f, 0.0f });
        AnimatePanelToggle(m_InspectorPanelToggle, *m_InspectorPanelToggleAnimation, open);
    }
    else
    {
        m_InspectorPanelAnimation->Stop();
        m_InspectorPanelToggleAnimation->Stop();
        m_InspectorPanel->SetOpacity(1.0f);
        m_InspectorPanel->SetRenderOffset({});
        m_InspectorPanel->SetVisibility(open ? GUI::EVisibility::Visible : GUI::EVisibility::Collapsed);
        m_InspectorPanelToggle->SetOpacity(1.0f);
        m_InspectorPanelToggle->SetRenderOffset({});
        m_InspectorPanelToggle->SetVisibility(open ? GUI::EVisibility::Collapsed : GUI::EVisibility::Visible);
    }
}

void ViewportPanel::AnimatePanel(
    const Ref<GUI::Widget>& panel,
    GUI::WidgetAnimation& animation,
    const bool open,
    const glm::vec2& offset
)
{
    panel->SetVisibility(GUI::EVisibility::HitTestInvisible);
    animation.ClearTracks();
    animation.AddTrack(
        MakeCurve(open ? 0.0f : 1.0f, open ? 1.0f : 0.0f, Styles().PanelTransitionDelay, Styles().PanelTransitionDuration),
        [](GUI::Widget& widget, const float value) { widget.SetOpacity(value); }
    );
    animation.AddTrack(
        MakeCurve(open ? offset : glm::vec2{}, open ? glm::vec2{} : offset, Styles().PanelTransitionDelay, Styles().PanelTransitionDuration),
        [](GUI::Widget& widget, const glm::vec2& value) { widget.SetRenderOffset(value); }
    );
    animation.OnFinished([panel, open]
    {
        panel->SetVisibility(open ? GUI::EVisibility::Visible : GUI::EVisibility::Collapsed);
    });
    animation.Play();
}

void ViewportPanel::AnimatePanelToggle(
    const Ref<GUI::Widget>& toggle,
    GUI::WidgetAnimation& animation,
    const bool panelOpen
)
{
    toggle->SetVisibility(GUI::EVisibility::HitTestInvisible);
    animation.ClearTracks();
    const float delay = panelOpen ? 0.0f : Styles().PanelToggleShowDelay;
    animation.AddTrack(
        MakeCurve(panelOpen ? 1.0f : 0.0f, panelOpen ? 0.0f : 1.0f, delay, Styles().PanelToggleDuration),
        [](GUI::Widget& widget, const float value) { widget.SetOpacity(value); }
    );
    animation.OnFinished([toggle, panelOpen]
    {
        toggle->SetVisibility(panelOpen ? GUI::EVisibility::Collapsed : GUI::EVisibility::Visible);
    });
    animation.Play();
}

void ViewportPanel::AddInspectorSectionHeader(
    const Ref<GUI::VerticalBox>& list,
    const std::string& name,
    bool* enabledValue,
    const char* iconPath
)
{
    const auto header = CreateRef<GUI::HorizontalBox>();
    header->SetStyle(Styles().PanelSection);
    header->SetPadding({ 10.0f, 0.0f });
    list->AddChild(header)
        .SetFixedSize(PanelHeaderHeight)
        .SetMargin(GUI::SMargin(0.0f, 4.0f, 0.0f, 2.0f))
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

    const auto disclosureIcon = CreateRef<GUI::Icon>();
    disclosureIcon->SetIcon(IconManager::Load("./Assets/Icons/chevron-right.svg"));
    disclosureIcon->SetSize({ 9.0f, 9.0f });
    disclosureIcon->SetStyle(Styles().SecondaryIcon);
    header->AddChild(disclosureIcon).SetMargin(GUI::SMargin(0.0f, 0.0f, 6.0f, 0.0f));

    if (iconPath)
    {
        const auto icon = CreateRef<GUI::Icon>();
        icon->SetIcon(IconManager::Load(iconPath));
        icon->SetSize({ 13.0f, 13.0f });
        icon->SetColor(GUI::EStyleLayer::Normal, { 0.922f, 0.694f, 0.286f, 1.0f });
        header->AddChild(icon).SetMargin(GUI::SMargin(0.0f, 0.0f, 6.0f, 0.0f));
    }

    const auto label = CreateRef<GUI::TextBlock>(name);
    label->SetColor(Styles().TextPrimary);
    label->SetFontSize(12.0f);
    header->AddChild(label);

    if (enabledValue)
    {
        const auto spacer = CreateRef<GUI::Canvas>();
        header->AddChild(spacer).SetFillSize();

        const auto checkbox = CreateRef<GUI::Checkbox>();
        checkbox->SetSize({ 13.0f, 13.0f });
        checkbox->SetStyle(Styles().Checkbox);
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
    labelText->SetColor(Styles().TextSecondary);
    labelText->SetFontSize(11.0f);
    row->AddChild(labelText)
        .SetFixedSize(LabelWidth)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Center);

    const auto field = CreateRef<GUI::TextField>(value);
    field->SetStyle(Styles().TextField);
    field->SetPadding({ 8.0f, 4.0f });
    field->SetCursorColor(Styles().TextPrimary);
    field->SetSelectionColor({ Styles().Accent.R, Styles().Accent.G, Styles().Accent.B, 0.35f });
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
    checkbox->SetStyle(Styles().Checkbox);
    checkbox->SetChecked(value);
    checkbox->OnCheckedChanged([&value](const bool checked) { value = checked; });
    row->AddChild(checkbox)
        .SetMargin(GUI::SMargin(0.0f, 0.0f, 8.0f, 0.0f))
        .SetVerticalAlignment(GUI::EVerticalAlignment::Center);

    const auto labelText = CreateRef<GUI::TextBlock>(label);
    labelText->SetColor(Styles().TextPrimary);
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
    labelText->SetColor(Styles().TextSecondary);
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
        chip->SetStyle(Styles().InspectorField);
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
        auto badgeStyle = Styles().Transparent;
        badgeStyle.Normal.Background.Color = { color.R, color.G, color.B, 0.2f };
        badge->SetStyle(badgeStyle);
        badge->SetPadding({ 5.0f, 4.0f });
        row->AddChild(badge);

        const auto axisLabel = CreateRef<GUI::TextBlock>(axis);
        axisLabel->SetColor(color);
        axisLabel->SetFontSize(11.0f);
        badge->AddChild(axisLabel);

        // Transparent background: the chip built above already supplies the outline/bg -
        // this field just needs to be able to take focus and text input over it.
        const auto field = CreateRef<GUI::TextField>(FormatFloat(component));
        field->SetStyle(Styles().TransparentTextField);
        field->SetCursorColor(Styles().TextPrimary);
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

    addAxis("X", Styles().AxisX, value.x);
    addAxis("Y", Styles().AxisY, value.y);
    addAxis("Z", Styles().AxisZ, value.z);
}
