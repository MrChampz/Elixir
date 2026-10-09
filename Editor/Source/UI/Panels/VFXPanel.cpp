#include "VFXPanel.h"
#include "../EditorStyles.h"
#include "../../Assets/VFXAssets.h"

#include <Engine/GUI/Button.h>
#include <Engine/GUI/ScrollBox.h>
#include <Engine/Input/InputCodes.h>
#include <Engine/Input/InputManager.h>

#include <utility>

namespace
{
    Ref<GUI::TextBlock> Text(const std::string& value, const float size = 12.0f)
    {
        const auto text = CreateRef<GUI::TextBlock>(value);
        text->SetFontSize(size);
        text->SetColor(EditorStyle::Get().TextPrimary);
        return text;
    }

    Ref<GUI::Button> CreateActionButton(const std::string& label, std::function<void()> clicked)
    {
        const auto button = CreateRef<GUI::Button>();
        button->SetStyle(EditorStyle::Get().PanelToggleButton);
        button->SetPadding({ 8.0f, 5.0f });
        button->SetContent(Text(label));
        button->OnClick(std::move(clicked));
        return button;
    }

    Ref<GUI::VerticalBox> SidePanel(const Ref<GUI::Canvas>& root, const bool right)
    {
        const auto panel = CreateRef<GUI::Overlay>();
        panel->SetStyle(EditorStyle::Get().Panel);
        panel->SetPadding({ 10.0f, 10.0f });
        root->AddChild(panel)
            .SetAnchors({ right ? 1.0f : 0.0f, 0.0f, right ? 1.0f : 0.0f, 1.0f })
            .SetOffsets(0.0f, 10.0f, 0.0f, -10.0f)
            .SetPosition({ right ? -10.0f : 10.0f, 0.0f })
            .SetAlignment({ right ? 1.0f : 0.0f, 0.0f })
            .SetSize({ 260.0f, 0.0f });

        const auto column = CreateRef<GUI::VerticalBox>();
        panel->AddChild(column)
            .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
            .SetVerticalAlignment(GUI::EVerticalAlignment::Fill);
        return column;
    }

    Ref<GUI::VerticalBox> ScrollList(const Ref<GUI::VerticalBox>& column)
    {
        const auto scroll = CreateRef<GUI::ScrollBox>();
        scroll->SetStyle(EditorStyle::Get().ScrollBar);
        column->AddChild(scroll).SetFillSize().SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
        const auto list = CreateRef<GUI::VerticalBox>();
        scroll->SetContent(list);
        return list;
    }

    const char* RenderModeName(const Aether::EParticleRenderMode mode)
    {
        switch (mode)
        {
            case Aether::EParticleRenderMode::Sprite: return "Sprite";
            case Aether::EParticleRenderMode::Mesh: return "Mesh";
            case Aether::EParticleRenderMode::Ribbon: return "Ribbon";
        }
        return "Unknown";
    }
}

VFXPanel::VFXPanel(Aether::Manager& manager, GraphicsContext& context, std::filesystem::path projectRoot,
    const float aspectRatio, std::function<void(const std::string&)> statusChanged)
    : m_Manager(manager), m_Context(context), m_ProjectRoot(std::move(projectRoot)),
      m_StatusChanged(std::move(statusChanged)), m_Camera(60.0f, aspectRatio)
{
    m_Camera.SetDistance(8.0f);
}

VFXPanel::~VFXPanel()
{
    if (m_Instance)
        m_Manager.Remove(m_Instance);
}

Ref<GUI::Widget> VFXPanel::Build()
{
    const auto root = CreateRef<GUI::Canvas>();
    root->SetStyle(EditorStyle::Get().Transparent);

    m_PreviewInput = CreateRef<GUI::Canvas>();
    root->AddChild(m_PreviewInput).SetAnchors(GUI::SAnchors::StretchAll())
        .SetOffsets(280.0f, 70.0f, -280.0f, -65.0f);

    const auto assets = SidePanel(root, false);
    assets->AddChild(Text("VFX Assets", 14.0f)).SetFixedSize(28.0f);
    assets->AddChild(Text("Assets / VFX")).SetFixedSize(24.0f);
    assets->AddChild(CreateActionButton("Refresh assets", [this] { m_RefreshRequested = true; }))
        .SetFixedSize(30.0f).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    m_AssetCount = Text("");
    assets->AddChild(m_AssetCount).SetFixedSize(26.0f);
    m_AssetList = ScrollList(assets);

    const auto inspector = SidePanel(root, true);
    inspector->AddChild(Text("VFX System", 14.0f)).SetFixedSize(28.0f);
    m_EffectName = Text("No effect selected");
    inspector->AddChild(m_EffectName).SetFixedSize(28.0f);
    inspector->AddChild(Text("Emitters")).SetFixedSize(26.0f);
    m_EmitterList = ScrollList(inspector);

    const auto toolbar = CreateRef<GUI::HorizontalBox>();
    toolbar->SetStyle(EditorStyle::Get().Toolbar);
    toolbar->SetPadding({ 5.0f, 5.0f });
    m_Playback = CreateActionButton("Pause", [this]
    {
        std::lock_guard lock(m_PreviewMutex);
        if (!m_Instance) return;
        m_Playing = !m_Playing;
        UpdateStatus(m_Playing ? "Playing" : "Paused");
    });
    toolbar->AddChild(m_Playback).SetMargin({ 2.0f, 0.0f });
    m_Restart = CreateActionButton("Restart", [this] { m_RestartRequested = true; });
    toolbar->AddChild(m_Restart).SetMargin({ 2.0f, 0.0f });
    m_Reload = CreateActionButton("Reload", [this] { m_ReloadRequested = true; });
    toolbar->AddChild(m_Reload).SetMargin({ 2.0f, 0.0f });
    toolbar->AddChild(CreateActionButton("Reset camera", [this]
    {
        std::lock_guard lock(m_PreviewMutex);
        m_Camera.SetTarget({ 0.0f, 0.0f, 0.0f });
        m_Camera.SetDistance(8.0f);
    })).SetMargin({ 2.0f, 0.0f });
    // Canvas snapshots a child's desired size, so dock after adding the controls.
    root->AddChild(toolbar).SetAnchors(GUI::SAnchors::TopCenter())
        .SetAlignment({ 0.5f, 0.0f }).SetPosition({ 0.0f, 10.0f });

    const auto footer = CreateRef<GUI::VerticalBox>();
    footer->SetStyle(EditorStyle::Get().StatsOverlay);
    footer->SetPadding({ 8.0f, 5.0f });
    m_Status = Text("Select an effect in Assets/VFX to preview it");
    footer->AddChild(m_Status);
    footer->AddChild(Text("Drag: orbit  |  Middle drag: pan  |  Scroll: zoom", 11.0f));
    root->AddChild(footer).SetAnchors({ 0.0f, 1.0f, 1.0f, 1.0f })
        .SetOffsets(280.0f, 0.0f, -280.0f, 0.0f)
        .SetAlignment({ 0.0f, 1.0f }).SetPosition({ 0.0f, -10.0f });

    UpdateStatus("Select an effect in Assets/VFX to preview it");
    RefreshAssets();
    return root;
}

void VFXPanel::OnUpdate(Timestep)
{
    if (std::exchange(m_RefreshRequested, false))
        RefreshAssets();
    if (!m_PendingPath.empty())
    {
        auto path = std::exchange(m_PendingPath, {});
        OpenAsset(path);
    }
    if (std::exchange(m_RestartRequested, false))
    {
        if (RestartPreview())
            UpdateStatus("Playing");
    }
    if (std::exchange(m_ReloadRequested, false) && !m_SelectedPath.empty())
        OpenAsset(m_SelectedPath, true);
}

void VFXPanel::OnActivated(const bool active)
{
    std::lock_guard lock(m_PreviewMutex);
    m_Active = active;
    if (!active)
    {
        // Release any camera gesture when its tab loses input ownership.
        MouseButtonReleasedEvent orbit(EE_MOUSE_BUTTON_LEFT);
        MouseButtonReleasedEvent pan(EE_MOUSE_BUTTON_MIDDLE);
        m_Camera.ProcessEvent(orbit);
        m_Camera.ProcessEvent(pan);
        m_OrbitDragging = false;
        m_PanDragging = false;
    }
}

void VFXPanel::RefreshAssets()
{
    const auto found = EditorAssets::FindVFXAssets(m_ProjectRoot);
    m_AssetList->ClearChildren();
    m_AssetRows.clear();
    m_AssetCount->SetText(std::to_string(found.Files.size()) + " effects");

    for (const auto& path : found.Files)
    {
        const auto row = CreateRef<GUI::Overlay>();
        row->SetPadding({ 8.0f, 5.0f });
        row->AddChild(Text(path.lexically_relative(m_ProjectRoot / "Assets" / "VFX").generic_string()));
        row->OnClick([this, path] { m_PendingPath = path; });
        m_AssetList->AddChild(row).SetFixedSize(30.0f)
            .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
        m_AssetRows.push_back({ path, row });
    }
    UpdateSelection();
    if (!found.Error.empty())
    {
        m_AssetList->AddChild(Text("Assets/VFX is unavailable"));
        EE_CORE_ERROR("{}", found.Error)
        UpdateStatus(found.Error);
    }
    else if (found.Files.empty())
    {
        m_AssetList->AddChild(Text("No JSON effects found"));
        UpdateStatus("No effects in Assets/VFX. Add an asset and refresh.");
    }
    else
        m_StatusChanged("Assets / VFX -- " + std::to_string(found.Files.size()) + " effects");
}

void VFXPanel::OpenAsset(const std::filesystem::path& path, const bool reload)
{
    const auto cached = m_LoadedSystems.find(path);
    const bool load = reload || cached == m_LoadedSystems.end();
    const auto system = load ? Aether::Manager::LoadEffect(path) : cached->second;
    if (!system)
    {
        UpdateStatus("Could not load " + path.filename().string() + ". See the log.");
        return;
    }

    if (load) for (const auto& emitter : system->GetEmitters())
    {
        if (const auto& authored = emitter->GetMaterialDescription(); authored && !authored->BaseColorTexturePath.empty())
        {
            auto description = *authored;
            const std::filesystem::path texture(description.BaseColorTexturePath);
            if (texture.is_relative())
                description.BaseColorTexturePath = (m_ProjectRoot / texture).lexically_normal().string();
            emitter->SetMaterialDescription(std::move(description));
        }
    }

    const auto instance = system->CreateInstance();
    bool activated = false;
    // Resolving materials may upload textures and must serialize with frame submission.
    const bool completed = instance && m_Context.RunRenderTaskAndWait([this, &activated, instance]
    {
        activated = m_Manager.Add(instance);
    });
    if (!completed || !activated)
    {
        EE_CORE_ERROR("Could not activate VFX preview: {}", path.string())
        UpdateStatus("Could not activate " + path.filename().string() + ". See the log.");
        return;
    }

    std::lock_guard lock(m_PreviewMutex);
    if (m_Instance)
        m_Manager.Remove(m_Instance);
    m_System = system;
    m_LoadedSystems.insert_or_assign(path, system);
    m_Instance = instance;
    m_SelectedPath = path;
    m_Playing = true;
    UpdateSelection();
    UpdateInspector();
    UpdateStatus("Playing");
}

bool VFXPanel::RestartPreview()
{
    if (!m_System) return false;
    const auto instance = m_System->CreateInstance();
    bool activated = false;
    const bool completed = instance && m_Context.RunRenderTaskAndWait([this, &activated, instance]
    {
        activated = m_Manager.Add(instance);
    });
    if (!completed || !activated)
    {
        EE_CORE_ERROR("Could not restart VFX preview: {}", m_SelectedPath.string())
        UpdateStatus("Could not restart the effect. See the log.");
        return false;
    }
    std::lock_guard lock(m_PreviewMutex);
    m_Manager.Remove(m_Instance);
    m_Instance = instance;
    m_Playing = true;
    return true;
}

void VFXPanel::UpdateInspector()
{
    m_EffectName->SetText(m_System->GetName());
    m_EmitterList->ClearChildren();
    uint64_t capacity = 0;
    for (const auto& emitter : m_System->GetEmitters())
    {
        capacity += emitter->GetMaxParticles();
        const auto block = CreateRef<GUI::VerticalBox>();
        block->SetStyle(EditorStyle::Get().PanelSection);
        block->SetPadding({ 8.0f, 8.0f });
        block->AddChild(Text(emitter->GetName()));
        block->AddChild(Text(RenderModeName(emitter->GetRenderMode()), 11.0f));
        block->AddChild(Text("Capacity: " + std::to_string(emitter->GetMaxParticles()), 11.0f));
        m_EmitterList->AddChild(block).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill)
            .SetMargin({ 0.0f, 0.0f, 0.0f, 6.0f });
    }
    m_EmitterList->AddChild(Text("Total capacity: " + std::to_string(capacity), 11.0f));
}

void VFXPanel::UpdateSelection()
{
    for (const auto& row : m_AssetRows)
        row.Widget->SetStyle(row.Path == m_SelectedPath
            ? EditorStyle::Get().Selection : EditorStyle::Get().Transparent);
}

void VFXPanel::UpdateStatus(const std::string& message)
{
    m_Status->SetText(message);
    m_Playback->SetEnabled(m_Instance != nullptr);
    m_Restart->SetEnabled(m_Instance != nullptr);
    m_Reload->SetEnabled(m_Instance != nullptr);
    const auto label = std::dynamic_pointer_cast<GUI::TextBlock>(m_Playback->GetContentSlot()->GetWidget());
    label->SetText(m_Playing ? "Pause" : "Play");
    const auto selection = m_SelectedPath.empty() ? "" : " / " +
        m_SelectedPath.lexically_relative(m_ProjectRoot / "Assets" / "VFX").generic_string();
    m_StatusChanged("Assets / VFX" + selection + " -- " + message);
}

void VFXPanel::Render(const Timestep frameTime, const Ref<Image>& sceneTarget)
{
    std::lock_guard lock(m_PreviewMutex);
    m_Manager.BeginFrame(m_Active && m_Playing ? frameTime : Timestep(0.0f));
    if (m_Active && m_Instance)
        m_Manager.Render(m_Camera.GetCamera(), sceneTarget);
}

void VFXPanel::ProcessEvent(Event& event, const bool popupOpen)
{
    std::lock_guard lock(m_PreviewMutex);
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
    if (!m_Active || popupOpen || event.Handled || !m_PreviewInput) return;

    const auto [x, y] = InputManager::GetMousePosition();
    const bool inside = m_PreviewInput->GetGeometry().Contains({ x, y });
    if (!inside && !(event.GetEventType() == EventType::MouseMoved && (m_OrbitDragging || m_PanDragging)))
        return;

    if (event.GetEventType() == EventType::MouseButtonPressed)
    {
        const auto& pressed = static_cast<const MouseButtonPressedEvent&>(event);
        if (pressed.GetMouseButton() == EE_MOUSE_BUTTON_LEFT) m_OrbitDragging = true;
        if (pressed.GetMouseButton() == EE_MOUSE_BUTTON_MIDDLE) m_PanDragging = true;
    }
    m_Camera.ProcessEvent(event);
}
