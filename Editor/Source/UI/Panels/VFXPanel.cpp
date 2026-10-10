#include "VFXPanel.h"
#include "../EditorStyles.h"

#include <Engine/Aether/Effect/Effect.h>
#include <Engine/GUI/Button.h>
#include <Engine/GUI/Checkbox.h>
#include <Engine/GUI/ScrollBox.h>
#include <Engine/GUI/TextField.h>
#include <Engine/GUI/Renderer/RenderBatch.h>
#include <Engine/Input/InputCodes.h>
#include <Engine/Input/InputManager.h>
#include <algorithm>
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
        auto style = EditorStyle::Get().PanelToggleButton;
        style.Normal.Background.CornerRadius = glm::vec4(3.0f);
        button->SetStyle(style);
        button->SetPadding({ 6.0f * scale, 4.0f * scale }); button->SetContent(Text(label, 11.0f * scale));
        button->OnClick(std::move(clicked)); return button;
    }

    void Row(const Ref<GUI::VerticalBox>& column, const Ref<GUI::Widget>& widget, const float height = 28.0f)
    {
        column->AddChild(widget).SetFixedSize(height).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
            .SetMargin({ 0.0f, 2.0f });
    }

    Ref<GUI::VerticalBox> MakePanel(const Ref<GUI::Canvas>& root, const GUI::SAnchors anchors,
        const float left, const float top, const float right, const float bottom)
    {
        const auto surface = CreateRef<GUI::Overlay>();
        auto style = EditorStyle::Get().Panel;
        style.Normal.Background.Color = { 0.075f, 0.079f, 0.09f, 1.0f };
        style.Normal.Background.CornerRadius = glm::vec4(3.0f);
        surface->SetStyle(style); surface->SetPadding({ 8.0f, 8.0f });
        auto& slot = root->AddChild(surface);
        slot.SetAnchors(anchors).SetOffsets(left, top, right, bottom);
        slot.SetPosition({ anchors.IsStretchingHorizontally() ? 0.0f : left,
            anchors.IsStretchingVertically() ? 0.0f : top });
        slot.SetSize({ anchors.IsStretchingHorizontally() ? 0.0f : right - left,
            anchors.IsStretchingVertically() ? 0.0f : bottom - top });
        const auto column = CreateRef<GUI::VerticalBox>();
        surface->AddChild(column).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
            .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
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
                batch.AddRect({ rect.Position + glm::vec2(x, 0), { 1, rect.Size.y } }, { 0.13f, 0.14f, 0.16f, 1 }, {}, {}, {}, {}, zOrder);
            for (float y = startY; y < rect.Size.y; y += spacing)
                batch.AddRect({ rect.Position + glm::vec2(0, y), { rect.Size.x, 1 } }, { 0.13f, 0.14f, 0.16f, 1 }, {}, {}, {}, {}, zOrder);
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
        glm::vec2 ComputeDesiredSize(const glm::vec2& size) override { return glm::min(size, glm::vec2(284, 160)); }
        void BuildDrawCommands(GUI::RenderBatch& batch, const int zOrder) override
        {
            if (m_Image)
                batch.AddImage(m_Image, GetGeometry(), {1, 1, 1, 1}, zOrder, GetGeometry());
        }
    private:
        Ref<Image> m_Image;
    };

    // Draws a time ruler and playhead using the same retained rendering path as other widgets.
    class TimeRuler final : public GUI::Widget
    {
    public:
        /** @brief Updates the playback marker in seconds. */
        void SetTime(const float time, const float duration)
        {
            if (time == m_Time && duration == m_Duration) return;
            m_Time = time; m_Duration = duration; MarkRenderDirty();
        }
    protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override
        { return { std::min(availableSize.x, 400.0f), 18.0f }; }
        void BuildDrawCommands(GUI::RenderBatch& batch, const int zOrder) override
        {
            const auto rect = GetGeometry();
            batch.AddRect(rect, { 0.10f, 0.11f, 0.13f, 1 }, {}, {}, {}, {}, zOrder);
            for (int tick = 0; tick <= 20; ++tick)
            {
                const auto x = rect.Position.x + rect.Size.x * tick / 20.0f;
                batch.AddRect({ { x, rect.Position.y }, { 1, tick % 4 == 0 ? rect.Size.y : 5.0f } },
                    { 0.24f, 0.26f, 0.30f, 1 }, {}, {}, {}, {}, zOrder + 1);
            }
            const auto x = rect.Position.x + rect.Size.x * std::clamp(m_Time / m_Duration, 0.0f, 1.0f);
            batch.AddRect({ { x, rect.Position.y }, { 2, rect.Size.y } }, EditorStyle::Get().Accent,
                {}, {}, {}, {}, zOrder + 2);
        }
    private:
        float m_Time = 0.0f, m_Duration = 5.0f;
    };
}

VFXPanel::VFXPanel(Aether::Manager& manager, GraphicsContext& context, std::filesystem::path projectRoot,
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
    const auto toolbar = CreateRef<GUI::HorizontalBox>();
    root->AddChild(toolbar).SetAnchors({1, 0, 1, 0}).SetAlignment({1, 0})
        .SetPosition({-12, 12}).SetSize({125, 30});
    m_WorkbenchToolbar = toolbar;
    const auto overviewHeader = toolbar;
    overviewHeader->AddChild(Action("-", [this] { m_GraphZoom = std::max(0.5f, m_GraphZoom - 0.1f); m_RebuildSelection = true; })).SetFixedSize(24);
    overviewHeader->AddChild(Action("+", [this] { m_GraphZoom = std::min(1.75f, m_GraphZoom + 0.1f); m_RebuildSelection = true; })).SetFixedSize(24);
    overviewHeader->AddChild(Action("+ Emitter", [this]
    {
        if (!m_Document) return;
        const auto name = UniqueEmitterName("Emitter");
        QueueEdit([name](Document& document) { document.Append({"emitters"}, MakeEmitter(name)); });
    })).SetFixedSize(75);
    const auto graph = CreateRef<GraphScrollBox>(); graph->SetStyle(EditorStyle::Get().ScrollBar);
    graph->SetScrollAxis(GUI::EScrollAxis::Both); graph->SetSize({ 4000, 4000 });
    graph->SetShowScrollbar(false); m_Graph = graph;
    workbench->AddChild(graph).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
        .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
    m_Overview = CreateRef<GUI::HorizontalBox>();
    m_Overview->SetPadding({ 2320, 2012, 2320, 2080 });
    graph->SetContent(m_Overview);

    const auto preview = MakePanel(root, { 0, 0, 0, 0 }, 12, 44, 312, 278);
    m_PreviewSurface = preview;
    Row(preview, Text("PREVIEW", 11), 22);
    m_PreviewInput = CreateRef<PreviewSurface>();
    preview->AddChild(m_PreviewInput).SetFillSize().SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    const auto previewControls = Buttons(preview);
    previewControls->AddChild(Action("Reset camera", [this]
    {
        std::lock_guard lock(m_PreviewMutex); m_Camera.SetTarget({ 0, 0, 0 }); m_Camera.SetDistance(5);
    })).SetFillSize();

    const auto selection = MakePanel(root, { 1, 0, 1, 1 }, -312, 44, -12, -12);
    m_InspectorSurface = selection;
    Row(selection, Text("SELECTION / DETAILS", 11), 26);
    m_Inspector = ScrollList(selection);

    const auto timeline = MakePanel(root, { 0, 1, 1, 1 }, 12, -150, -324, -12);
    m_TimelineSurface = timeline;
    const auto playback = Buttons(timeline);
    playback->AddChild(Text("TIMELINE", 11)).SetFixedSize(90);
    playback->AddChild(Action("Restart", [this] { m_RestartRequested = true; })).SetFixedSize(65);
    m_Playback = Action("Pause", [this]
    {
        std::lock_guard lock(m_PreviewMutex); m_Playing = !m_Playing;
    }); playback->AddChild(m_Playback).SetFixedSize(60);
    playback->AddChild(Action("Step", [this]
    {
        std::lock_guard lock(m_PreviewMutex); m_Playing = false; m_StepRequested = true;
    })).SetFixedSize(55);
    const auto loop = CreateRef<GUI::Checkbox>(); loop->SetStyle(EditorStyle::Get().Checkbox); loop->SetChecked(true);
    loop->OnCheckedChanged([this](const bool checked) { std::lock_guard lock(m_PreviewMutex); m_Loop = checked; });
    playback->AddChild(loop).SetFixedSize(24); playback->AddChild(Text("Loop 5s", 11)).SetFixedSize(65);
    playback->AddChild(Action("Speed", [this]
    {
        std::lock_guard lock(m_PreviewMutex);
        m_PlaybackSpeed = m_PlaybackSpeed == 0.25f ? 0.5f : m_PlaybackSpeed == 0.5f ? 1.0f : m_PlaybackSpeed == 1.0f ? 2.0f : 0.25f;
    })).SetFixedSize(60);
    m_Clock = Text("0.00s / 5.00s", 11); playback->AddChild(m_Clock).SetFillSize();
    m_TimeBar = CreateRef<TimeRuler>(); Row(timeline, m_TimeBar, 18);
    const auto labels = CreateRef<GUI::Canvas>();
    for (int second = 0; second <= 5; ++second)
    {
        const float anchor = second / 5.0f;
        labels->AddChild(Text(std::to_string(second) + "s", 10))
            .SetAnchors({ anchor, 0, anchor, 0 }).SetPosition({ second == 5 ? -15.0f : 0.0f, 0 })
            .SetSize({ 20, 18 });
    }
    Row(timeline, labels, 18);
    m_Tracks = ScrollList(timeline);
    m_Status = Text("Select an effect to begin authoring", 10); Row(timeline, m_Status, 22);
    UpdateOverview(); UpdateInspector(); UpdateStatus("Opening effect...");
    return root;
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
    {
        std::lock_guard lock(m_PreviewMutex);
        loopRestart = std::exchange(m_LoopRestart, false);
        m_Clock->SetText(std::format("{:.2f}s / {:.2f}s   {:.2f}x", m_Playhead, m_LoopDuration, m_PlaybackSpeed));
        std::static_pointer_cast<TimeRuler>(m_TimeBar)->SetTime(m_Playhead, m_LoopDuration);
        const auto label = std::dynamic_pointer_cast<GUI::TextBlock>(m_Playback->GetContentSlot()->GetWidget());
        label->SetText(m_Playing ? "Pause" : "Play");
    }
    if (std::exchange(m_RestartRequested, false) || loopRestart) RestartPreview();
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
    const bool completed = m_Context.RunRenderTaskAndWait([this, &compiled, system, replacement, current]
    {
        if (current) current->Swap(*replacement);
        compiled = m_Manager.Recompile(system);
        if (!compiled && current) current->Swap(*replacement);
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
    m_Instance = instance; m_Playhead = 0; m_Playing = true; m_StepRequested = false; m_LoopRestart = false;
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
    m_Overview->ClearChildren(); m_Tracks->ClearChildren();
    if (!m_Document) { m_Overview->AddChild(Text("Open an effect or create a new system.")); return; }
    const auto graphRow = [this](const Ref<GUI::VerticalBox>& column, const Ref<GUI::Widget>& widget, float height = 28)
    { Row(column, widget, height * m_GraphZoom); };
    const auto graphText = [this](const std::string& text, float size = 12)
    { return Text(text, size * m_GraphZoom); };
    const auto graphAction = [this](const std::string& label, std::function<void()> callback)
    { return Action(label, std::move(callback), m_GraphZoom); };
    const auto graphSection = [&](const Ref<GUI::VerticalBox>& column, const std::string& title, const SColor color)
    { const auto text = graphText(title, 11); text->SetColor(color); graphRow(column, text, 24); };
    const auto system = CreateRef<GUI::VerticalBox>(); system->SetStyle(EditorStyle::Get().PanelSection); system->SetPadding({ 8, 8 });
    graphRow(system, graphAction(m_Document->Root().String("name"), [this] { m_SelectedEmitter = -1; m_SelectedModule = -1; m_RebuildSelection = true; }), 34);
    graphRow(system, graphText("SYSTEM", 10), 22);
    graphRow(system, graphAction("User parameters", [this] { m_SelectedEmitter = -1; m_SelectedModule = -1; m_RebuildSelection = true; }), 28);
    graphRow(system, graphText("GPU simulation", 10), 22);
    m_Overview->AddChild(system).SetFixedSize(180 * m_GraphZoom).SetVerticalAlignment(GUI::EVerticalAlignment::Top).SetMargin({ 4, 4 });
    const auto& emitters = m_Document->Root().Find("emitters")->Array;
    if (m_SelectedEmitter >= static_cast<int>(emitters.size())) { m_SelectedEmitter = -1; m_SelectedModule = -1; }
    for (size_t index = 0; index < emitters.size(); ++index)
    {
        const auto& emitter = emitters[index];
        const auto path = Path{ "emitters", std::to_string(index) };
        const auto card = CreateRef<GUI::VerticalBox>(); card->SetPadding({ 8 * m_GraphZoom, 8 * m_GraphZoom });
        card->SetStyle(m_SelectedEmitter == static_cast<int>(index) ? EditorStyle::Get().Selection : EditorStyle::Get().PanelSection);
        const auto header = CreateRef<GUI::HorizontalBox>(); graphRow(card, header, 30);
        const auto toggle = CreateRef<GUI::Checkbox>(); toggle->SetStyle(EditorStyle::Get().Checkbox); toggle->SetChecked(emitter.Enabled());
        toggle->OnCheckedChanged([this, path](const bool enabled) { QueueEdit([path, enabled](Document& document) { document.Set(Child(path, "enabled"), Boolean(enabled)); }); });
        header->AddChild(toggle).SetFixedSize(20);
        header->AddChild(graphAction(emitter.String("name"), [this, index] { m_SelectedEmitter = static_cast<int>(index); m_SelectedModule = -1; m_RebuildSelection = true; })).SetFillSize();
        graphRow(card, graphText(emitter.String("renderMode", "Sprite") + " renderer", 10), 20);
        for (const auto& phase : { std::string("spawnModules"), std::string("updateModules") })
        {
            graphSection(card, phase == "spawnModules" ? "PARTICLE SPAWN" : "PARTICLE UPDATE",
                phase == "spawnModules" ? EditorStyle::Get().AxisY : EditorStyle::Get().AxisZ);
            if (const auto modules = emitter.Find(phase))
                for (size_t moduleIndex = 0; moduleIndex < modules->Array.size(); ++moduleIndex)
                {
                    const auto& module = modules->Array[moduleIndex];
                    const auto button = graphAction((module.Enabled() ? "" : "[off] ") + module.String("type"), [this, index, moduleIndex, phase]
                    {
                        m_SelectedEmitter = static_cast<int>(index); m_SelectedModule = static_cast<int>(moduleIndex);
                        m_SelectedPhase = phase; m_RebuildSelection = true;
                    });
                    graphRow(card, button, 28);
                }
            graphRow(card, graphAction("+ Add module", [this, index, phase]
            {
                m_SelectedEmitter = static_cast<int>(index); m_PalettePhase = phase; m_RebuildSelection = true;
            }), 28);
        }
        graphSection(card, "RENDER", EditorStyle::Get().AxisX);
        graphRow(card, graphAction("Renderer properties", [this, index]
        {
            m_SelectedEmitter = static_cast<int>(index); m_SelectedModule = -1; m_RebuildSelection = true;
        }), 28);
        m_Overview->AddChild(card).SetFixedSize(210 * m_GraphZoom).SetVerticalAlignment(GUI::EVerticalAlignment::Top).SetMargin({ 4, 4 });
        const auto track = CreateRef<GUI::HorizontalBox>(); track->SetStyle(emitter.Enabled() ? EditorStyle::Get().Selection : EditorStyle::Get().PanelSection);
        track->AddChild(Action(emitter.String("name") + (emitter.Enabled() ? "   |   Active" : "   |   Disabled"), [this, index]
        { m_SelectedEmitter = static_cast<int>(index); m_SelectedModule = -1; m_RebuildSelection = true; })).SetFillSize();
        Row(m_Tracks, track, 20);
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
    Row(column, Text(label, 10), 16);
    const auto input = CreateRef<GUI::TextField>(value.Type == EValueType::String ? value.Text : value.JSON());
    input->SetStyle(EditorStyle::Get().TextField); input->SetFontSize(11); input->SetPadding({ 6, 4 });
    input->OnChange([this, path](const std::string& text)
    {
        const auto existing = std::find_if(m_Drafts.begin(), m_Drafts.end(), [&path](const Edit& edit) { return edit.Location == path; });
        if (existing == m_Drafts.end()) m_Drafts.push_back({ path, text }); else existing->Text = text;
        m_EditDelay = 0.6f;
    });
    Row(column, input, 28);
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
        AddProperties(m_Inspector, module, modulePath); return;
    }
    m_SelectedModule = -1;
    Row(m_Inspector, Text(emitter.String("name"), 13));
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
    for (const auto* mode : { "Sprite", "Mesh", "Ribbon" })
        modeButtons->AddChild(Action(mode, [this, emitterPath, mode]
        {
            QueueEdit([emitterPath, mode](Document& document) { document.Set(Child(emitterPath, "renderMode"), Value::FromString(mode)); });
        })).SetFillSize();
    AddProperties(m_Inspector, emitter, emitterPath);
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
    if (m_Active && m_Instance && !m_LoopRestart)
    {
        if (std::exchange(m_StepRequested, false)) seconds = 1.0f / 60.0f;
        else if (m_Playing) seconds = frameTime.GetSeconds() * m_PlaybackSpeed;
        seconds = std::min(seconds, std::max(0.0f, m_LoopDuration - m_Playhead));
        m_Playhead += seconds;
        if (m_Playhead >= m_LoopDuration)
        {
            if (m_Loop) m_LoopRestart = true; else m_Playing = false;
        }
    }
    m_Manager.BeginFrame(Timestep(seconds));
    if (m_Active && m_Instance && m_PreviewRect.Size.x > 0 && m_PreviewRect.Size.y > 0)
    {
        m_Manager.Render(m_Camera.GetCamera(), sceneTarget);
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
        !m_PreviewSurface->GetGeometry().Contains(mouse) && !m_InspectorSurface->GetGeometry().Contains(mouse) &&
        !m_TimelineSurface->GetGeometry().Contains(mouse) && !m_WorkbenchToolbar->GetGeometry().Contains(mouse);
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
    if (!m_Active || popupOpen || !m_PreviewInput) return;

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
