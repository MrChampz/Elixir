#include "MeshPanel.h"

#include <Engine/GUI/Button.h>
#include <Engine/GUI/ScrollBox.h>
#include <Engine/GUI/Renderer/RenderBatch.h>
#include <Engine/Input/InputCodes.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Materials/Material.h>
#include <Engine/Mesh/GeometryPool.h>
#include <algorithm>
#include <cmath>
#include <format>

namespace
{
    // Draws only the image belonging to this document's viewport.
    class MeshPreview final : public GUI::Widget
    {
    public:
        /** Changes the image displayed by the viewport. */
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

    Ref<GUI::TextBlock> Label(const std::string& text, float size = 11)
    {
        auto label = CreateRef<GUI::TextBlock>(text);
        label->SetFontSize(size);
        label->SetColor(EditorStyle::Get().TextPrimary);
        return label;
    }
}

MeshPanel::MeshPanel(GraphicsContext& context, Ref<StaticMesh> mesh, std::filesystem::path path)
    : m_Context(context), m_Mesh(std::move(mesh)), m_Path(std::move(path)), m_Camera(60, 1)
{
    FrameMesh();
}

Ref<GUI::Widget> MeshPanel::Build()
{
    const auto& style = EditorStyle::Get();
    auto root = CreateRef<GUI::Canvas>();
    root->SetStyle(style.Transparent);
    m_Preview = CreateRef<MeshPreview>();
    root->AddChild(m_Preview).SetAnchors(GUI::SAnchors::StretchAll());

    auto toolbar = CreateRef<GUI::HorizontalBox>();
    toolbar->SetStyle(style.Toolbar);
    toolbar->SetPadding({8, 4});
    toolbar->AddChild(Label(m_Path.filename().string(), 12)).SetFillSize()
        .SetVerticalAlignment(GUI::EVerticalAlignment::Center);
    auto frame = CreateRef<GUI::Button>();
    frame->SetStyle(style.PanelHeaderButton);
    frame->SetPadding({8, 3});
    frame->SetContent(Label("Frame mesh"));
    frame->OnClick([this] { m_FrameRequested = true; });
    toolbar->AddChild(frame).SetVerticalAlignment(GUI::EVerticalAlignment::Center);
    root->AddChild(toolbar).SetPosition({10, 10}).SetSize({340, 34});
    m_Toolbar = toolbar;
    RegisterOverlay(toolbar);

    auto inspector = CreateRef<GUI::VerticalBox>();
    inspector->SetStyle(style.Panel);
    inspector->SetPadding({1, 1});
    auto header = CreateRef<GUI::HorizontalBox>();
    header->SetStyle(style.Transparent);
    header->SetPadding({10, 0});
    header->AddChild(Label("Mesh", 12)).SetFillSize();
    inspector->AddChild(header).SetFixedSize(27).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    auto separator = CreateRef<GUI::Canvas>();
    separator->SetStyle(style.PanelHeaderBorder);
    inspector->AddChild(separator).SetFixedSize(1).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    auto scroll = CreateRef<GUI::ScrollBox>();
    scroll->SetStyle(style.ScrollBar);
    inspector->AddChild(scroll).SetFillSize().SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    auto details = CreateRef<GUI::VerticalBox>();
    details->SetPadding({10, 8});
    scroll->SetContent(details);
    const auto row = [&](const std::string& title, const std::string& value)
    {
        auto column = CreateRef<GUI::VerticalBox>();
        auto caption = Label(title, 10);
        caption->SetColor(style.TextSecondary);
        column->AddChild(caption).SetFixedSize(20).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
        column->AddChild(Label(value)).SetFixedSize(24).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
        details->AddChild(column).SetFixedSize(48).SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    };
    row("Asset", m_Path.filename().string());
    row("Sections", std::to_string(m_Mesh->GetSections().size()));
    if (const auto geometry = m_Mesh->GetGeometry())
    {
        row("Vertices", std::to_string(geometry->VertexCount));
        row("Triangles", std::to_string(geometry->IndexCount / 3));
    }
    const auto size = m_Mesh->GetLocalBounds().Max - m_Mesh->GetLocalBounds().Min;
    row("Bounds", std::format("{:.2f} × {:.2f} × {:.2f}", size.x, size.y, size.z));
    row("Materials", std::to_string(m_Mesh->GetMaterials().size()));
    for (const auto& material : m_Mesh->GetMaterials())
        if (material) details->AddChild(Label(material->GetName())).SetFixedSize(24)
            .SetHorizontalAlignment(GUI::EHorizontalAlignment::Fill);
    root->AddChild(inspector).SetAnchors({1, 0, 1, 1}).SetPosition({-270, 0})
        .SetSize({260, 0}).SetOffsets(0, 10, 0, -10);
    m_Inspector = inspector;
    RegisterOverlay(inspector, {12, 0});
    return root;
}

void MeshPanel::FrameMesh()
{
    const auto& bounds = m_Mesh->GetLocalBounds();
    const float radius = std::max(glm::length(bounds.Max - bounds.Min) * 0.5f, 0.1f);
    const auto extent = GetPreviewExtent();
    const float aspect = extent.Height ? float(extent.Width) / extent.Height : 1.0f;
    const float verticalAngle = glm::radians(30.0f);
    const float horizontalAngle = std::atan(std::tan(verticalAngle) * aspect);
    const float distance = radius / std::sin(std::min(verticalAngle, horizontalAngle)) * 1.15f;
    m_Camera.GetPerspectiveCamera().SetProjection(60, aspect, std::max(0.001f, radius * 0.001f),
        std::max(100.0f, distance + radius * 4));
    m_Camera.SetTarget((bounds.Min + bounds.Max) * 0.5f);
    m_Camera.SetDistance(distance);
    m_Camera.SetOrbitYaw(-65);
    m_Camera.SetOrbitPitch(15);
    m_Camera.SetZoomSensitivity(std::max(0.05f, radius * 0.1f));
}

void MeshPanel::OnUpdate(Timestep)
{
    const auto extent = GetPreviewExtent();
    if (extent.Height) m_Camera.GetPerspectiveCamera().SetAspectRatio(float(extent.Width) / extent.Height);
    if (m_FrameRequested) { m_FrameRequested = false; FrameMesh(); }
}

void MeshPanel::OnActivated(bool active)
{
    m_Active = active;
    if (!active) StopNavigation();
}

void MeshPanel::SetPreviewImage(const Ref<Image>& image)
{
    std::static_pointer_cast<MeshPreview>(m_Preview)->SetSource(image);
}

Extent3D MeshPanel::GetPreviewExtent() const
{
    if (!m_Preview) return {0, 0, 1};
    const auto size = m_Preview->GetGeometry().Size * m_Context.GetDPIScale();
    if (size.x <= 0 || size.y <= 0) return {0, 0, 1};
    return {uint32_t(std::ceil(size.x)), uint32_t(std::ceil(size.y)), 1};
}

void MeshPanel::StopNavigation()
{
    MouseButtonReleasedEvent orbit(EE_MOUSE_BUTTON_LEFT), pan(EE_MOUSE_BUTTON_MIDDLE);
    m_Camera.ProcessEvent(orbit);
    m_Camera.ProcessEvent(pan);
    m_OrbitDragging = false;
    m_PanDragging = false;
}

void MeshPanel::ProcessEvent(Event& event, bool inputBlocked)
{
    if (event.GetEventType() == EventType::MouseButtonReleased)
    {
        m_Camera.ProcessEvent(event);
        const int button = static_cast<const MouseButtonReleasedEvent&>(event).GetMouseButton();
        if (button == EE_MOUSE_BUTTON_LEFT) m_OrbitDragging = false;
        if (button == EE_MOUSE_BUTTON_MIDDLE) m_PanDragging = false;
        return;
    }
    if (!m_Active || inputBlocked) { StopNavigation(); return; }
    const auto [x, y] = InputManager::GetMousePosition();
    const glm::vec2 point{x, y};
    const bool dragging = m_OrbitDragging || m_PanDragging;
    if (!(dragging && event.GetEventType() == EventType::MouseMoved))
    {
        if (event.Handled || !m_Preview->GetGeometry().Contains(point)) return;
        if ((m_Inspector->IsRenderVisible() && m_Inspector->GetGeometry().Contains(point)) ||
            (m_Toolbar->IsRenderVisible() && m_Toolbar->GetGeometry().Contains(point))) return;
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
    else if ((dragging && event.GetEventType() == EventType::MouseMoved) ||
        event.GetEventType() == EventType::MouseScrolled)
    {
        m_Camera.ProcessEvent(event);
        event.Handled = true;
    }
}
