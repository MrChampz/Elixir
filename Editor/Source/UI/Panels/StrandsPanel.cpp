#include "StrandsPanel.h"

#include <Engine/GUI/Button.h>
#include <Engine/Aether/Emitter.h>
#include <Engine/GUI/Renderer/RenderBatch.h>
#include <Engine/Input/InputCodes.h>
#include <Engine/Input/InputManager.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <format>

namespace
{
    // Displays the isolated strand scene render target.
    class StrandPreview final : public GUI::Widget
    {
    public:
        /** Updates the image used by the viewport. */
        void SetSource(const Ref<Image>& image)
        {
            if (m_Image == image) return;
            m_Image = image;
            MarkRenderDirty();
        }
    protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& available) override { return available; }
        void BuildDrawCommands(GUI::RenderBatch& batch, int zOrder) override
        {
            if (!m_Image) return;
            batch.AddImage(m_Image, GetGeometry(), {1, 1, 1, 1}, zOrder);
            batch.Clip(GetGeometry());
        }
    private:
        Ref<Image> m_Image;
    };
    constexpr std::array SampleFiles{"StrandPlayground.json", "StrandCables.json", "StrandSeaweed.json", "StrandEnergy.json"};
    constexpr std::array SampleLabels{"Tentacles", "Cables", "Seaweed", "Energy"};
    constexpr std::array MoveCodes{EE_KEY_W, EE_KEY_A, EE_KEY_S, EE_KEY_D};

    Ref<GUI::TextBlock> Label(const std::string& text)
    {
        auto label = CreateRef<GUI::TextBlock>(text);
        label->SetFontSize(11);
        label->SetColor(EditorStyle::Get().TextPrimary);
        return label;
    }
}

StrandsPanel::StrandsPanel(Aether::Manager& manager, GraphicsContext& context, const std::filesystem::path& effect)
    : m_Manager(manager), m_Context(context)
{
    m_EffectDirectory = effect.parent_path();
    LoadSample(0);
    ResetPosition();
}

bool StrandsPanel::LoadSample(int index)
{
    if (index < 0 || index >= int(SampleFiles.size())) return false;
    const auto path = m_EffectDirectory / SampleFiles[index];
    const auto system = Aether::Manager::LoadEffect(path);
    auto instance = system ? system->CreateInstance() : nullptr;
    if (instance) instance->SetWorldTransform(glm::translate(glm::mat4(1), m_Position));
    bool added = false;
    if (instance && m_Context.RunRenderTaskAndWait([&]
    {
        added = m_Manager.Add(instance);
        if (added && m_Instance) m_Manager.Remove(m_Instance);
    }) && added)
    {
        m_Instance = std::move(instance);
        m_SampleIndex = index;
        m_SampleName = system->GetName();
        m_StrandCount = 0;
        for (const auto& emitter : system->GetEmitters()) m_StrandCount += emitter->GetMaxParticles();
        m_LoadError.clear();
        StopNavigation();
        return true;
    }
    m_LoadError = "Could not load " + std::string(SampleLabels[index]) + ". See the log.";
    EE_CORE_ERROR("Could not activate the strand sample: {}", path.string());
    return false;
}

Ref<GUI::Widget> StrandsPanel::Build()
{
    const auto& style = EditorStyle::Get();
    auto root = CreateRef<GUI::Canvas>();
    root->SetStyle(style.Transparent);
    m_Preview = CreateRef<StrandPreview>();
    root->AddChild(m_Preview).SetAnchors(GUI::SAnchors::StretchAll());
    auto toolbar = CreateRef<GUI::VerticalBox>();
    auto controls = CreateRef<GUI::HorizontalBox>();
    toolbar->SetStyle(style.Toolbar);
    toolbar->SetPadding({10, 4});
    controls->AddChild(Label("WASD: move   |   Drag: orbit   |   Wheel: zoom")).SetFillSize()
        .SetVerticalAlignment(GUI::EVerticalAlignment::Center);
    auto reset = CreateRef<GUI::Button>();
    reset->SetStyle(style.PanelHeaderButton);
    reset->SetPadding({8, 3});
    reset->SetContent(Label("Reset position"));
    reset->OnClick([this] { m_ResetRequested = true; });
    controls->AddChild(reset).SetVerticalAlignment(GUI::EVerticalAlignment::Center);
    toolbar->AddChild(controls).SetFixedSize(28).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    auto samples = CreateRef<GUI::HorizontalBox>();
    for (int index = 0; index < int(SampleLabels.size()); ++index)
    {
        auto button = CreateRef<GUI::Button>();
        button->SetStyle(style.PanelHeaderButton);
        button->SetPadding({12, 3});
        button->SetContent(Label(SampleLabels[index]));
        button->OnClick([this, index] { m_RequestedSample = index; });
        samples->AddChild(button).SetMargin({0, 2, 6, 0});
    }
    toolbar->AddChild(samples).SetFixedSize(30).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    root->AddChild(toolbar).SetPosition({10, 10}).SetSize({530, 66});
    m_Toolbar = toolbar;
    RegisterOverlay(toolbar);
    m_Status = Label(m_Instance ? "" : "Could not load strand effect. See the log.");
    m_Status->SetVisibility(GUI::EVisibility::HitTestInvisible);
    root->AddChild(m_Status).SetPosition({20, 82}).SetSize({500, 24});
    return root;
}

void StrandsPanel::ResetPosition()
{
    m_Position = {};
    if (m_Instance) m_Instance->SetWorldTransform(glm::mat4(1));
    m_Camera.SetTarget({0, 0, 0});
    m_Camera.SetDistance(2.55f);
    m_Camera.SetOrbitYaw(90);
    m_Camera.SetOrbitPitch(glm::degrees(-0.446f));
    m_Camera.SetZoomSensitivity(0.15f);
    StopNavigation();
}

void StrandsPanel::OnUpdate(Timestep frameTime)
{
    if (!m_Active) return;
    if (m_RequestedSample >= 0)
    {
        const int requested = m_RequestedSample;
        m_RequestedSample = -1;
        LoadSample(requested);
    }
    const auto extent = GetPreviewExtent();
    if (extent.Height) m_Camera.GetPerspectiveCamera().SetAspectRatio(float(extent.Width) / extent.Height);
    if (m_ResetRequested) { m_ResetRequested = false; ResetPosition(); }
    if (!m_Instance) { m_Status->SetText(m_LoadError); return; }
    glm::vec2 direction{};
    if (!m_InputBlocked)
    {
        // Poll as well as tracking unhandled key presses, so losing window focus cannot leave a key held.
        for (size_t index = 0; index < MoveCodes.size(); ++index)
            m_MoveKeys[index] = m_MoveKeys[index] && InputManager::IsKeyPressed(MoveCodes[index]);
        direction = {float(m_MoveKeys[3]) - float(m_MoveKeys[1]), float(m_MoveKeys[0]) - float(m_MoveKeys[2])};
    }
    const float length = glm::length(direction);
    if (length > 1) direction /= length;
    const float step = 0.9f * std::clamp(frameTime.GetSeconds(), 0.0f, 0.05f);
    const auto previous = m_Position;
    m_Position.x = std::clamp(m_Position.x + direction.x * step, -1.25f, 1.25f);
    m_Position.y = std::clamp(m_Position.y + direction.y * step, -0.95f, 0.95f);
    if (previous != m_Position) m_Instance->SetWorldTransform(glm::translate(glm::mat4(1), m_Position));
    m_Status->SetText(m_LoadError.empty() ? std::format("{}   |   {} strands   |   Origin: ({:.2f}, {:.2f})",
        m_SampleName, m_StrandCount, m_Position.x, m_Position.y) : m_LoadError);
}

void StrandsPanel::OnActivated(bool active)
{
    m_Active = active;
    if (!active) StopNavigation();
}

void StrandsPanel::SetInputBlocked(bool blocked)
{
    m_InputBlocked = blocked;
    if (blocked) StopNavigation();
}

void StrandsPanel::SetPreviewImage(const Ref<Image>& image)
{
    std::static_pointer_cast<StrandPreview>(m_Preview)->SetSource(image);
}

Extent3D StrandsPanel::GetPreviewExtent() const
{
    if (!m_Preview) return {0, 0, 1};
    const auto size = m_Preview->GetGeometry().Size * m_Context.GetDPIScale();
    if (size.x <= 0 || size.y <= 0) return {0, 0, 1};
    return {uint32_t(std::ceil(size.x)), uint32_t(std::ceil(size.y)), 1};
}

void StrandsPanel::StopNavigation()
{
    m_MoveKeys.fill(false);
    MouseButtonReleasedEvent orbit(EE_MOUSE_BUTTON_LEFT), pan(EE_MOUSE_BUTTON_MIDDLE);
    m_Camera.ProcessEvent(orbit);
    m_Camera.ProcessEvent(pan);
    m_OrbitDragging = m_PanDragging = false;
}

void StrandsPanel::ProcessEvent(Event& event, bool inputBlocked)
{
    SetInputBlocked(inputBlocked);
    if (event.GetEventType() == EventType::KeyReleased)
    {
        const int code = static_cast<const KeyReleasedEvent&>(event).GetKeyCode();
        for (size_t index = 0; index < MoveCodes.size(); ++index)
            if (code == MoveCodes[index]) m_MoveKeys[index] = false;
    }
    if (event.GetEventType() == EventType::MouseButtonReleased)
    {
        m_Camera.ProcessEvent(event);
        const int button = static_cast<const MouseButtonReleasedEvent&>(event).GetMouseButton();
        if (button == EE_MOUSE_BUTTON_LEFT) m_OrbitDragging = false;
        if (button == EE_MOUSE_BUTTON_MIDDLE) m_PanDragging = false;
        return;
    }
    if (!m_Active || m_InputBlocked) { StopNavigation(); return; }
    if (event.GetEventType() == EventType::KeyPressed)
    {
        const auto& key = static_cast<const KeyPressedEvent&>(event);
        if (event.Handled || key.IsCtrlPressed() || key.IsAltPressed()) return;
        for (size_t index = 0; index < MoveCodes.size(); ++index)
            if (key.GetKeyCode() == MoveCodes[index]) { m_MoveKeys[index] = true; event.Handled = true; }
        return;
    }
    const auto [x, y] = InputManager::GetMousePosition();
    const bool dragging = m_OrbitDragging || m_PanDragging;
    if (!(dragging && event.GetEventType() == EventType::MouseMoved))
    {
        if (event.Handled || !m_Preview->GetGeometry().Contains({x, y})) return;
        if (m_Toolbar->IsRenderVisible() && m_Toolbar->GetGeometry().Contains({x, y})) return;
    }
    if (event.GetEventType() == EventType::MouseButtonPressed)
    {
        const int button = static_cast<const MouseButtonPressedEvent&>(event).GetMouseButton();
        if (button == EE_MOUSE_BUTTON_LEFT) m_OrbitDragging = true;
        else if (button == EE_MOUSE_BUTTON_MIDDLE) m_PanDragging = true;
        else return;
        m_Camera.ProcessEvent(event);
        MouseMovedEvent origin(x, y);
        m_Camera.ProcessEvent(origin);
        event.Handled = true;
    }
    else if ((dragging && event.GetEventType() == EventType::MouseMoved) || event.GetEventType() == EventType::MouseScrolled)
    {
        m_Camera.ProcessEvent(event);
        event.Handled = true;
    }
}
