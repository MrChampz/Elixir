#include "VFXPanel.h"
#include "../EditorStyles.h"
#include "../../VFX/VFXTimeline.h"

#include <Engine/Aether/Effect/Effect.h>
#include <Engine/Core/Platform.h>
#include <Engine/GUI/Button.h>
#include <Engine/GUI/Checkbox.h>
#include <Engine/GUI/Icon.h>
#include <Engine/Icon/IconManager.h>
#include <Engine/GUI/ScrollBox.h>
#include <Engine/GUI/TextField.h>
#include <Engine/GUI/Renderer/RenderBatch.h>
#include <Engine/Input/InputCodes.h>
#include <Engine/Input/InputManager.h>
#include <algorithm>
#include <array>
#include <format>
#include <cctype>
#include <cmath>
#include <utility>

using namespace EditorVFX;

namespace
{
    Ref<GUI::TextBlock> Text(const std::string& value, const float size = 12.0f)
    {
        const auto text = CreateRef<GUI::TextBlock>(value);
        text->SetFontSize(size); text->SetColor(EditorStyle::Get().TextPrimary);
        return text;
    }

    // Uses compact controls inside emitter stacks and toolbars.
    class StackButton final : public GUI::Button
    {
    public:
        /** @brief Creates a button sized for dense editor panels. */
        explicit StackButton(float scale = 1) : m_MinimumHeight(24 * scale) {}
    protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override
        {
            const auto padding = GetPadding();
            const glm::vec2 inset{ padding.GetTotalHorizontal(), padding.GetTotalVertical() };
            const auto content = GetContentSlot()->GetWidget();
            const auto desired = content ? content->Measure(glm::max(availableSize - inset, glm::vec2(0))) : glm::vec2(0);
            return glm::max(desired + inset, glm::vec2(0, m_MinimumHeight));
        }
    private:
        float m_MinimumHeight;
    };

    Ref<GUI::Button> Action(const std::string& label, std::function<void()> clicked, const float scale = 1)
    {
        const auto button = CreateRef<StackButton>(scale);
        button->SetStyle(EditorStyle::Get().PanelToggleButton);
        button->SetPadding({ 6.0f * scale, 4.0f * scale }); button->SetContent(Text(label, 11.0f * scale));
        button->OnClick(std::move(clicked)); return button;
    }

    void Row(const Ref<GUI::VerticalBox>& column, const Ref<GUI::Widget>& widget, const float height = 28.0f)
    {
        column->AddChild(widget).SetFixedSize(height).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
            .SetMargin({ 0.0f, 2.0f });
    }

    Ref<GUI::Icon> CreateIcon(const std::string& name, const float size = 12)
    {
        const auto icon = CreateRef<GUI::Icon>();
        icon->SetIcon(IconManager::Load((std::filesystem::path(ELIXIR_EDITOR_PROJECT_ROOT) / "Assets" / "Icons" / (name + ".svg")).string()));
        icon->SetStyle(EditorStyle::Get().SecondaryIcon); icon->SetSize({size, size});
        return icon;
    }

    SColor EmitterColor(const size_t index)
    {
        const float hue = std::fmod(0.08f + static_cast<float>(index) * 0.61803398875f, 1.0f);
        constexpr float saturation = 0.60f;
        constexpr float value = 0.78f;
        const float sectorPosition = hue * 6;
        const int sector = static_cast<int>(sectorPosition);
        const float fraction = sectorPosition - static_cast<float>(sector);
        const float low = value * (1 - saturation);
        const float falling = value * (1 - saturation * fraction);
        const float rising = value * (1 - saturation * (1 - fraction));
        switch (sector)
        {
            case 0: return {value, rising, low, 1};
            case 1: return {falling, value, low, 1};
            case 2: return {low, value, rising, 1};
            case 3: return {low, falling, value, 1};
            case 4: return {rising, low, value, 1};
            default: return {value, low, falling, 1};
        }
    }

    SButtonStyle StackHeaderStyle(const SColor color, const float radius)
    {
        auto style = EditorStyle::Get().PanelHeaderButton;
        style.Normal.Background.Color = color;
        style.Normal.Background.CornerRadius = {radius, radius, 0, 0};
        style.Hovered = style.Normal; style.Pressed = style.Normal; style.Focused = style.Normal;
        style.Disabled = style.Normal;
        return style;
    }

    Ref<GUI::Canvas> ColorMarker(const SColor color, const float size = 7)
    {
        const auto marker = CreateRef<GUI::Canvas>(); marker->SetSize({size, size});
        auto style = EditorStyle::Get().Transparent;
        style.Normal.Background.Color = color; style.Normal.Background.CornerRadius = glm::vec4(2);
        marker->SetStyle(style); return marker;
    }

    // Keeps the frame visible above preview images and colored stack headers.
    class SurfaceOutline final : public GUI::Widget
    {
    public:
        /** @brief Draws a surface border without intercepting input. */
        explicit SurfaceOutline(const SBrush& brush) : m_Brush(brush)
        { SetVisibility(GUI::EVisibility::HitTestInvisible); }
    protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return {}; }
        void BuildDrawCommands(GUI::RenderBatch& batch, const int zOrder) override
        { batch.AddRect(GetGeometry(), {}, m_Brush.CornerRadius, {}, {}, m_Brush.Outline, zOrder); }
    private:
        SBrush m_Brush;
    };

    Ref<GUI::Overlay> FramedStack(const Ref<GUI::VerticalBox>& column, const SWidgetStyle& style)
    {
        const auto surface = CreateRef<GUI::Overlay>(); surface->SetStyle(style);
        surface->AddChild(column).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill).SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
        surface->AddChild(CreateRef<SurfaceOutline>(style.Normal.Background))
            .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill).SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
        return surface;
    }

    Ref<GUI::VerticalBox> MakePanel(const Ref<GUI::Canvas>& root, const GUI::SAnchors anchors,
        const float left, const float top, const float right, const float bottom)
    {
        const auto surface = CreateRef<GUI::Overlay>();
        surface->SetStyle(EditorStyle::Get().Panel);
        auto& slot = root->AddChild(surface);
        slot.SetAnchors(anchors).SetOffsets(left, top, right, bottom);
        slot.SetPosition({ anchors.IsStretchingHorizontally() ? 0.0f : left,
            anchors.IsStretchingVertically() ? 0.0f : top });
        slot.SetSize({ anchors.IsStretchingHorizontally() ? 0.0f : right - left,
            anchors.IsStretchingVertically() ? 0.0f : bottom - top });
        const auto column = CreateRef<GUI::VerticalBox>();
        surface->AddChild(column).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
            .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
        surface->AddChild(CreateRef<SurfaceOutline>(EditorStyle::Get().Panel.Normal.Background))
            .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill).SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
        return column;
    }

    Ref<GUI::VerticalBox> ScrollList(const Ref<GUI::VerticalBox>& column)
    {
        const auto scroll = CreateRef<GUI::ScrollBox>();
        scroll->SetStyle(EditorStyle::Get().ScrollBar); scroll->SetSize({ 400.0f, 4000.0f });
        column->AddChild(scroll).SetFillSize().SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
        const auto list = CreateRef<GUI::VerticalBox>(); scroll->SetContent(list); return list;
    }

    Value Boolean(const bool checked)
    {
        Value value; value.Type = EValueType::Boolean; value.Text = checked ? "true" : "false"; return value;
    }

    Path Child(Path path, const std::string& key) { path.push_back(key); return path; }

    Ref<GUI::HorizontalBox> Buttons(const Ref<GUI::VerticalBox>& column)
    {
        const auto row = CreateRef<GUI::HorizontalBox>(); Row(column, row, 30.0f); return row;
    }

    void Section(const Ref<GUI::VerticalBox>& column, const std::string& title, const SColor color)
    {
        const auto text = Text(title, 11.0f); text->SetColor(color); Row(column, text, 24.0f);
    }

    // Provides a large graph canvas and reserves wheel gestures for document zoom.
    class GraphScrollBox final : public GUI::ScrollBox
    {
    protected:
        void LayoutChildren(const GUI::SRect& rect) override
        {
            GUI::ScrollBox::LayoutChildren(rect);
            if (!m_Positioned)
            {
                m_Positioned = true; SetScrollOffset({2000, 2000});
                GUI::ScrollBox::LayoutChildren(rect);
            }
        }
        GUI::SInputReply HandleMouseScrolled(const MouseScrolledEvent&) override
        { return GUI::SInputReply::Handled(); }
    private:
        bool m_Positioned = false;
    };

    // Draws the overview's grid without intercepting emitter-card input.
    class GraphGrid final : public GUI::Widget
    {
    public:
        /** @brief Aligns the grid with the document's navigation and zoom. */
        void SetNavigation(const glm::vec2 offset, const float zoom)
        {
            if (offset == m_Offset && zoom == m_Zoom) return;
            m_Offset = offset; m_Zoom = zoom; MarkRenderDirty();
        }
    protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override
        { return glm::min(availableSize, glm::vec2(4000)); }
        void BuildDrawCommands(GUI::RenderBatch& batch, const int zOrder) override
        {
            const auto rect = GetGeometry();
            const float spacing = 24 * m_Zoom;
            const float startX = std::fmod(-m_Offset.x, spacing);
            const float startY = std::fmod(-m_Offset.y, spacing);
            for (float x = startX; x < rect.Size.x; x += spacing)
                for (float y = startY; y < rect.Size.y; y += spacing)
                    batch.AddRect({ rect.Position + glm::vec2(x, y), { 1.5f, 1.5f } },
                        { 0.22f, 0.23f, 0.25f, 1 }, {}, {}, {}, {}, zOrder);

        }
    private:
        glm::vec2 m_Offset{};
        float m_Zoom = 1;
    };

    // Displays the preview target within its widget bounds.
    class PreviewSurface final : public GUI::Widget
    {
    public:
        /** @brief Updates the scene image displayed in the floating preview. */
        void SetSource(const Ref<Image>& image)
        {
            m_Image = image;
            MarkRenderDirty();
        }
    protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& size) override { return glm::min(size, glm::vec2(180, 180)); }
        void BuildDrawCommands(GUI::RenderBatch& batch, const int zOrder) override
        {
            if (m_Image)
            {
                const auto& brush = EditorStyle::Get().Panel.Normal.Background;
                const float inset = brush.Outline.Thickness;
                const glm::vec4 radius{0, 0, std::max(0.0f, brush.CornerRadius.z - inset),
                    std::max(0.0f, brush.CornerRadius.w - inset)};
                batch.AddImage(m_Image, GetGeometry(), {1, 1, 1, 1}, zOrder);
                batch.Clip(GetGeometry(), radius);
            }
        }
    private:
        Ref<Image> m_Image;
    };

    // Draws a time ruler and playhead using the same retained rendering path as other widgets.
    class TimeRuler final : public GUI::Widget
    {
    public:
        /** @brief Shows the pointer cursor over timeline navigation areas. */
        TimeRuler()
        {
            OnMouseEnter([] { Platform::Get().SetCursorShape(ECursorShape::Hand); });
            OnMouseLeave([] { Platform::Get().SetPreviousCursorShape(); });
        }
        /** @brief Sets the callback used to seek when the ruler or a track is dragged. */
        void OnSeek(std::function<void(float)> callback) { m_OnSeek = std::move(callback); }
        /** @brief Reports whether this ruler accepts timeline navigation. */
        bool CanHandleMouseInput() const override { return IsEnabled() && bool(m_OnSeek); }
        /** @brief Colors an emitter track over the preview simulation interval. */
        void SetTrack(const SColor color, std::vector<TimelineInterval> intervals)
        { m_TrackColor = color; m_Intervals = std::move(intervals); m_IsTrack = true; MarkRenderDirty(); }
        /** @brief Updates the playback marker in seconds. */
        void SetTime(const float time, const float duration)
        {
            if (time == m_Time && duration == m_Duration) return;
            m_Time = time; m_Duration = duration; MarkRenderDirty();
        }
    protected:
        GUI::SInputReply HandleMouseDown(const MouseButtonPressedEvent& event) override
        {
            if (event.GetMouseButton() != EE_MOUSE_BUTTON_LEFT || !CanHandleMouseInput() ||
                GetGeometry().Size.x <= 0) return GUI::SInputReply::Unhandled();
            m_Dragging = true;
            SeekAt(event.GetX());
            return GUI::SInputReply::HandledAndCaptured();
        }
        GUI::SInputReply HandleMouseMove(const MouseMovedEvent& event) override
        {
            if (!m_Dragging) return GUI::SInputReply::Unhandled();
            SeekAt(event.GetX());
            return GUI::SInputReply::Handled();
        }
        GUI::SInputReply HandleMouseUp(const MouseButtonReleasedEvent& event) override
        {
            if (event.GetMouseButton() != EE_MOUSE_BUTTON_LEFT || !m_Dragging)
                return GUI::SInputReply::Unhandled();
            m_Dragging = false;
            SeekAt(event.GetX());
            return GUI::SInputReply::Handled();
        }
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override
        { return { std::min(availableSize.x, 400.0f), 18.0f }; }
        void BuildDrawCommands(GUI::RenderBatch& batch, const int zOrder) override
        {
            const auto rect = GetGeometry();
            batch.AddRect(rect, { 0.10f, 0.11f, 0.13f, 1 }, {}, {}, {}, {}, zOrder);
            if (m_IsTrack)
                for (const auto& interval : m_Intervals)
                {
                    const float start = std::clamp(interval.Start / m_Duration, 0.0f, 1.0f);
                    const float end = std::clamp(interval.End / m_Duration, start, 1.0f);
                    batch.AddRect({rect.Position + glm::vec2(rect.Size.x * start, 5),
                        {rect.Size.x * (end - start), std::max(0.0f, rect.Size.y - 10)}},
                        m_TrackColor, glm::vec4(3), {}, {}, {}, zOrder + 1);
                }
            for (int tick = 0; !m_IsTrack && tick <= 20; ++tick)
            {
                const auto x = rect.Position.x + rect.Size.x * tick / 20.0f;
                batch.AddRect({ { x, rect.Position.y }, { 1, tick % 4 == 0 ? rect.Size.y : 5.0f } },
                    { 0.24f, 0.26f, 0.30f, 1 }, {}, {}, {}, {}, zOrder + 1);
            }
            const auto x = rect.Position.x + rect.Size.x * std::clamp(m_Time / m_Duration, 0.0f, 1.0f);
            batch.AddRect({ { x, rect.Position.y }, { 2, rect.Size.y } }, EditorStyle::Get().TextPrimary,
                {}, {}, {}, {}, zOrder + 2);
        }
    private:
        // Maps captured pointer movement to a bounded time, including outside the track.
        void SeekAt(const float x)
        {
            const auto rect = GetGeometry();
            if (!m_OnSeek || rect.Size.x <= 0) return;
            const float fraction = std::clamp((x - rect.Position.x) / rect.Size.x, 0.0f, 1.0f);
            m_OnSeek(fraction * m_Duration);
        }

        std::function<void(float)> m_OnSeek;
        bool m_Dragging = false;
        float m_Time = 0.0f, m_Duration = 5.0f;
        bool m_IsTrack = false;
        SColor m_TrackColor;
        std::vector<TimelineInterval> m_Intervals;
    };
}

VFXPanel::VFXPanel(EditorVFXManager& manager, GraphicsContext& context, std::filesystem::path projectRoot,
    const float aspectRatio, std::function<void(const std::string&)> statusChanged)
    : m_Manager(manager), m_Context(context), m_ProjectRoot(std::move(projectRoot)),
      m_StatusChanged(std::move(statusChanged)), m_Camera(60.0f, aspectRatio)
{
    m_Camera.SetDistance(5.0f);
}

VFXPanel::~VFXPanel() { if (m_Instance) m_Manager.Remove(m_Instance); }

Ref<GUI::Widget> VFXPanel::Build()
{
    const auto root = CreateRef<GUI::Canvas>(); root->SetStyle(EditorStyle::Get().Transparent);
    // The workbench occupies the complete document viewport; tools float above it.
    const auto workbench = CreateRef<GUI::Overlay>();
    workbench->SetStyle(EditorStyle::Get().Transparent);
    workbench->SetBackgroundColor(GUI::EStyleLayer::Normal, {0.05f, 0.055f, 0.065f, 1});
    root->AddChild(workbench).SetAnchors(GUI::SAnchors::StretchAll());
    m_Grid = CreateRef<GraphGrid>();
    workbench->AddChild(m_Grid).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    const auto graph = CreateRef<GraphScrollBox>(); graph->SetStyle(EditorStyle::Get().ScrollBar);
    graph->SetScrollAxis(GUI::EScrollAxis::Both); graph->SetSize({ 4000, 4000 });
    graph->SetShowScrollbar(false); m_Graph = graph;
    workbench->AddChild(graph).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    m_Overview = CreateRef<GUI::HorizontalBox>();
    m_Overview->SetPadding({ 2260, 2050, 2260, 2080 });
    graph->SetContent(m_Overview);

    const auto header = [](const Ref<GUI::VerticalBox>& column, const std::string& title, const std::string& iconName)
    {
        const auto heading = CreateRef<GUI::HorizontalBox>(); heading->SetPadding({8, 0});
        heading->AddChild(CreateIcon(iconName, 9)).SetFixedSize(9).SetMargin({0, 0, 6, 0});
        heading->AddChild(Text(title, 12)).SetFillSize();
        column->AddChild(heading).SetFixedSize(27).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
        const auto divider = CreateRef<GUI::Canvas>(); divider->SetStyle(EditorStyle::Get().PanelHeaderBorder);
        column->AddChild(divider).SetFixedSize(1).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
        return heading;
    };
    const auto tools = CreateRef<GUI::Overlay>(); tools->SetStyle(EditorStyle::Get().Toolbar); tools->SetPadding({4, 4});
    root->AddChild(tools).SetAnchors(GUI::SAnchors::TopCenter()).SetAlignment({0.5f, 0}).SetPosition({0, 10}).SetSize({160, 34});
    m_WorkbenchToolbar = tools;
    const auto toolbar = CreateRef<GUI::HorizontalBox>(); tools->AddChild(toolbar).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill).SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    const auto command = [&](const std::string& name, const std::string& label, std::function<void()> callback)
    {
        const auto button = Action("", std::move(callback)); button->SetStyle(EditorStyle::Get().PanelHeaderButton);
        const auto content = CreateRef<GUI::HorizontalBox>();
        content->AddChild(CreateIcon(name)).SetFixedSize(13).SetMargin({0, 0, 5, 0}); content->AddChild(Text(label, 11));
        button->SetContent(content); toolbar->AddChild(button).SetFillSize();
    };
    command("save", "Save", [this] { m_SaveRequested = true; });
    command("compile", "Compile", [this] { m_CompileRequested = true; });

    const auto modules = MakePanel(root, {0, 0, 0, 0}, 10, 10, 190, 44);
    m_ModuleToolbar = modules;
    const auto moduleTools = CreateRef<GUI::HorizontalBox>(); moduleTools->SetPadding({4, 4});
    modules->AddChild(moduleTools).SetFillSize().SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    moduleTools->AddChild(Action("+ Emitter", [this]
    {
        if (!m_Document) return;
        const auto name = UniqueEmitterName("Emitter");
        QueueEdit([name](Document& document) { document.Append({"emitters"}, MakeEmitter(name)); });
    })).SetFillSize();
    moduleTools->AddChild(Action("System", [this] { m_SelectedEmitter = -1; m_SelectedModule = -1; m_RebuildSelection = true; })).SetFixedSize(64);

    const auto preview = MakePanel(root, {0, 0, 0, 0}, 10, 56, 190, 264);
    m_PreviewSurface = preview;
    header(preview, "Preview", "eye");
    m_PreviewInput = CreateRef<PreviewSurface>();
    const float previewInset = EditorStyle::Get().Panel.Normal.Background.Outline.Thickness;
    preview->AddChild(m_PreviewInput).SetFillSize().SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
        .SetMargin({previewInset, 0, previewInset, previewInset});

    const auto selection = MakePanel(root, {1, 0, 1, 1}, -270, 10, -10, -10);
    m_InspectorSurface = selection;
    header(selection, "Module Properties", "chevron-right");
    m_Inspector = ScrollList(selection); m_Inspector->SetPadding({10, 8});

    const auto timeline = MakePanel(root, {0, 1, 1, 1}, 10, -190, -290, -14);
    m_TimelineSurface = timeline;
    const auto timelineHeading = header(timeline, "Timeline", "clock");
    m_Clock = Text("0.00s / 5.00s", 10); m_Clock->SetColor(EditorStyle::Get().TextSecondary);
    timelineHeading->AddChild(m_Clock);
    const auto playback = CreateRef<GUI::HorizontalBox>(); playback->SetPadding({6, 2});
    timeline->AddChild(playback).SetFixedSize(34).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    const auto transport = [&](const std::string& name, std::function<void()> callback)
    {
        const auto button = CreateRef<GUI::Canvas>(); button->SetSize({26, 26});
        button->SetStyle(EditorStyle::Get().ToolbarButtonInactive); button->OnClick(std::move(callback));
        button->AddChild(CreateIcon(name, 13)).SetAnchors(GUI::SAnchors::MiddleCenter())
            .SetAlignment({0.5f, 0.5f}).SetSize({13, 13});
        playback->AddChild(button).SetFixedSize(26).SetMargin({0, 0, 2, 0}); return button;
    };
    const auto seek = [this](const float frames)
    {
        std::lock_guard lock(m_PreviewMutex);
        const float current = m_SeekRequested.value_or(m_Playhead);
        m_SeekRequested = std::clamp(current + frames / 60.0f, 0.0f, m_LoopDuration);
        m_Playing = false;
    };
    transport("restart", [this]
    { std::lock_guard lock(m_PreviewMutex); m_SeekRequested = 0; m_Playing = false; });
    transport("fast-rewind", [seek] { seek(-10); });
    transport("step-backward", [seek] { seek(-1); });
    m_PlayButton = transport("play", [this] { std::lock_guard lock(m_PreviewMutex); m_Playing = true; });
    m_PauseButton = transport("pause", [this] { std::lock_guard lock(m_PreviewMutex); m_Playing = false; });
    transport("step-forward", [this] { std::lock_guard lock(m_PreviewMutex); m_Playing = false; m_StepRequested = true; });
    transport("fast-forward", [seek] { seek(10); });
    const auto speed = Action("1.00x", [this]
    {
        std::lock_guard lock(m_PreviewMutex);
        m_PlaybackSpeed = m_PlaybackSpeed < 4 ? m_PlaybackSpeed * 2 : 0.25f;
    });
    m_SpeedLabel = std::static_pointer_cast<GUI::TextBlock>(speed->GetContentSlot()->GetWidget());
    playback->AddChild(speed).SetFixedSize(56).SetMargin({8, 0, 8, 0});
    const auto loop = CreateRef<GUI::Checkbox>(); loop->SetStyle(EditorStyle::Get().Checkbox);
    loop->SetSize({16, 16}); loop->SetChecked(true);
    loop->OnCheckedChanged([this](const bool checked) { std::lock_guard lock(m_PreviewMutex); m_Loop = checked; });
    playback->AddChild(loop).SetFixedSize(16).SetVerticalAlignment(GUI::EVerticalAlignment::Center);
    playback->AddChild(Text("Loop", 11)).SetMargin({4, 0});
    const auto ruler = CreateRef<GUI::HorizontalBox>();
    ruler->AddChild(Text("Emitter", 10)).SetFixedSize(110);
    const auto rulerArea = CreateRef<GUI::Overlay>();
    ruler->AddChild(rulerArea).SetFillSize().SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    m_TimeBar = CreateRef<TimeRuler>();
    std::static_pointer_cast<TimeRuler>(m_TimeBar)->OnSeek([this](float time) { RequestSeek(time); });
    rulerArea->AddChild(m_TimeBar).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill).SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    const auto labels = CreateRef<GUI::Canvas>();
    labels->SetVisibility(GUI::EVisibility::HitTestInvisible);
    for (int second = 0; second <= 5; ++second)
    {
        const float anchor = second / 5.0f;
        labels->AddChild(Text(std::to_string(second) + "s", 10))
            .SetAnchors({anchor, 0, anchor, 0}).SetPosition({second == 5 ? -18.0f : 3.0f, 0}).SetSize({20, 18});
    }
    rulerArea->AddChild(labels).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill).SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    m_TimelineRuler = ruler;
    m_Tracks = ScrollList(timeline);
    m_Status = Text("Select an effect to begin authoring", 10);
    UpdateOverview(); UpdateInspector(); UpdateStatus("Opening effect...");
    RegisterOverlay(tools);
    RegisterOverlay(modules->GetParent());
    RegisterOverlay(preview->GetParent(), {-12, 0});
    RegisterOverlay(selection->GetParent(), {12, 0});
    RegisterOverlay(timeline->GetParent(), {0, 12});
    return root;
}

void VFXPanel::RequestSeek(const float time)
{
    std::lock_guard lock(m_PreviewMutex);
    m_SeekRequested = std::clamp(time, 0.0f, m_LoopDuration);
    m_Playing = false;
    m_LoopRestart = false;
}

void VFXPanel::QueueEdit(std::function<void(Document&)> edit) { m_EditCommand = std::move(edit); }

bool VFXPanel::ApplyDrafts()
{
    if (m_Drafts.empty() || !m_Document) return true;
    std::string error;
    if (!m_Document->Apply(m_Drafts, error)) { UpdateStatus("Invalid property: " + error); return false; }
    m_Drafts.clear(); m_NeedsCompile = true; UpdateOverview(); UpdateStatus("Changes pending compilation"); return true;
}

void VFXPanel::OnUpdate(const Timestep frameTime)
{
    const auto region = m_PreviewInput->GetGeometry();
    if (region.Size.x > 0 && region.Size.y > 0)
    {
        std::lock_guard lock(m_PreviewMutex); m_PreviewRect = region;
        m_Camera.GetPerspectiveCamera().SetAspectRatio(region.Size.x / region.Size.y);
    }
    if (!m_PendingPath.empty())
    {
        const auto path = std::exchange(m_PendingPath, {});
        if (ApplyDrafts()) OpenAsset(path);
    }
    if (m_EditCommand && m_Document)
    {
        auto command = std::exchange(m_EditCommand, {});
        if (ApplyDrafts())
        {
            command(*m_Document); m_NeedsCompile = true; m_EditDelay = 0.25f;
            m_PalettePhase.clear(); UpdateOverview(); UpdateInspector(); UpdateStatus("Changes pending compilation");
        }
    }
    if (std::exchange(m_RebuildSelection, false)) { if (ApplyDrafts()) { UpdateOverview(); UpdateInspector(); } }
    if (std::exchange(m_DiscardReloadRequested, false) && m_Document) OpenAsset(m_SelectedPath, true);
    if (std::exchange(m_ReloadRequested, false) && m_Document)
    {
        if (m_Document->Dirty() || !m_Drafts.empty())
        {
            m_ConfirmReload = true; UpdateInspector(); UpdateStatus("Reload will discard unsaved edits. Confirm in Details.");
        }
        else OpenAsset(m_SelectedPath, true);
    }
    m_EditDelay = std::max(0.0f, m_EditDelay - frameTime.GetSeconds());
    if (m_EditDelay == 0 && !m_Drafts.empty() && m_AutoCompile) ApplyDrafts();
    if (std::exchange(m_CompileRequested, false)) { if (ApplyDrafts()) CompilePreview(); }
    else if (m_NeedsCompile && m_AutoCompile && m_EditDelay == 0 && m_Drafts.empty()) CompilePreview();
    if (std::exchange(m_SaveRequested, false) && m_Document && ApplyDrafts())
    {
        if (m_AppliedDefinitions[m_SelectedPath] == m_Document->Root().JSON() || CompilePreview())
        {
            std::string error;
            UpdateStatus(m_Document->Save(error) ? "Saved " + m_SelectedPath.filename().string() : error);

        }
    }
    bool loopRestart = false;
    std::optional<float> seekTime;
    {
        std::lock_guard lock(m_PreviewMutex);
        loopRestart = std::exchange(m_LoopRestart, false);
        seekTime = std::exchange(m_SeekRequested, std::nullopt);
        const float displayedTime = seekTime.value_or(m_Playhead);
        m_Clock->SetText(std::format("{:.2f}s / {:.2f}s", displayedTime, m_LoopDuration));
        m_SpeedLabel->SetText(std::format("{:.2f}x", m_PlaybackSpeed));
        std::static_pointer_cast<TimeRuler>(m_TimeBar)->SetTime(displayedTime, m_LoopDuration);
        for (const auto& track : m_TrackBars) std::static_pointer_cast<TimeRuler>(track)->SetTime(displayedTime, m_LoopDuration);
        m_PlayButton->SetStyle(m_Playing ? EditorStyle::Get().PlayButtonActive : EditorStyle::Get().PlayButtonInactive);
        m_PauseButton->SetStyle(m_Playing ? EditorStyle::Get().PauseButtonInactive : EditorStyle::Get().PauseButtonActive);
    }
    if (seekTime)
    {
        if (RestartPreview())
        {
            std::lock_guard lock(m_PreviewMutex);
            m_ReplayTime = *seekTime; m_Playhead = *seekTime; m_Playing = false;
        }
    }
    else if (loopRestart) RestartPreview();
    std::static_pointer_cast<GraphGrid>(m_Grid)->SetNavigation(m_Graph->GetScrollOffset(), m_GraphZoom);
}

void VFXPanel::OnActivated(const bool active)
{
    std::lock_guard lock(m_PreviewMutex); m_Active = active;
    if (active && m_Status) UpdateStatus(std::string(m_Status->GetText()));
    if (!active) m_GraphPanning = false;
    if (!active)
    {
        MouseButtonReleasedEvent orbit(EE_MOUSE_BUTTON_LEFT), pan(EE_MOUSE_BUTTON_MIDDLE);
        m_Camera.ProcessEvent(orbit); m_Camera.ProcessEvent(pan);
        m_OrbitDragging = false; m_PanDragging = false;
    }
}

void VFXPanel::OpenAsset(const std::filesystem::path& path, const bool reload)
{
    auto session = m_Documents.find(path);
    if (reload || session == m_Documents.end())
    {
        Document loaded; std::string error;
        if (!loaded.Load(path, error)) { UpdateStatus(error); return; }
        // Validate before replacing any unsaved session or preview.
        if (!Aether::Effect::LoadEffectText(loaded.Root().JSON(), &error)) { UpdateStatus(error); return; }
        session = m_Documents.insert_or_assign(path, std::move(loaded)).first;
    }
    m_Document = &session->second; m_SelectedPath = path; m_Drafts.clear();
    m_SelectedEmitter = -1; m_SelectedModule = -1; m_SelectedPhase.clear(); m_PalettePhase.clear(); m_ConfirmReload = false;
    const auto emitters = m_Document->Root().Find("emitters");
    if (emitters && !emitters->Array.empty()) m_SelectedEmitter = 0;
    const auto cached = m_LoadedSystems.find(path);
    if (cached != m_LoadedSystems.end())
    {
        m_System = cached->second;
        m_NeedsCompile = m_AppliedDefinitions[path] != m_Document->Root().JSON();
        if (m_NeedsCompile) CompilePreview(); else RestartPreview();
    }
    else { m_NeedsCompile = true; CompilePreview(); }
    UpdateOverview(); UpdateInspector();
}

bool VFXPanel::CompilePreview()
{
    if (!m_Document) return false;
    m_NeedsCompile = false;
    std::string error;
    std::vector<std::optional<LeafSettings>> leaves;
    if (!ParseLeafRenderers(m_Document->Root(), leaves, error))
    { UpdateStatus("Compile failed: " + error); return false; }
    std::vector<std::optional<BalloonSettings>> balloons;
    if (!ParseBalloonRenderers(m_Document->Root(), balloons, error))
    { UpdateStatus("Compile failed: " + error); return false; }
    const auto replacement = Aether::Effect::LoadEffectText(m_Document->Root().JSON(), &error);
    if (!replacement) { UpdateStatus("Compile failed: " + error); return false; }
    const auto existing = m_LoadedSystems.find(m_SelectedPath);
    const auto current = existing == m_LoadedSystems.end() ? nullptr : existing->second;
    for (const auto& emitter : replacement->GetEmitters())
    {
        if (const auto& description = emitter->GetMaterialDescription(); description && !description->BaseColorTexturePath.empty())
        {
            auto adjusted = *description;
            const std::filesystem::path texture(adjusted.BaseColorTexturePath);
            if (texture.is_relative()) adjusted.BaseColorTexturePath = (m_ProjectRoot / texture).lexically_normal().string();
            emitter->SetMaterialDescription(std::move(adjusted));
        }
        if (current)
            if (const auto old = current->FindEmitter(emitter->GetName()); old && old->GetRenderMode() == emitter->GetRenderMode())
            {
                const auto& previous = old->GetMaterialDescription(); const auto& next = emitter->GetMaterialDescription();
                const bool unchanged = (!previous && !next) || (previous && next && previous->BaseColor == next->BaseColor &&
                    previous->Opacity == next->Opacity && previous->Emissive == next->Emissive && previous->BaseColorTexturePath == next->BaseColorTexturePath);
                if (unchanged) emitter->SetMaterial(old->GetMaterial());
            }
    }
    bool compiled = false;
    const auto system = current ? current : replacement;
    const bool completed = m_Context.RunRenderTaskAndWait([this, &compiled, &leaves, &balloons, system, replacement, current]
    {
        if (current) current->Swap(*replacement);
        compiled = m_Manager.Recompile(system);
        if (!compiled && current) current->Swap(*replacement);
        if (compiled)
        {
            m_Manager.SetLeafRenderers(std::move(leaves));
            m_Manager.SetBalloonRenderers(std::move(balloons));
        }
    });
    if (!completed || !compiled) { UpdateStatus("Could not compile the effect. Previous preview preserved."); return false; }
    m_System = system; m_LoadedSystems.insert_or_assign(m_SelectedPath, system);
    m_AppliedDefinitions[m_SelectedPath] = m_Document->Root().JSON();
    RestartPreview(); UpdateStatus("Compiled successfully"); return true;
}

bool VFXPanel::RestartPreview()
{
    if (!m_System) return false;
    Ref<Aether::SystemInstance> instance;
    bool activated = m_System->GetEmitters().empty();
    if (!activated)
    {
        instance = m_System->CreateInstance();
        const bool completed = m_Context.RunRenderTaskAndWait([this, &activated, instance] { activated = m_Manager.Add(instance); });
        if (!completed || !activated) { UpdateStatus("Could not activate the preview. See the log."); return false; }
    }
    std::lock_guard lock(m_PreviewMutex);
    if (m_Instance) m_Manager.Remove(m_Instance);
    m_Instance = instance; m_ReplayTime = 0; m_Playhead = 0; m_Playing = true; m_StepRequested = false; m_LoopRestart = false;
    return true;
}

std::string VFXPanel::UniqueEmitterName(const std::string& base) const
{
    if (!m_Document) return base;
    const auto emitters = m_Document->Root().Find("emitters");
    auto candidate = base;
    for (int suffix = 1; ; ++suffix)
    {
        const bool exists = std::any_of(emitters->Array.begin(), emitters->Array.end(), [&candidate](const Value& emitter) { return emitter.String("name") == candidate; });
        if (!exists) return candidate;
        candidate = base + std::to_string(suffix);
    }
}

void VFXPanel::UpdateOverview()
{
    m_Overview->ClearChildren(); m_Tracks->ClearChildren(); m_TrackBars.clear();
    m_Tracks->AddChild(m_TimelineRuler).SetFixedSize(18).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    if (!m_Document) { m_Overview->AddChild(Text("Open an effect or create a new system.")); return; }
    const auto graphRow = [this](const Ref<GUI::VerticalBox>& column, const Ref<GUI::Widget>& widget, float height = 28)
    {
        column->AddChild(widget).SetFixedSize(height * m_GraphZoom)
            .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill).SetMargin({0, m_GraphZoom});
    };
    const auto graphText = [this](const std::string& text, float size = 12)
    { return Text(text, size * m_GraphZoom); };
    const auto graphAction = [this](const std::string& label, std::function<void()> callback)
    {
        const auto button = CreateRef<StackButton>(0.7f * m_GraphZoom);
        button->SetStyle(EditorStyle::Get().PanelHeaderButton); button->SetPadding({4 * m_GraphZoom, 0});
        button->SetContent(Text(label, 11 * m_GraphZoom)).SetHorizontalAlignment(GUI::EHorizontalAlignment::Left);
        button->OnClick(std::move(callback)); return button;
    };
    auto sectionStyle = EditorStyle::Get().PanelSection;
    sectionStyle.Normal.Background.Color = {0.216f, 0.227f, 0.251f, 1};
    sectionStyle.Normal.Background.Outline = {{0.31f, 0.33f, 0.35f, 1}, m_GraphZoom};
    sectionStyle.Normal.Background.CornerRadius = glm::vec4(4 * m_GraphZoom);
    const auto graphSection = [&](const Ref<GUI::VerticalBox>& column, const std::string& title,
        const SColor color, std::function<void()> addModule = {})
    {
        const auto text = graphText(title, 11); text->SetColor(color);
        const auto heading = CreateRef<GUI::HorizontalBox>(); heading->SetPadding({8 * m_GraphZoom, 0});
        heading->AddChild(text).SetFillSize();
        if (addModule)
        {
            const auto add = graphAction("+", std::move(addModule));
            add->SetPadding({0, 0});
            add->SetContent(graphText("+", 13)).SetHorizontalAlignment(GUI::EHorizontalAlignment::Right);
            heading->AddChild(add).SetFixedSize(18 * m_GraphZoom)
                .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
        }
        heading->SetBackgroundColor(GUI::EStyleLayer::Normal,
            {0.07f + color.R * 0.16f, 0.07f + color.G * 0.16f, 0.07f + color.B * 0.16f, 1});
        heading->SetCornerRadius(GUI::EStyleLayer::Normal, {4 * m_GraphZoom, 4 * m_GraphZoom, 0, 0});
        column->AddChild(heading).SetFixedSize(22 * m_GraphZoom).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    };
    const auto system = CreateRef<GUI::VerticalBox>();
    auto systemStyle = EditorStyle::Get().PanelSection;
    systemStyle.Normal.Background.CornerRadius = glm::vec4(6 * m_GraphZoom);
    systemStyle.Normal.Background.Outline = {EditorStyle::Get().PanelHeaderBorder.Normal.Background.Color, 1};
    const auto systemHeader = graphAction(m_Document->Root().String("name"), [this]
    { m_SelectedEmitter = -1; m_SelectedModule = -1; m_RebuildSelection = true; });
    systemHeader->SetStyle(StackHeaderStyle({0.12f, 0.35f, 0.40f, 1}, 6 * m_GraphZoom));
    system->AddChild(systemHeader).SetFixedSize(26 * m_GraphZoom).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    const auto systemBody = CreateRef<GUI::VerticalBox>(); systemBody->SetPadding({6 * m_GraphZoom, 6 * m_GraphZoom});
    system->AddChild(systemBody).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    for (const auto* title : {"Properties", "User Parameters"})
    {
        const auto button = graphAction(title, [this]
        { m_SelectedEmitter = -1; m_SelectedModule = -1; m_RebuildSelection = true; });
        const auto content = CreateRef<GUI::HorizontalBox>();
        content->AddChild(CreateIcon(std::string(title) == "Properties" ? "edit" : "user", 11 * m_GraphZoom))
            .SetFixedSize(11 * m_GraphZoom).SetMargin({0, 0, 5 * m_GraphZoom, 0});
        content->AddChild(graphText(title, 11)).SetFillSize();
        button->SetContent(content).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
        graphRow(systemBody, button, 24);
    }
    const auto systemState = CreateRef<GUI::VerticalBox>();
    systemBody->AddChild(FramedStack(systemState, sectionStyle))
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill).SetMargin({0, 3 * m_GraphZoom});
    graphSection(systemState, "System State", {0.35f, 0.70f, 0.75f, 1});
    systemState->AddChild(graphText("GPU simulation", 10)).SetFixedSize(22 * m_GraphZoom)
        .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill).SetMargin({8 * m_GraphZoom, 2 * m_GraphZoom});
    m_Overview->AddChild(FramedStack(system, systemStyle)).SetFixedSize(180 * m_GraphZoom)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Top).SetMargin({4, 4});
    const auto& emitters = m_Document->Root().Find("emitters")->Array;
    if (m_SelectedEmitter >= static_cast<int>(emitters.size())) { m_SelectedEmitter = -1; m_SelectedModule = -1; }
    for (size_t index = 0; index < emitters.size(); ++index)
    {
        const auto& emitter = emitters[index];
        const auto path = Path{ "emitters", std::to_string(index) };
        const auto card = CreateRef<GUI::VerticalBox>();
        auto cardStyle = EditorStyle::Get().PanelSection;
        cardStyle.Normal.Background.CornerRadius = glm::vec4(6 * m_GraphZoom);
        cardStyle.Normal.Background.Outline = {m_SelectedEmitter == static_cast<int>(index)
            ? EditorStyle::Get().Accent : EditorStyle::Get().PanelHeaderBorder.Normal.Background.Color, 1};

        const auto header = CreateRef<GUI::HorizontalBox>(); header->SetPadding({8 * m_GraphZoom, 0});
        card->AddChild(header).SetFixedSize(26 * m_GraphZoom).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
        const auto body = CreateRef<GUI::VerticalBox>(); body->SetPadding({6 * m_GraphZoom, 6 * m_GraphZoom});
        card->AddChild(body).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
        const auto toggle = CreateRef<GUI::Checkbox>(); toggle->SetStyle(EditorStyle::Get().Checkbox); toggle->SetChecked(emitter.Enabled());
        toggle->SetSize({14 * m_GraphZoom, 14 * m_GraphZoom});
        toggle->OnCheckedChanged([this, path](const bool enabled) { QueueEdit([path, enabled](Document& document) { document.Set(Child(path, "enabled"), Boolean(enabled)); }); });
        header->AddChild(toggle).SetFixedSize(14 * m_GraphZoom)
            .SetVerticalAlignment(GUI::EVerticalAlignment::Center).SetMargin({0, 0, 6 * m_GraphZoom, 0});
        header->AddChild(graphAction(emitter.String("name"), [this, index] { m_SelectedEmitter = static_cast<int>(index); m_SelectedModule = -1; m_RebuildSelection = true; })).SetFillSize();
        const auto color = EmitterColor(index);
        header->SetBackgroundColor(GUI::EStyleLayer::Normal, color);
        header->SetCornerRadius(GUI::EStyleLayer::Normal, {6 * m_GraphZoom, 6 * m_GraphZoom, 0, 0});
        const auto spawn = CreateRef<GUI::VerticalBox>(); spawn->SetStyle(EditorStyle::Get().Transparent);
        spawn->SetPadding({0, 0});
        graphSection(spawn, "Emitter Spawn", EditorStyle::Get().AxisY);
        const auto rate = emitter.Find("spawnRate");
        std::string rateText = rate ? rate->Text : "0";
        if (rateText.starts_with('$'))
        {
            const auto key = rateText.substr(1);
            const auto parameters = emitter.Find("parameters");
            const auto globals = m_Document->Root().Find("parameters");
            const auto resolved = parameters && parameters->Find(key) ? parameters->Find(key) : globals ? globals->Find(key) : nullptr;
            if (resolved) rateText = resolved->Text;
        }
        graphRow(spawn, graphAction("Rate " + rateText + " / sec", [this, index]
        { m_SelectedEmitter = static_cast<int>(index); m_SelectedModule = -1; m_RebuildSelection = true; }), 24);
        body->AddChild(FramedStack(spawn, sectionStyle)).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill).SetMargin({0, 3 * m_GraphZoom});
        for (const auto& phase : { std::string("spawnModules"), std::string("updateModules") })
        {
            const auto group = CreateRef<GUI::VerticalBox>();
            body->AddChild(FramedStack(group, sectionStyle)).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill).SetMargin({0, 3 * m_GraphZoom});
            const auto modules = emitter.Find(phase);
            const auto phaseTitle = phase == "spawnModules" ? "Particle Spawn" : "Particle Update";
            const auto phaseColor = phase == "spawnModules" ? EditorStyle::Get().AxisY : EditorStyle::Get().AxisZ;
            graphSection(group, phaseTitle, phaseColor, [this, index, phase]
            {
                m_SelectedEmitter = static_cast<int>(index); m_PalettePhase = phase; m_RebuildSelection = true;
            });
            const auto moduleRows = CreateRef<GUI::VerticalBox>();
            moduleRows->SetPadding({4 * m_GraphZoom, 3 * m_GraphZoom});
            group->AddChild(moduleRows).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
            if (modules)
                for (size_t moduleIndex = 0; moduleIndex < modules->Array.size(); ++moduleIndex)
                {
                    const auto& module = modules->Array[moduleIndex];
                    const auto button = graphAction((module.Enabled() ? "" : "[off] ") + module.String("type"), [this, index, moduleIndex, phase]
                    {
                        m_SelectedEmitter = static_cast<int>(index); m_SelectedModule = static_cast<int>(moduleIndex);
                        m_SelectedPhase = phase; m_RebuildSelection = true;
                    });
                    if (m_SelectedEmitter == static_cast<int>(index) && m_SelectedModule == static_cast<int>(moduleIndex) && m_SelectedPhase == phase)
                    {
                        auto selectionStyle = EditorStyle::Get().PanelHeaderButton;
                        selectionStyle.Normal.Background = EditorStyle::Get().Selection.Normal.Background;
                        selectionStyle.Normal.Background.CornerRadius = glm::vec4(3 * m_GraphZoom);
                        selectionStyle.Hovered = selectionStyle.Normal;
                        selectionStyle.Pressed = selectionStyle.Normal;
                        selectionStyle.Focused = selectionStyle.Normal;
                        button->SetStyle(selectionStyle);
                    }
                    graphRow(moduleRows, button, 18);
                }
        }
        const auto particleState = CreateRef<GUI::VerticalBox>();
        body->AddChild(FramedStack(particleState, sectionStyle))
            .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill).SetMargin({0, 3 * m_GraphZoom});
        graphSection(particleState, "Particle State", EditorStyle::Get().AxisZ);
        graphRow(particleState, ParticleStateControl(emitter, path, m_GraphZoom), 24);

        const auto renderer = CreateRef<GUI::VerticalBox>();
        body->AddChild(FramedStack(renderer, sectionStyle)).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill).SetMargin({0, 3 * m_GraphZoom});
        graphSection(renderer, emitter.String("renderMode", "Sprite") + " Renderer", {0.65f, 0.48f, 0.85f, 1});
        graphRow(renderer, graphAction("Renderer properties", [this, index]
        {
            m_SelectedEmitter = static_cast<int>(index); m_SelectedModule = -1; m_RebuildSelection = true;
        }), 28);
        if (emitter.Find("leafRenderer"))
        {
            const auto leaves = CreateRef<GUI::VerticalBox>();
            body->AddChild(FramedStack(leaves, sectionStyle)).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
                .SetMargin({0, 3 * m_GraphZoom});
            graphSection(leaves, "Leaf Renderer (Editor)", {0.35f, 0.75f, 0.4f, 1});
            graphRow(leaves, graphAction("Renderer properties", [this, index]
            { m_SelectedEmitter = static_cast<int>(index); m_SelectedModule = -1; m_RebuildSelection = true; }), 28);
        }
        if (emitter.Find("balloonRenderer"))
        {
            const auto leaves = CreateRef<GUI::VerticalBox>();
            body->AddChild(FramedStack(leaves, sectionStyle)).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
                .SetMargin({0, 3 * m_GraphZoom});
            graphSection(leaves, "Balloon Renderer (Editor)", {0.35f, 0.75f, 0.4f, 1});
            graphRow(leaves, graphAction("Renderer properties", [this, index]
            { m_SelectedEmitter = static_cast<int>(index); m_SelectedModule = -1; m_RebuildSelection = true; }), 28);
        }
        m_Overview->AddChild(FramedStack(card, cardStyle)).SetFixedSize(190 * m_GraphZoom).SetVerticalAlignment(GUI::EVerticalAlignment::Top).SetMargin({ 4, 4 });
        const auto track = CreateRef<GUI::HorizontalBox>();
        track->SetStyle(m_SelectedEmitter == static_cast<int>(index) ? EditorStyle::Get().Selection : EditorStyle::Get().Transparent);
        const auto label = Action(emitter.String("name"), [this, index]
        { m_SelectedEmitter = static_cast<int>(index); m_SelectedModule = -1; m_RebuildSelection = true; });
        label->SetStyle(EditorStyle::Get().PanelHeaderButton);
        label->GetContentSlot()->SetHorizontalAlignment(GUI::EHorizontalAlignment::Left);
        const auto trackLabel = CreateRef<GUI::HorizontalBox>(); trackLabel->SetPadding({8, 0});
        trackLabel->AddChild(ColorMarker(color)).SetFixedSize(7).SetMargin({0, 0, 6, 0});
        trackLabel->AddChild(label).SetFillSize();
        track->AddChild(trackLabel).SetFixedSize(110);
        const auto bar = CreateRef<TimeRuler>();
        bar->OnSeek([this](float time) { RequestSeek(time); });
        auto trackColor = color; trackColor.A = emitter.Enabled() ? 0.8f : 0.2f; bar->SetTrack(trackColor, EmitterTimeline(m_Document->Root(), index, m_LoopDuration));
        track->AddChild(bar).SetFillSize().SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
        m_TrackBars.push_back(bar);
        m_Tracks->AddChild(track).SetFixedSize(26).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);

    }
}

void VFXPanel::AddProperties(const Ref<GUI::VerticalBox>& column, const Value& value, const Path& path, const std::string& label)
{
    if (value.Type == EValueType::Object)
    {
        if (!label.empty()) Section(column, label, EditorStyle::Get().TextSecondary);
        const std::vector<std::string> priority{ "name", "renderMode", "maxParticles", "spawnRate", "parameters", "material", "burst", "trigger" };
        for (const auto& key : priority)
            if (const auto child = value.Find(key)) AddProperties(column, *child, Child(path, key), key);
        for (const auto& [key, child] : value.Object)
        {
            if (key == "type" || key == "enabled" || key == "spawnModules" || key == "updateModules" || key == "emitters" ||
                std::find(priority.begin(), priority.end(), key) != priority.end()) continue;
            AddProperties(column, child, Child(path, key), key);
        }
        return;
    }
    if (value.Type == EValueType::Array && !value.Array.empty() &&
        (value.Array.front().Type == EValueType::Object || value.Array.front().Type == EValueType::Array))
    {
        Section(column, label, EditorStyle::Get().TextSecondary);
        for (size_t index = 0; index < value.Array.size(); ++index)
            AddProperties(column, value.Array[index], Child(path, std::to_string(index)), std::to_string(index));
        return;
    }
    const auto property = CreateRef<GUI::HorizontalBox>();
    const auto caption = Text(label, 11); caption->SetColor(EditorStyle::Get().TextSecondary);
    property->AddChild(caption).SetFixedSize(80);
    const auto input = CreateRef<GUI::TextField>(value.Type == EValueType::String ? value.Text : value.JSON());
    input->SetStyle(EditorStyle::Get().TextField); input->SetFontSize(11); input->SetPadding({ 6, 4 });
    input->OnChange([this, path](const std::string& text)
    {
        // Materialize optional strand defaults only when the user edits them.
        if (m_Document && !m_Document->Get(path) && path.size() == 4 && path[2] == "strand")
        {
            Value replacement; std::string error;
            if (!Value::Parse(text, replacement, error) || replacement.Type != EValueType::Number) return;
            QueueEdit([path, replacement](Document& document)
            {
                const Path emitterPath{path[0], path[1]};
                const auto existing = document.Get(emitterPath);
                if (!existing) return;
                auto emitter = *existing;
                auto& strand = emitter.Object["strand"];
                strand.Type = EValueType::Object;
                strand.Object[path[3]] = replacement;
                document.Set(emitterPath, std::move(emitter));
            });
            return;
        }
        const auto existing = std::find_if(m_Drafts.begin(), m_Drafts.end(), [&path](const Edit& edit) { return edit.Location == path; });
        if (existing == m_Drafts.end()) m_Drafts.push_back({ path, text }); else existing->Text = text;
        m_EditDelay = 0.6f;
    });
    property->AddChild(input).SetFillSize().SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    Row(column, property, 24);
}

Ref<GUI::Widget> VFXPanel::ParticleStateControl(const Value& emitter, const Path& emitterPath, const float scale)
{
    const auto state = emitter.Find("particleState");
    const auto expiry = state ? state->Find("killOnLifetimeExpired") : nullptr;
    const bool enabled = !expiry || expiry->Type != EValueType::Boolean || expiry->Text != "false";
    const auto row = CreateRef<GUI::HorizontalBox>();
    row->SetPadding({6 * scale, 0});
    const auto checkbox = CreateRef<GUI::Checkbox>();
    checkbox->SetStyle(EditorStyle::Get().Checkbox);
    checkbox->SetSize({14 * scale, 14 * scale});
    checkbox->SetChecked(enabled);
    const auto statePath = Child(emitterPath, "particleState");
    checkbox->OnCheckedChanged([this, statePath](const bool checked)
    {
        QueueEdit([statePath, checked](Document& document)
        {
            const auto existing = document.Get(statePath);
            auto value = existing ? *existing : Value{};
            value.Type = EValueType::Object;
            value.Object["killOnLifetimeExpired"] = Boolean(checked);
            // Write the parent object once so a missing section is one undo step.
            document.Set(statePath, std::move(value));
        });
    });
    row->AddChild(checkbox).SetFixedSize(14 * scale)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Center).SetMargin({0, 0, 6 * scale, 0});
    row->AddChild(Text("Kill on lifetime", 11 * scale)).SetFillSize()
        .SetVerticalAlignment(GUI::EVerticalAlignment::Center);
    return row;
}

void VFXPanel::UpdateInspector()
{
    m_Inspector->ClearChildren();
    if (!m_Document) { Row(m_Inspector, Text("Select a system, emitter, or module.", 11)); return; }
    if (m_ConfirmReload)
    {
        Row(m_Inspector, Text("Discard unsaved changes?", 12));
        Row(m_Inspector, Action("Discard and reload", [this] { m_Drafts.clear(); m_DiscardReloadRequested = true; }));
        Row(m_Inspector, Action("Keep editing", [this] { m_ConfirmReload = false; m_RebuildSelection = true; })); return;
    }
    const auto emitters = m_Document->Root().Find("emitters");
    if (m_SelectedEmitter < 0 || m_SelectedEmitter >= static_cast<int>(emitters->Array.size()))
    {
        Row(m_Inspector, Text("System properties", 13)); AddProperties(m_Inspector, m_Document->Root(), {});
        Row(m_Inspector, Action("+ User parameter", [this]
        {
            QueueEdit([](Document& document)
            {
                auto parameters = document.Root().Find("parameters") ? *document.Root().Find("parameters") : Value{};
                parameters.Type = EValueType::Object;
                std::string name = "Parameter";
                for (int index = 1; parameters.Object.contains(name); ++index) name = "Parameter" + std::to_string(index);
                Value value; value.Type = EValueType::Number; value.Text = "1"; parameters.Object[name] = value;
                document.Set({ "parameters" }, std::move(parameters));
            });
        }));
        return;
    }
    const auto emitterPath = Path{ "emitters", std::to_string(m_SelectedEmitter) };
    const auto& emitter = emitters->Array[m_SelectedEmitter];
    const auto emitterLabel = CreateRef<GUI::HorizontalBox>();
    emitterLabel->AddChild(ColorMarker(EmitterColor(static_cast<size_t>(m_SelectedEmitter))))
        .SetFixedSize(7).SetMargin({0, 0, 6, 0});
    emitterLabel->AddChild(Text(emitter.String("name"), 11)).SetFillSize();
    Row(m_Inspector, emitterLabel, 24);
    if (!m_PalettePhase.empty())
    {
        Row(m_Inspector, Text(m_PalettePhase == "spawnModules" ? "Add spawn module" : "Add update module", 13));
        const auto phasePath = Child(emitterPath, m_PalettePhase);
        for (const auto& module : ModuleTemplates(m_PalettePhase == "spawnModules"))
            Row(m_Inspector, Action(module.String("type"), [this, phasePath, module]
            {
                QueueEdit([phasePath, module](Document& document)
                {
                    if (!document.Get(phasePath)) { Value list; list.Type = EValueType::Array; document.Set(phasePath, list); }
                    document.Append(phasePath, module);
                });
            }));
        Row(m_Inspector, Action("Cancel", [this] { m_PalettePhase.clear(); m_RebuildSelection = true; })); return;
    }
    const auto moduleList = emitter.Find(m_SelectedPhase);
    if (m_SelectedModule >= 0 && moduleList && m_SelectedModule < static_cast<int>(moduleList->Array.size()))
    {
        const auto& module = moduleList->Array[m_SelectedModule];
        const auto listPath = Child(emitterPath, m_SelectedPhase);
        const auto modulePath = Child(listPath, std::to_string(m_SelectedModule));
        Row(m_Inspector, Text(module.String("type"), 13));
        Row(m_Inspector, Text(emitter.String("name") + " / " + (m_SelectedPhase == "spawnModules" ? "Spawn" : "Update"), 10));
        const auto commands = Buttons(m_Inspector);
        const auto index = static_cast<size_t>(m_SelectedModule);
        commands->AddChild(Action("Up", [this, listPath, index] { QueueEdit([this, listPath, index](Document& document) { if (document.Move(listPath, index, -1)) --m_SelectedModule; }); })).SetFillSize();
        commands->AddChild(Action("Down", [this, listPath, index] { QueueEdit([this, listPath, index](Document& document) { if (document.Move(listPath, index, 1)) ++m_SelectedModule; }); })).SetFillSize();
        commands->AddChild(Action("Copy", [this, listPath, module] { QueueEdit([listPath, module](Document& document) { document.Append(listPath, module); }); })).SetFillSize();
        commands->AddChild(Action("Delete", [this, listPath, index] { QueueEdit([this, listPath, index](Document& document) { document.Remove(listPath, index); m_SelectedModule = -1; }); })).SetFillSize();
        const auto enabled = CreateRef<GUI::Checkbox>(); enabled->SetStyle(EditorStyle::Get().Checkbox); enabled->SetChecked(module.Enabled());
        enabled->OnCheckedChanged([this, modulePath](const bool checked) { QueueEdit([modulePath, checked](Document& document) { document.Set(Child(modulePath, "enabled"), Boolean(checked)); }); });
        const auto toggleRow = Buttons(m_Inspector); toggleRow->AddChild(enabled).SetFixedSize(25); toggleRow->AddChild(Text("Module enabled", 11)).SetFillSize();
        AddProperties(m_Inspector, module, modulePath);
        Row(m_Inspector, Action("Apply", [this] { m_RebuildSelection = true; })); return;
    }
    m_SelectedModule = -1;

    const auto commands = Buttons(m_Inspector);
    const auto name = UniqueEmitterName(emitter.String("name") + "Copy");
    commands->AddChild(Action("Duplicate", [this, emitter, name]
    {
        auto copy = emitter; copy.Object["name"] = Value::FromString(name);
        QueueEdit([copy](Document& document) { document.Append({ "emitters" }, copy); });
    })).SetFillSize();
    const auto index = static_cast<size_t>(m_SelectedEmitter);
    commands->AddChild(Action("Delete emitter", [this, index]
    {
        QueueEdit([this, index](Document& document) { document.Remove({ "emitters" }, index); m_SelectedEmitter = -1; });
    })).SetFillSize();
    const auto modeButtons = Buttons(m_Inspector);
    for (const auto* mode : { "Sprite", "Mesh", "Ribbon", "Strand" })
        modeButtons->AddChild(Action(mode, [this, emitterPath, mode]
        {
            QueueEdit([emitterPath, mode](Document& document) { document.Set(Child(emitterPath, "renderMode"), Value::FromString(mode)); });
        })).SetFillSize();
    Section(m_Inspector, "Particle State", EditorStyle::Get().AxisZ);
    Row(m_Inspector, ParticleStateControl(emitter, emitterPath), 24);
    const auto help = Text("Off: particles stay alive after lifetime.", 10);
    help->SetColor(EditorStyle::Get().TextSecondary);
    Row(m_Inspector, help, 22);
    if (const auto state = emitter.Find("particleState"))
    {
        auto otherStateFields = *state;
        otherStateFields.Object.erase("killOnLifetimeExpired");
        AddProperties(m_Inspector, otherStateFields, Child(emitterPath, "particleState"));
    }
    auto properties = emitter;
    properties.Object.erase("particleState");
    properties.Object.erase("leafRenderer");
    properties.Object.erase("balloonRenderer");
    if (emitter.String("renderMode") == "Strand")
    {
        auto& strand = properties.Object["strand"];
        strand.Type = EValueType::Object;
        strand.Object.erase("endAttachment");
        for (const auto& [key, number] : std::array<std::pair<const char*, const char*>, 4>{
            {{"rootWidthScale", "1"}, {"tipWidthScale", "0"}, {"widthExponent", "1"}, {"branchWidthScale", "0.65"}}})
        {
            if (!strand.Find(key))
            {
                Value value; value.Type = EValueType::Number; value.Text = number;
                strand.Object[key] = std::move(value);
            }
        }
    }
    AddProperties(m_Inspector, properties, emitterPath);
    if (emitter.String("renderMode") == "Strand")
    {
        Section(m_Inspector, "Strand End Attachment", EditorStyle::Get().AxisZ);
        const auto strand = emitter.Find("strand");
        const auto end = strand ? strand->Find("endAttachment") : nullptr;
        const auto checkbox = CreateRef<GUI::Checkbox>();
        checkbox->SetStyle(EditorStyle::Get().Checkbox);
        checkbox->SetChecked(end && end->Enabled());
        checkbox->OnCheckedChanged([this, emitterPath](bool enabled)
        {
            QueueEdit([emitterPath, enabled](Document& document)
            {
                const auto existing = document.Get(emitterPath);
                if (!existing) return;
                auto emitter = *existing;
                auto& strand = emitter.Object["strand"]; strand.Type = EValueType::Object;
                auto& end = strand.Object["endAttachment"]; end.Type = EValueType::Object;
                end.Object["enabled"] = Boolean(enabled);
                if (!end.Find("position"))
                {
                    Value position; std::string error;
                    if (Value::Parse("[0,0,0]", position, error)) end.Object["position"] = std::move(position);
                }
                if (!end.Find("space")) end.Object["space"] = Value::FromString("Simulation");
                document.Set(emitterPath, std::move(emitter));
            });
        });
        const auto row = CreateRef<GUI::HorizontalBox>();
        row->AddChild(checkbox).SetFixedSize(16);
        row->AddChild(Text("Pin end", 11)).SetFillSize();
        Row(m_Inspector, row, 24);
        if (end) AddProperties(m_Inspector, *end, Child(Child(emitterPath, "strand"), "endAttachment"));
    }

    if (emitter.String("renderMode") == "Strand")
    {
        Section(m_Inspector, "Leaf Renderer (Editor)", {0.35f, 0.75f, 0.4f, 1});
        const auto leafPath = Child(emitterPath, "leafRenderer");
        if (const auto leaves = emitter.Find("leafRenderer"))
        {
            const auto checkbox = CreateRef<GUI::Checkbox>();
            checkbox->SetStyle(EditorStyle::Get().Checkbox);
            checkbox->SetChecked(leaves->Enabled());
            checkbox->OnCheckedChanged([this, leafPath](bool enabled)
            {
                QueueEdit([leafPath, enabled](Document& document)
                {
                    const auto existing = document.Get(leafPath);
                    if (!existing) return;
                    auto settings = *existing; settings.Object["enabled"] = Boolean(enabled);
                    document.Set(leafPath, std::move(settings));
                });
            });
            const auto enabledRow = CreateRef<GUI::HorizontalBox>();
            enabledRow->AddChild(checkbox).SetFixedSize(16);
            enabledRow->AddChild(Text("Enabled", 11)).SetFillSize();
            Row(m_Inspector, enabledRow, 24);
            AddProperties(m_Inspector, *leaves, leafPath);
            Row(m_Inspector, Action("Remove Leaf Renderer", [this, emitterPath]
            {
                QueueEdit([emitterPath](Document& document)
                {
                    const auto existing = document.Get(emitterPath);
                    if (!existing) return;
                    auto emitter = *existing; emitter.Object.erase("leafRenderer");
                    document.Set(emitterPath, std::move(emitter));
                });
            }));
        }
        else Row(m_Inspector, Action("Add Leaf Renderer", [this, leafPath]
        {
            QueueEdit([leafPath](Document& document)
            {
                Value settings; std::string error;
                if (Value::Parse(R"({"enabled":true,"interval":3,"length":0.12,"width":0.1,
                    "color":[0.12,0.45,0.08,1],"shape":"Ivy","maturationSegments":2})", settings, error))
                    document.Set(leafPath, std::move(settings));
            });
        }));
    }
    if (emitter.String("renderMode") == "Strand")
    {
        Section(m_Inspector, "Balloon Renderer (Editor)", {0.35f, 0.75f, 0.4f, 1});
        const auto balloonPath = Child(emitterPath, "balloonRenderer");
        if (const auto balloons = emitter.Find("balloonRenderer"))
        {
            const auto checkbox = CreateRef<GUI::Checkbox>();
            checkbox->SetStyle(EditorStyle::Get().Checkbox);
            checkbox->SetChecked(balloons->Enabled());
            checkbox->OnCheckedChanged([this, balloonPath](bool enabled)
            {
                QueueEdit([balloonPath, enabled](Document& document)
                {
                    const auto existing = document.Get(balloonPath);
                    if (!existing) return;
                    auto settings = *existing; settings.Object["enabled"] = Boolean(enabled);
                    document.Set(balloonPath, std::move(settings));
                });
            });
            const auto enabledRow = CreateRef<GUI::HorizontalBox>();
            enabledRow->AddChild(checkbox).SetFixedSize(16);
            enabledRow->AddChild(Text("Enabled", 11)).SetFillSize();
            Row(m_Inspector, enabledRow, 24);
            AddProperties(m_Inspector, *balloons, balloonPath);
            Row(m_Inspector, Action("Remove Balloon Renderer", [this, emitterPath]
            {
                QueueEdit([emitterPath](Document& document)
                {
                    const auto existing = document.Get(emitterPath);
                    if (!existing) return;
                    auto emitter = *existing; emitter.Object.erase("balloonRenderer");
                    document.Set(emitterPath, std::move(emitter));
                });
            }));
        }
        else Row(m_Inspector, Action("Add Balloon Renderer", [this, balloonPath]
        {
            QueueEdit([balloonPath](Document& document)
            {
                Value settings; std::string error;
                if (Value::Parse(R"({"enabled":true,"radius":0.11,"height":0.28,"tilt":0.4,
                    "color":[1,1,1,1],"colorVariation":1,"seed":42})", settings, error))
                    document.Set(balloonPath, std::move(settings));
            });
        }));
    }

}

void VFXPanel::UpdateStatus(const std::string& message)
{
    m_Status->SetText(message);

    m_StatusChanged(m_SelectedPath.empty() ? message : m_SelectedPath.filename().string() + " -- " + message);
}

void VFXPanel::SetPreviewImage(const Ref<Image>& image)
{
    std::static_pointer_cast<PreviewSurface>(m_PreviewInput)->SetSource(image);
}

Extent3D VFXPanel::GetPreviewExtent() const
{
    if (m_PreviewRect.Size.x <= 0 || m_PreviewRect.Size.y <= 0) return {0, 0, 1};
    const auto scale = m_Context.GetDPIScale();
    return {static_cast<uint32_t>(std::max(1.0f, std::ceil(m_PreviewRect.Size.x * scale))),
            static_cast<uint32_t>(std::max(1.0f, std::ceil(m_PreviewRect.Size.y * scale))), 1};
}

void VFXPanel::Render(const Timestep frameTime, const Ref<Image>& sceneTarget)
{
    std::lock_guard lock(m_PreviewMutex);
    float seconds = 0.0f;
    if (m_Active && m_Instance && !m_LoopRestart && !m_ReplayTime)
    {
        if (std::exchange(m_StepRequested, false)) seconds = 1.0f / 60.0f;
        else if (m_Playing) seconds = frameTime.GetSeconds() * m_PlaybackSpeed;
        seconds = std::min(seconds, std::max(0.0f, m_LoopDuration - m_Playhead));
        m_Playhead += seconds;
        if (m_Playhead >= m_LoopDuration)
        {
            if (m_Loop && m_Playing) m_LoopRestart = true; else m_Playing = false;
        }
    }
    m_Manager.BeginFrame(Timestep(seconds));
    if (m_Active && m_Instance && m_PreviewRect.Size.x > 0 && m_PreviewRect.Size.y > 0)
    {
        if (const auto replayTime = std::exchange(m_ReplayTime, std::nullopt))
            m_Manager.Replay(m_Camera.GetCamera(), sceneTarget, *replayTime);
        else m_Manager.Render(m_Camera.GetCamera(), sceneTarget);
    }
}
void VFXPanel::ProcessEvent(Event& event, const bool popupOpen)
{
    std::lock_guard lock(m_PreviewMutex);
    if (m_Active && !popupOpen && event.GetEventType() == EventType::KeyPressed &&
        (InputManager::IsKeyPressed(EE_KEY_LEFT_CONTROL) || InputManager::IsKeyPressed(EE_KEY_RIGHT_CONTROL) ||
         InputManager::IsKeyPressed(EE_KEY_LEFT_SUPER) || InputManager::IsKeyPressed(EE_KEY_RIGHT_SUPER)))
    {
        const auto key = static_cast<const KeyPressedEvent&>(event).GetKeyCode();
        if (key == EE_KEY_S) { m_SaveRequested = true; event.Handled = true; }
        if (key == EE_KEY_Z || key == EE_KEY_Y)
        {
            m_Drafts.clear();
            QueueEdit([key](Document& document) { if (key == EE_KEY_Z) document.Undo(); else document.Redo(); });
            event.Handled = true;
        }
    }
    const auto [mouseX, mouseY] = InputManager::GetMousePosition();
    const glm::vec2 mouse{mouseX, mouseY};
    const bool graphHit = m_Active && !popupOpen && m_Graph && m_Graph->GetGeometry().Contains(mouse) &&
        (AreOverlaysHidden() || (!m_PreviewSurface->GetGeometry().Contains(mouse) && !m_InspectorSurface->GetGeometry().Contains(mouse) &&
        !m_ModuleToolbar->GetGeometry().Contains(mouse) &&
        !m_TimelineSurface->GetGeometry().Contains(mouse) && !m_WorkbenchToolbar->GetGeometry().Contains(mouse)));
    if (graphHit && event.GetEventType() == EventType::MouseScrolled)
    {
        const float previous = m_GraphZoom;
        m_GraphZoom = std::clamp(previous * std::pow(1.15f, static_cast<const MouseScrolledEvent&>(event).GetOffsetY()), 0.5f, 1.75f);
        const glm::vec2 local = mouse - m_Graph->GetGeometry().Position;
        const glm::vec2 origin{2320, 2012};
        m_Graph->SetScrollOffset(origin + (m_Graph->GetScrollOffset() + local - origin) * (m_GraphZoom / previous) - local);
        m_RebuildSelection = true; event.Handled = true; return;
    }
    if (graphHit && event.GetEventType() == EventType::MouseButtonPressed)
    {
        const auto button = static_cast<const MouseButtonPressedEvent&>(event).GetMouseButton();
        std::vector<Ref<GUI::Widget>> path; m_Graph->HitTest(mouse, path);
        const bool controlHit = std::ranges::any_of(path, [this](const auto& widget)
        { return widget != m_Graph && widget->CanHandleMouseInput(); });
        if (button == EE_MOUSE_BUTTON_MIDDLE || (button == EE_MOUSE_BUTTON_LEFT && !controlHit))
        { m_GraphPanning = true; m_GraphLastPosition = mouse; event.Handled = true; return; }
    }
    if (m_GraphPanning && event.GetEventType() == EventType::MouseMoved)
    {
        m_Graph->SetScrollOffset(m_Graph->GetScrollOffset() + m_GraphLastPosition - mouse);
        m_GraphLastPosition = mouse; event.Handled = true; return;
    }
    if (event.GetEventType() == EventType::MouseButtonReleased) m_GraphPanning = false;
    if (event.GetEventType() == EventType::FramebufferResize ||
        event.GetEventType() == EventType::MouseButtonReleased)
    {
        m_Camera.ProcessEvent(event);
        if (event.GetEventType() == EventType::MouseButtonReleased)
        {
            const auto& released = static_cast<const MouseButtonReleasedEvent&>(event);
            if (released.GetMouseButton() == EE_MOUSE_BUTTON_LEFT) m_OrbitDragging = false;
            if (released.GetMouseButton() == EE_MOUSE_BUTTON_MIDDLE) m_PanDragging = false;
        }
        return;
    }
    if (!m_Active || popupOpen || AreOverlaysHidden() || !m_PreviewInput) return;

    const auto [x, y] = InputManager::GetMousePosition();
    const bool inside = m_PreviewInput->GetGeometry().Contains({ x, y });
    if (!inside && !(event.GetEventType() == EventType::MouseMoved && (m_OrbitDragging || m_PanDragging)))
        return;

    if (event.GetEventType() == EventType::MouseButtonPressed)
    {
        const auto& pressed = static_cast<const MouseButtonPressedEvent&>(event);
        if (pressed.GetMouseButton() == EE_MOUSE_BUTTON_LEFT) m_OrbitDragging = true;
        if (pressed.GetMouseButton() == EE_MOUSE_BUTTON_MIDDLE) m_PanDragging = true;
        m_Camera.ProcessEvent(event);
        // Seed the drag origin at the click so the first movement also rotates the camera.
        if (m_OrbitDragging || m_PanDragging)
        {
            MouseMovedEvent origin(x, y);
            m_Camera.ProcessEvent(origin);
            event.Handled = true;
        }
        return;
    }
    m_Camera.ProcessEvent(event);
    if (m_OrbitDragging || m_PanDragging || event.GetEventType() == EventType::MouseScrolled)
        event.Handled = true;
}
