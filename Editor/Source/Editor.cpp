#include "Editor.h"
#include "ContentBrowser.h"
#include "GameViewRenderer.h"
#include "RmlUiInput.h"
#include "RmlUiRenderer.h"
#include "RmlUiSystemInterface.h"

#include <Engine/Core/Entrypoint.h>
#include <Engine/Event/KeyEvent.h>
#include <Engine/Event/MouseEvent.h>
#include <Engine/Input/InputCodes.h>
#include <Engine/Aether/Effect/Effect.h>
#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace
{
    constexpr int MaterialConnectionSegmentCount = 18;

    Rml::String EscapeRml(const std::string& value)
    {
        Rml::String escaped;
        escaped.reserve(value.size());
        for (const char character : value)
        {
            switch (character)
            {
                case '&': escaped += "&amp;"; break;
                case '<': escaped += "&lt;"; break;
                case '>': escaped += "&gt;"; break;
                case '\"': escaped += "&quot;"; break;
                case '\'': escaped += "&#39;"; break;
                default: escaped += character; break;
            }
        }
        return escaped;
    }

    std::string FormatVector(const glm::vec3& value)
    {
        return std::format("{:.2f}, {:.2f}, {:.2f}", value.x, value.y, value.z);
    }

    const char* RenderModeName(const Elixir::Aether::Core::EParticleRenderMode mode)
    {
        using Elixir::Aether::Core::EParticleRenderMode;
        switch (mode)
        {
            case EParticleRenderMode::Sprite: return "Sprite";
            case EParticleRenderMode::Ribbon: return "Ribbon";
            case EParticleRenderMode::Mesh: return "Mesh";
        }
        return "Unknown";
    }

    struct SDockWindowDefinition
    {
        const char* PanelId;
        const char* HeaderId;
        const char* TabId;
        const char* CloseId;
        const char* Name;
    };

    struct SMaterialConnectionDefinition
    {
        const char* ElementId;
        const char* OutputPortId;
        const char* InputPortId;
        const char* OutputNodeId;
        const char* InputNodeId;
    };

    constexpr std::array DockWindows {
        SDockWindowDefinition {
            "content-browser",
            "content-browser-header",
            "dock-tab-content-browser",
            "dock-close-content-browser",
            "Content Browser"
        },
        SDockWindowDefinition {
            "material-editor",
            "material-editor-header",
            "dock-tab-material-editor",
            "dock-close-material-editor",
            "Material Editor"
        }
    };

    constexpr std::array MaterialNodeIds {
        "material-node-texture",
        "material-node-multiply",
        "material-node-constant",
        "material-node-output"
    };

    constexpr std::array MaterialNodeHeaderIds {
        "material-node-texture-header",
        "material-node-multiply-header",
        "material-node-constant-header",
        "material-node-output-header"
    };

    constexpr std::array MaterialConnections {
        SMaterialConnectionDefinition {
            "material-link-texture-multiply",
            "port-texture-color",
            "port-multiply-a",
            "material-node-texture",
            "material-node-multiply"
        },
        SMaterialConnectionDefinition {
            "material-link-constant-multiply",
            "port-constant-value",
            "port-multiply-b",
            "material-node-constant",
            "material-node-multiply"
        },
        SMaterialConnectionDefinition {
            "material-link-multiply-output",
            "port-multiply-result",
            "port-output-base",
            "material-node-multiply",
            "material-node-output"
        }
    };

    constexpr std::array SceneTransformToolIds {
        "scene-tool-select",
        "scene-tool-move",
        "scene-tool-rotate",
        "scene-tool-scale"
    };

    constexpr std::array HierarchyIds {
        "object-scene",
        "object-light",
        "object-camera",
        "object-player",
        "object-environment",
        "object-particles"
    };

    constexpr std::array InteractiveIds {
        "tool-play",
        "tool-pause",
        "scene-tool-select",
        "scene-tool-move",
        "scene-tool-rotate",
        "scene-tool-scale",
        "scene-tool-space",
        "scene-camera-mode",
        "scene-view-mode",
        "scene-show-menu",
        "scene-grid-toggle",
        "scene-snap-toggle",
        "scene-camera-speed",
        "scene-settings",
        "object-scene",
        "object-light",
        "object-camera",
        "object-player",
        "object-environment",
        "object-particles",
        "add-component",
        "dock-tab-content-browser",
        "dock-tab-material-editor",
        "dock-close-content-browser",
        "dock-close-material-editor",
        "workspace-tab-level",
        "workspace-tab-material",
        "workspace-close-material",
        "menu-window",
        "menu-material-editor",
        "material-editor-close",
        "material-add-texture",
        "material-add-multiply",
        "material-add-constant",
        "material-delete-node",
        "material-node-texture",
        "material-node-multiply",
        "material-node-constant",
        "material-node-output",
        "port-texture-color",
        "port-multiply-a",
        "port-multiply-b",
        "port-multiply-result",
        "port-constant-value",
        "port-output-base"
    };
}

Editor::Editor()
{
    m_Window->SetTitle("Editor");

    m_GameViewRenderer = CreateScope<GameViewRenderer>(
        m_GraphicsContext.get(),
        m_ShaderLoader.get()
    );
    m_RmlSystemInterface = CreateScope<RmlUiSystemInterface>(m_Window->GetHandle());
    m_RmlRenderer = CreateScope<RmlUiRenderer>(m_GraphicsContext.get(), m_ShaderLoader.get());
    m_GameViewTexture = m_RmlRenderer->RegisterExternalTexture(
        m_GameViewRenderer->GetRenderTarget()
    );

    Rml::SetSystemInterface(m_RmlSystemInterface.get());
    EE_CORE_ASSERT(Rml::Initialise(), "Could not initialize RmlUi.")
    EE_CORE_ASSERT(
        Rml::LoadFontFace("./Assets/Fonts/SF-Pro-Display-Regular.otf"),
        "Could not load the Editor RmlUi font."
    )

    const auto extent = m_Window->GetFramebufferExtent();
    m_RmlContext = Rml::CreateContext(
        "editor",
        { static_cast<int>(extent.Width), static_cast<int>(extent.Height) },
        m_RmlRenderer.get()
    );
    EE_CORE_ASSERT(m_RmlContext, "Could not create the Editor RmlUi context.")
    m_RmlContext->SetDensityIndependentPixelRatio(
        std::max(m_Window->GetDPIScale(), 1.0f)
    );

    m_RmlDocument = m_RmlContext->LoadDocument("./Assets/Editor/Editor.rml");
    EE_CORE_ASSERT(m_RmlDocument, "Could not load the Editor RmlUi document.")
    m_ContentBrowser = CreateScope<ContentBrowser>(
        m_RmlDocument,
        std::filesystem::path(EDITOR_PROJECT_ASSET_ROOT),
        [this](const std::filesystem::path& effectPath)
        {
            OpenEffectMaterial(effectPath);
        }
    );
    InitializeMaterialConnectionSegments();
    RegisterUiListeners();
    m_RmlDocument->Show();
}

Editor::~Editor()
{
    if (m_RmlContext)
    {
        m_ContentBrowser.reset();
        m_RmlDocument = nullptr;
        Rml::RemoveContext("editor");
        m_RmlContext = nullptr;
    }

    Rml::Shutdown();
}

void Editor::Render(const Timestep frameTime)
{
    Application::Render(frameTime);

    const auto extent = m_Window->GetFramebufferExtent();
    m_RmlContext->SetDimensions({ static_cast<int>(extent.Width), static_cast<int>(extent.Height) });
    m_RmlContext->SetDensityIndependentPixelRatio(
        std::max(m_Window->GetDPIScale(), 1.0f)
    );
    ProcessPendingInput();
    m_ContentBrowser->Update();
    m_RmlContext->Update();
    if (Rml::Element* viewport = m_RmlDocument->GetElementById("viewport"))
    {
        const glm::vec2 viewportSize {
            viewport->GetClientWidth(),
            viewport->GetClientHeight(),
        };
        if (viewportSize != m_LastDockViewportSize)
        {
            UpdateDockLayout();
            m_RmlContext->Update();
        }
    }
    UpdateMaterialConnections();

    Rml::Element* sceneView = m_RmlDocument->GetElementById("scene-view");
    const bool sceneViewActive = sceneView && sceneView->IsClassSet("dock-view-active");
    glm::vec2 sceneOrigin { 0.0f, 0.0f };
    glm::vec2 sceneSize { 0.0f, 0.0f };
    if (sceneViewActive)
    {
        const Rml::Vector2f sceneOffset = sceneView->GetAbsoluteOffset();
        sceneOrigin = { sceneOffset.x, sceneOffset.y };
        sceneSize = {
            static_cast<float>(sceneView->GetClientWidth()),
            static_cast<float>(sceneView->GetClientHeight()),
        };

        const Extent2D sceneExtent {
            static_cast<uint32_t>(std::max(sceneSize.x, 1.0f)),
            static_cast<uint32_t>(std::max(sceneSize.y, 1.0f)),
        };
        if (m_GameViewRenderer->Resize(sceneExtent))
        {
            m_RmlRenderer->UpdateExternalTexture(
                m_GameViewTexture,
                m_GameViewRenderer->GetRenderTarget()
            );
        }
    }

    m_GraphicsContext->Clear();
    m_GameViewRenderer->Render();
    m_RmlRenderer->BeginFrame(extent);
    if (sceneViewActive)
        m_RmlRenderer->RenderTexture(m_GameViewTexture, sceneOrigin, sceneSize);
    m_RmlContext->Render();
    m_RmlRenderer->EndFrame();
}

void Editor::OnEvent(Elixir::Event& event)
{
    using namespace Elixir;

    switch (event.GetEventType())
    {
        case EventType::MouseMoved:
        {
            const auto& mouseEvent = static_cast<MouseMovedEvent&>(event);
            const auto windowExtent = m_Window->GetWindowExtent();
            const auto framebufferExtent = m_Window->GetFramebufferExtent();
            const float scaleX = windowExtent.Width > 0
                ? static_cast<float>(framebufferExtent.Width) / static_cast<float>(windowExtent.Width)
                : 1.0f;
            const float scaleY = windowExtent.Height > 0
                ? static_cast<float>(framebufferExtent.Height) / static_cast<float>(windowExtent.Height)
                : 1.0f;

            QueueInput({
                .Type = ERmlInputType::MouseMove,
                .Value0 = static_cast<int>(std::lround(mouseEvent.GetX() * scaleX)),
                .Value1 = static_cast<int>(std::lround(mouseEvent.GetY() * scaleY)),
                .Modifiers = GetModifierState()
            });
            break;
        }
        case EventType::MouseButtonPressed:
        {
            const auto& mouseEvent = static_cast<MouseButtonPressedEvent&>(event);
            QueueInput({
                .Type = ERmlInputType::MouseButtonDown,
                .Value0 = mouseEvent.GetMouseButton(),
                .Modifiers = GetModifierState()
            });
            break;
        }
        case EventType::MouseButtonReleased:
        {
            const auto& mouseEvent = static_cast<MouseButtonReleasedEvent&>(event);
            QueueInput({
                .Type = ERmlInputType::MouseButtonUp,
                .Value0 = mouseEvent.GetMouseButton(),
                .Modifiers = GetModifierState()
            });
            break;
        }
        case EventType::MouseScrolled:
        {
            const auto& mouseEvent = static_cast<MouseScrolledEvent&>(event);
            QueueInput({
                .Type = ERmlInputType::MouseWheel,
                .WheelDelta = -mouseEvent.GetOffsetY(),
                .Modifiers = GetModifierState()
            });
            break;
        }
        case EventType::KeyPressed:
        {
            const auto& keyEvent = static_cast<KeyPressedEvent&>(event);
            UpdateModifierKey(keyEvent.GetKeyCode(), true);
            QueueInput({
                .Type = ERmlInputType::KeyDown,
                .Key = ConvertToRmlUiKey(keyEvent.GetKeyCode()),
                .Modifiers = GetModifierState()
            });
            break;
        }
        case EventType::KeyReleased:
        {
            const auto& keyEvent = static_cast<KeyReleasedEvent&>(event);
            UpdateModifierKey(keyEvent.GetKeyCode(), false);
            QueueInput({
                .Type = ERmlInputType::KeyUp,
                .Key = ConvertToRmlUiKey(keyEvent.GetKeyCode()),
                .Modifiers = GetModifierState()
            });
            break;
        }
        case EventType::KeyTyped:
        {
            const auto& keyEvent = static_cast<KeyTypedEvent&>(event);
            QueueInput({
                .Type = ERmlInputType::TextInput,
                .Value0 = keyEvent.GetKeyCode(),
                .Modifiers = GetModifierState()
            });
            break;
        }
        default: break;
    }

    Application::OnEvent(event);
}

void Editor::ProcessEvent(Rml::Event& event)
{
    if (!m_RmlDocument)
        return;

    if (event.GetId() != Rml::EventId::Click)
    {
        Rml::Element* element = event.GetCurrentElement();
        if (element && !element->GetAttribute<Rml::String>("data-node", "").empty())
            ProcessMaterialNodeEvent(event);
        else
            ProcessDockEvent(event);
        return;
    }

    Rml::Element* element = event.GetCurrentElement();
    if (!element)
        return;

    const Rml::String id = element->GetId();
    Rml::Element* status = m_RmlDocument->GetElementById("status-left");

    if (id == "material-add-texture")
    {
        ShowMaterialNode("material-node-texture");
        return;
    }

    if (id == "material-add-multiply")
    {
        ShowMaterialNode("material-node-multiply");
        return;
    }

    if (id == "material-add-constant")
    {
        ShowMaterialNode("material-node-constant");
        return;
    }

    if (id == "material-delete-node")
    {
        DeleteSelectedMaterialNode();
        return;
    }

    if (!element->GetAttribute<Rml::String>("data-port-direction", "").empty())
    {
        HandleMaterialPortClick(event, element);
        return;
    }

    for (const char* nodeId : MaterialNodeIds)
    {
        if (id == nodeId)
        {
            SelectMaterialNode(id);
            return;
        }
    }

    if (id == "menu-window")
    {
        Rml::Element* windowMenu = m_RmlDocument->GetElementById("window-dropdown");
        SetWindowMenuOpen(!windowMenu->IsClassSet("dropdown-open"));
        return;
    }

    if (id == "menu-material-editor")
    {
        event.StopPropagation();
        OpenMaterialEditor();
        return;
    }

    if (id == "material-editor-close")
    {
        event.StopPropagation();
        CloseMaterialEditor();
        return;
    }

    if (id == "workspace-tab-level")
    {
        Rml::Element* materialEditor = m_RmlDocument->GetElementById("material-editor");
        if (materialEditor->IsClassSet("docked-window"))
            ActivateDockTab("scene-view");
        else
        {
            materialEditor->SetClass("tool-window-hidden", true);
            SetWorkspaceTabActive(false);
        }

        if (status)
            status->SetInnerRML("Scene active");
        return;
    }

    if (id == "workspace-tab-material")
    {
        OpenMaterialEditor();
        return;
    }

    if (id == "workspace-close-material")
    {
        event.StopPropagation();
        CloseMaterialEditor();
        return;
    }

    if (id == "dock-tab-scene")
    {
        ActivateDockTab("scene-view");
        return;
    }

    for (const SDockWindowDefinition& dockWindow : DockWindows)
    {
        if (id == dockWindow.CloseId)
        {
            event.StopPropagation();
            UndockPanel(dockWindow.PanelId);
            return;
        }

        if (id == dockWindow.TabId)
        {
            ActivateDockTab(dockWindow.PanelId);
            return;
        }
    }

    for (const char* toolId : SceneTransformToolIds)
    {
        if (id == toolId)
        {
            for (const char* otherToolId : SceneTransformToolIds)
                m_RmlDocument->GetElementById(otherToolId)->SetClass("scene-tool-active", id == otherToolId);

            if (status)
                status->SetInnerRML(
                    "Active tool: " + element->GetAttribute<Rml::String>("data-label", "Tool")
                );
            return;
        }
    }

    if (id == "scene-tool-space")
    {
        const bool local = !element->IsClassSet("scene-tool-active");
        element->SetClass("scene-tool-active", local);
        if (status)
            status->SetInnerRML(local ? "Transform space: Local" : "Transform space: World");
        return;
    }

    if (id == "scene-grid-toggle" || id == "scene-snap-toggle")
    {
        const bool enabled = !element->IsClassSet("scene-tool-active");
        element->SetClass("scene-tool-active", enabled);
        if (status)
        {
            const Rml::String action = id == "scene-grid-toggle" ? "Grid" : "Transform snapping";
            status->SetInnerRML(action + (enabled ? " enabled" : " disabled"));
        }
        return;
    }

    if (id == "scene-camera-mode" || id == "scene-view-mode" || id == "scene-show-menu"
        || id == "scene-camera-speed" || id == "scene-settings")
    {
        if (status)
            status->SetInnerRML(element->GetAttribute<Rml::String>("data-label", "Scene action"));
        return;
    }

    if (id == "tool-play")
    {
        m_Playing = !m_Playing;
        element->SetClass("playing", m_Playing);
        if (status)
            status->SetInnerRML(m_Playing ? "Game running" : "Game stopped");
        return;
    }

    if (id == "tool-pause")
    {
        if (status)
            status->SetInnerRML("Game paused");
        return;
    }

    for (const char* objectId : HierarchyIds)
    {
        if (id == objectId)
        {
            for (const char* otherId : HierarchyIds)
                m_RmlDocument->GetElementById(otherId)->SetClass("tree-selected", id == otherId);

            const Rml::String objectName = element->GetAttribute<Rml::String>("data-name", "Object");
            m_RmlDocument->GetElementById("object-name")->SetInnerRML(objectName);
            if (status)
                status->SetInnerRML("Selected object: " + objectName);
            return;
        }
    }

    if (id == "add-component" && status)
        status->SetInnerRML("Add Component clicked");
}

void Editor::QueueInput(SRmlInputEvent event)
{
    const std::lock_guard lock(m_RmlInputMutex);
    m_PendingRmlInput.push_back(event);
}

void Editor::ProcessPendingInput()
{
    std::vector<SRmlInputEvent> pendingInput;
    {
        const std::lock_guard lock(m_RmlInputMutex);
        pendingInput.swap(m_PendingRmlInput);
    }

    for (const SRmlInputEvent& input : pendingInput)
    {
        switch (input.Type)
        {
            case ERmlInputType::MouseMove:
                m_RmlContext->ProcessMouseMove(input.Value0, input.Value1, input.Modifiers);
                break;
            case ERmlInputType::MouseButtonDown:
                m_RmlContext->ProcessMouseButtonDown(input.Value0, input.Modifiers);
                break;
            case ERmlInputType::MouseButtonUp:
                m_RmlContext->ProcessMouseButtonUp(input.Value0, input.Modifiers);
                break;
            case ERmlInputType::MouseWheel:
                m_RmlContext->ProcessMouseWheel(input.WheelDelta, input.Modifiers);
                break;
            case ERmlInputType::KeyDown:
                m_RmlContext->ProcessKeyDown(input.Key, input.Modifiers);
                if (input.Key == Rml::Input::KI_M && (input.Modifiers & Rml::Input::KM_CTRL))
                    OpenMaterialEditor();
                break;
            case ERmlInputType::KeyUp:
                m_RmlContext->ProcessKeyUp(input.Key, input.Modifiers);
                break;
            case ERmlInputType::TextInput:
                m_RmlContext->ProcessTextInput(static_cast<Rml::Character>(input.Value0));
                break;
        }
    }
}

void Editor::UpdateModifierKey(const int keyCode, const bool pressed)
{
    switch (keyCode)
    {
        case EE_KEY_LEFT_CONTROL: m_ModifierKeys[0] = pressed; break;
        case EE_KEY_RIGHT_CONTROL: m_ModifierKeys[1] = pressed; break;
        case EE_KEY_LEFT_SHIFT: m_ModifierKeys[2] = pressed; break;
        case EE_KEY_RIGHT_SHIFT: m_ModifierKeys[3] = pressed; break;
        case EE_KEY_LEFT_ALT: m_ModifierKeys[4] = pressed; break;
        case EE_KEY_RIGHT_ALT: m_ModifierKeys[5] = pressed; break;
        case EE_KEY_LEFT_SUPER: m_ModifierKeys[6] = pressed; break;
        case EE_KEY_RIGHT_SUPER: m_ModifierKeys[7] = pressed; break;
        default: break;
    }
}

int Editor::GetModifierState() const
{
    int modifiers = 0;
    if (m_ModifierKeys[0] || m_ModifierKeys[1]) modifiers |= Rml::Input::KM_CTRL;
    if (m_ModifierKeys[2] || m_ModifierKeys[3]) modifiers |= Rml::Input::KM_SHIFT;
    if (m_ModifierKeys[4] || m_ModifierKeys[5]) modifiers |= Rml::Input::KM_ALT;
    if (m_ModifierKeys[6] || m_ModifierKeys[7]) modifiers |= Rml::Input::KM_META;
    return modifiers;
}

void Editor::RegisterUiListeners()
{
    for (const char* elementId : InteractiveIds)
    {
        if (Rml::Element* element = m_RmlDocument->GetElementById(elementId))
            element->AddEventListener("click", this);
    }
    m_RmlDocument->GetElementById("dock-tab-scene")->AddEventListener("click", this);

    for (const SDockWindowDefinition& dockWindow : DockWindows)
    {
        if (Rml::Element* header = m_RmlDocument->GetElementById(dockWindow.HeaderId))
        {
            header->AddEventListener("dragstart", this);
            header->AddEventListener("drag", this);
            header->AddEventListener("dragend", this);
        }
        if (Rml::Element* tab = m_RmlDocument->GetElementById(dockWindow.TabId))
        {
            tab->AddEventListener("dragstart", this);
            tab->AddEventListener("dragend", this);
        }
    }

    for (const char* headerId : MaterialNodeHeaderIds)
    {
        if (Rml::Element* header = m_RmlDocument->GetElementById(headerId))
        {
            header->AddEventListener("dragstart", this);
            header->AddEventListener("drag", this);
            header->AddEventListener("dragend", this);
        }
    }

    constexpr std::array dropZoneIds {
        "dock-drop-center", "dock-drop-left", "dock-drop-right",
        "dock-drop-top", "dock-drop-bottom"
    };
    for (const char* dropZoneId : dropZoneIds)
    {
        if (Rml::Element* dropZone = m_RmlDocument->GetElementById(dropZoneId))
        {
            dropZone->AddEventListener("dragover", this);
            dropZone->AddEventListener("dragout", this);
            dropZone->AddEventListener("dragdrop", this);
        }
    }

    constexpr std::array splitterIds {
        "dock-splitter-left", "dock-splitter-right",
        "dock-splitter-top", "dock-splitter-bottom"
    };
    for (const char* splitterId : splitterIds)
    {
        if (Rml::Element* splitter = m_RmlDocument->GetElementById(splitterId))
        {
            splitter->AddEventListener("dragstart", this);
            splitter->AddEventListener("drag", this);
            splitter->AddEventListener("dragend", this);
        }
    }

    UpdateDockLayout();
}

void Editor::ProcessDockEvent(Rml::Event& event)
{
    Rml::Element* element = event.GetCurrentElement();
    if (!element)
        return;

    const Rml::String elementId = element->GetId();
    const Rml::String slotName = element->GetAttribute<Rml::String>("data-dock-slot", "");
    const auto parseSlot = [](const Rml::String& value)
    {
        if (value == "left") return EDockSlot::Left;
        if (value == "right") return EDockSlot::Right;
        if (value == "top") return EDockSlot::Top;
        if (value == "bottom") return EDockSlot::Bottom;
        if (value == "center") return EDockSlot::Center;
        return EDockSlot::Floating;
    };
    const EDockSlot eventSlot = parseSlot(slotName);
    Rml::Element* status = m_RmlDocument->GetElementById("status-left");

    switch (event.GetId())
    {
        case Rml::EventId::Dragstart:
        {
            if (elementId.starts_with("dock-splitter-"))
            {
                m_HoveredDockSlot = eventSlot;
                break;
            }

            m_DraggedDockPanel = element->GetAttribute<Rml::String>("data-panel", "");
            if (m_DraggedDockPanel.empty())
                break;

            m_DockDropHandled = false;
            const float mouseX = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
            const float mouseY = static_cast<float>(event.GetParameter<int>("mouse_y", 0));
            if (Rml::Element* panel = m_RmlDocument->GetElementById(m_DraggedDockPanel))
            {
                const Rml::Vector2f panelOffset = panel->GetAbsoluteOffset();
                m_DockDragOffset = {
                    std::clamp(mouseX - panelOffset.x, 16.0f, 180.0f),
                    std::clamp(mouseY - panelOffset.y, 12.0f, 32.0f),
                };
            }
            SetDockOverlayVisible(true);
            if (status)
            {
                const Rml::String name = element->GetAttribute<Rml::String>("data-name", "Window");
                status->SetInnerRML("Dragging " + name + " - choose a docking region");
            }
            break;
        }
        case Rml::EventId::Drag:
            if (elementId.starts_with("dock-splitter-"))
            {
                ResizeDockRegion(
                    eventSlot,
                    static_cast<float>(event.GetParameter<int>("mouse_x", 0)),
                    static_cast<float>(event.GetParameter<int>("mouse_y", 0))
                );
            }
            break;
        case Rml::EventId::Dragover:
            element->SetClass("dock-drop-hover", true);
            m_HoveredDockSlot = eventSlot;
            SetDockPreview(eventSlot, true);
            break;
        case Rml::EventId::Dragout:
            element->SetClass("dock-drop-hover", false);
            SetDockPreview(eventSlot, false);
            break;
        case Rml::EventId::Dragdrop:
        {
            Rml::Element* draggedElement = static_cast<Rml::Element*>(
                event.GetParameter<void*>("drag_element", nullptr)
            );
            Rml::String panelId = m_DraggedDockPanel;
            if (panelId.empty() && draggedElement)
                panelId = draggedElement->GetAttribute<Rml::String>("data-panel", "");
            if (!panelId.empty() && eventSlot != EDockSlot::Floating)
            {
                DockPanel(panelId, eventSlot);
                m_DockDropHandled = true;
            }
            SetDockOverlayVisible(false);
            break;
        }
        case Rml::EventId::Dragend:
            if (elementId.starts_with("dock-splitter-"))
            {
                m_HoveredDockSlot = EDockSlot::Floating;
                break;
            }

            if (!m_DraggedDockPanel.empty() && !m_DockDropHandled)
            {
                const float mouseX = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
                const float mouseY = static_cast<float>(event.GetParameter<int>("mouse_y", 0));
                if (GetPanelDockSlot(m_DraggedDockPanel) != EDockSlot::Floating)
                    UndockPanel(m_DraggedDockPanel);
                SetFloatingPanelPosition(m_DraggedDockPanel, mouseX, mouseY);
            }
            m_DraggedDockPanel.clear();
            m_DockDropHandled = false;
            SetDockOverlayVisible(false);
            break;
        default: break;
    }
}

void Editor::DockPanel(const Rml::String& panelId, const EDockSlot slot)
{
    Rml::Element* panel = m_RmlDocument->GetElementById(panelId);
    const auto slotSuffix = [](const EDockSlot value) -> const char*
    {
        switch (value)
        {
            case EDockSlot::Center: return "center";
            case EDockSlot::Left: return "left";
            case EDockSlot::Right: return "right";
            case EDockSlot::Top: return "top";
            case EDockSlot::Bottom: return "bottom";
            case EDockSlot::Floating: return "";
        }
        return "";
    };
    Rml::Element* dockContent = m_RmlDocument->GetElementById(
        Rml::String("dock-content-") + slotSuffix(slot)
    );
    Rml::Element* dockTabs = m_RmlDocument->GetElementById(
        Rml::String("dock-tabs-") + slotSuffix(slot)
    );
    if (!panel || !dockContent || !dockTabs || slot == EDockSlot::Floating)
        return;

    const EDockSlot previousSlot = GetPanelDockSlot(panelId);
    panel->SetClass("tool-window-hidden", false);
    panel->SetClass("docked-window", true);
    panel->SetProperty("left", "0px");
    panel->SetProperty("right", "0px");
    panel->SetProperty("top", "0px");
    panel->SetProperty("bottom", "0px");
    panel->SetProperty("width", "auto");
    panel->SetProperty("height", "auto");
    panel->SetProperty("margin-left", "0px");
    panel->SetProperty("margin-top", "0px");
    if (panel->GetParentNode() != dockContent)
    {
        Rml::ElementPtr ownedPanel = panel->GetParentNode()->RemoveChild(panel);
        dockContent->AppendChild(std::move(ownedPanel));
    }

    for (const SDockWindowDefinition& dockWindow : DockWindows)
    {
        if (panelId == dockWindow.PanelId)
        {
            Rml::Element* tab = m_RmlDocument->GetElementById(dockWindow.TabId);
            if (tab->GetParentNode() != dockTabs)
            {
                Rml::ElementPtr ownedTab = tab->GetParentNode()->RemoveChild(tab);
                dockTabs->AppendChild(std::move(ownedTab));
            }
            tab->SetClass("dock-tab-available", true);
            if (Rml::Element* status = m_RmlDocument->GetElementById("status-left"))
                status->SetInnerRML(Rml::String(dockWindow.Name) + " docked in " + slotSuffix(slot));
            break;
        }
    }

    m_DockSlots[panelId] = slot;
    m_ActiveDockPanels[static_cast<size_t>(slot)] = panelId;
    if (previousSlot != EDockSlot::Floating && previousSlot != slot
        && m_ActiveDockPanels[static_cast<size_t>(previousSlot)] == panelId)
    {
        Rml::String replacement = GetFirstDockedPanel(previousSlot, panelId);
        if (replacement.empty() && previousSlot == EDockSlot::Center)
            replacement = "scene-view";
        m_ActiveDockPanels[static_cast<size_t>(previousSlot)] = replacement;
    }
    UpdateDockLayout();
    ActivateDockTab(panelId);
}

void Editor::UndockPanel(const Rml::String& panelId)
{
    Rml::Element* panel = m_RmlDocument->GetElementById(panelId);
    Rml::Element* editorShell = m_RmlDocument->GetElementById("editor-shell");
    const EDockSlot previousSlot = GetPanelDockSlot(panelId);
    if (!panel || !editorShell || previousSlot == EDockSlot::Floating)
        return;

    Rml::ElementPtr ownedPanel = panel->GetParentNode()->RemoveChild(panel);
    panel->SetClass("docked-window", false);
    panel->SetClass("dock-view-active", false);
    editorShell->AppendChild(std::move(ownedPanel));
    m_DockSlots.erase(panelId);

    for (const SDockWindowDefinition& dockWindow : DockWindows)
    {
        if (panelId == dockWindow.PanelId)
        {
            Rml::Element* tab = m_RmlDocument->GetElementById(dockWindow.TabId);
            Rml::Element* centerTabs = m_RmlDocument->GetElementById("dock-tabs-center");
            if (tab->GetParentNode() != centerTabs)
            {
                Rml::ElementPtr ownedTab = tab->GetParentNode()->RemoveChild(tab);
                centerTabs->AppendChild(std::move(ownedTab));
            }
            tab->SetClass("dock-tab-available", false);
            tab->SetClass("dock-tab-active", false);
            if (Rml::Element* status = m_RmlDocument->GetElementById("status-left"))
                status->SetInnerRML(Rml::String(dockWindow.Name) + " restored to its panel");
            break;
        }
    }

    if (m_ActiveDockPanels[static_cast<size_t>(previousSlot)] == panelId)
    {
        Rml::String replacement = GetFirstDockedPanel(previousSlot, panelId);
        if (replacement.empty() && previousSlot == EDockSlot::Center)
            replacement = "scene-view";
        m_ActiveDockPanels[static_cast<size_t>(previousSlot)] = replacement;
        if (!replacement.empty())
            ActivateDockTab(replacement);
    }
    UpdateDockLayout();
    if (panelId == "material-editor")
    {
        m_RmlDocument->GetElementById("workspace-tab-material")
            ->SetClass("workspace-tab-hidden", false);
        SetWorkspaceTabActive(true);
    }
}

void Editor::ActivateDockTab(const Rml::String& panelId)
{
    const EDockSlot slot = panelId == "scene-view" ? EDockSlot::Center : GetPanelDockSlot(panelId);
    if (slot == EDockSlot::Floating)
        return;

    m_ActiveDockPanels[static_cast<size_t>(slot)] = panelId;
    Rml::Element* sceneView = m_RmlDocument->GetElementById("scene-view");
    sceneView->SetClass(
        "dock-view-active",
        m_ActiveDockPanels[static_cast<size_t>(EDockSlot::Center)] == "scene-view"
    );
    m_RmlDocument->GetElementById("scene-toolbar")
        ->SetClass("scene-context-hidden", !sceneView->IsClassSet("dock-view-active"));
    for (const SDockWindowDefinition& dockWindow : DockWindows)
    {
        Rml::Element* panel = m_RmlDocument->GetElementById(dockWindow.PanelId);
        Rml::Element* tab = m_RmlDocument->GetElementById(dockWindow.TabId);
        const EDockSlot panelSlot = GetPanelDockSlot(dockWindow.PanelId);
        const bool active = panelSlot != EDockSlot::Floating
            && m_ActiveDockPanels[static_cast<size_t>(panelSlot)] == dockWindow.PanelId;
        panel->SetClass("dock-view-active", active);
        tab->SetClass("dock-tab-active", active);
    }

    const bool materialEditorActive = panelId == "material-editor";
    if (materialEditorActive)
        m_RmlDocument->GetElementById("workspace-tab-material")
            ->SetClass("workspace-tab-hidden", false);
    SetWorkspaceTabActive(materialEditorActive);
}

Editor::EDockSlot Editor::GetPanelDockSlot(const Rml::String& panelId) const
{
    const auto slot = m_DockSlots.find(panelId);
    return slot == m_DockSlots.end() ? EDockSlot::Floating : slot->second;
}

Rml::String Editor::GetFirstDockedPanel(
    const EDockSlot slot,
    const Rml::String& excluded
) const
{
    for (const SDockWindowDefinition& dockWindow : DockWindows)
    {
        if (Rml::String(dockWindow.PanelId) != excluded && GetPanelDockSlot(dockWindow.PanelId) == slot)
            return dockWindow.PanelId;
    }
    return {};
}

bool Editor::IsDockSlotOccupied(const EDockSlot slot) const
{
    return std::ranges::any_of(DockWindows, [this, slot](const SDockWindowDefinition& window)
    {
        return GetPanelDockSlot(window.PanelId) == slot;
    });
}

void Editor::UpdateDockLayout()
{
    Rml::Element* viewport = m_RmlDocument->GetElementById("viewport");
    if (!viewport)
        return;

    const float viewportWidth = viewport->GetClientWidth();
    const float viewportHeight = viewport->GetClientHeight();
    if (viewportWidth <= 1.0f || viewportHeight <= 1.0f)
        return;
    m_LastDockViewportSize = { viewportWidth, viewportHeight };
    const bool leftVisible = IsDockSlotOccupied(EDockSlot::Left);
    const bool rightVisible = IsDockSlotOccupied(EDockSlot::Right);
    const bool topVisible = IsDockSlotOccupied(EDockSlot::Top);
    const bool bottomVisible = IsDockSlotOccupied(EDockSlot::Bottom);

    const float maximumSideWidth = viewportWidth * 0.45f;
    const float minimumSideWidth = std::min(180.0f, maximumSideWidth);
    const float maximumTopHeight = viewportHeight * 0.45f;
    const float maximumBottomHeight = viewportHeight * 0.55f;
    const float minimumTopHeight = std::min(140.0f, maximumTopHeight);
    const float minimumBottomHeight = std::min(140.0f, maximumBottomHeight);
    m_LeftDockWidth = std::clamp(m_LeftDockWidth, minimumSideWidth, maximumSideWidth);
    m_RightDockWidth = std::clamp(m_RightDockWidth, minimumSideWidth, maximumSideWidth);
    m_TopDockHeight = std::clamp(m_TopDockHeight, minimumTopHeight, maximumTopHeight);
    m_BottomDockHeight = std::clamp(m_BottomDockHeight, minimumBottomHeight, maximumBottomHeight);

    const float maximumHorizontalDocking = std::max(viewportWidth - 260.0f, 0.0f);
    const float horizontalDocking = (leftVisible ? m_LeftDockWidth : 0.0f)
        + (rightVisible ? m_RightDockWidth : 0.0f);
    if (horizontalDocking > maximumHorizontalDocking && horizontalDocking > 0.0f)
    {
        const float scale = maximumHorizontalDocking / horizontalDocking;
        if (leftVisible)
            m_LeftDockWidth *= scale;
        if (rightVisible)
            m_RightDockWidth *= scale;
    }

    const float maximumVerticalDocking = std::max(viewportHeight - 180.0f, 0.0f);
    const float verticalDocking = (topVisible ? m_TopDockHeight : 0.0f)
        + (bottomVisible ? m_BottomDockHeight : 0.0f);
    if (verticalDocking > maximumVerticalDocking && verticalDocking > 0.0f)
    {
        const float scale = maximumVerticalDocking / verticalDocking;
        if (topVisible)
            m_TopDockHeight *= scale;
        if (bottomVisible)
            m_BottomDockHeight *= scale;
    }

    const float left = leftVisible ? m_LeftDockWidth : 0.0f;
    const float right = rightVisible ? m_RightDockWidth : 0.0f;
    const float top = topVisible ? m_TopDockHeight : 0.0f;
    const float bottom = bottomVisible ? m_BottomDockHeight : 0.0f;
    const auto setPx = [](Rml::Element* element, const Rml::PropertyId property, const float value)
    {
        element->SetProperty(property, Rml::Property(value, Rml::Property::PX));
    };
    const auto setBox = [&setPx](
        Rml::Element* element,
        const float boxLeft,
        const float boxRight,
        const float boxTop,
        const float boxBottom
    )
    {
        setPx(element, Rml::PropertyId::Left, boxLeft);
        setPx(element, Rml::PropertyId::Right, boxRight);
        setPx(element, Rml::PropertyId::Top, boxTop);
        setPx(element, Rml::PropertyId::Bottom, boxBottom);
        element->SetProperty("width", "auto");
        element->SetProperty("height", "auto");
    };

    Rml::Element* centerRegion = m_RmlDocument->GetElementById("dock-region-center");
    Rml::Element* leftRegion = m_RmlDocument->GetElementById("dock-region-left");
    Rml::Element* rightRegion = m_RmlDocument->GetElementById("dock-region-right");
    Rml::Element* topRegion = m_RmlDocument->GetElementById("dock-region-top");
    Rml::Element* bottomRegion = m_RmlDocument->GetElementById("dock-region-bottom");
    setBox(centerRegion, left, right, top, bottom);
    setBox(leftRegion, 0.0f, viewportWidth - left, 0.0f, 0.0f);
    setBox(rightRegion, viewportWidth - right, 0.0f, 0.0f, 0.0f);
    setBox(topRegion, left, right, 0.0f, viewportHeight - top);
    setBox(bottomRegion, left, right, viewportHeight - bottom, 0.0f);

    leftRegion->SetClass("dock-region-visible", leftVisible);
    rightRegion->SetClass("dock-region-visible", rightVisible);
    topRegion->SetClass("dock-region-visible", topVisible);
    bottomRegion->SetClass("dock-region-visible", bottomVisible);
    centerRegion->SetClass("dock-region-tabs-visible", IsDockSlotOccupied(EDockSlot::Center));
    leftRegion->SetClass("dock-region-tabs-visible", leftVisible);
    rightRegion->SetClass("dock-region-tabs-visible", rightVisible);
    topRegion->SetClass("dock-region-tabs-visible", topVisible);
    bottomRegion->SetClass("dock-region-tabs-visible", bottomVisible);

    Rml::Element* leftSplitter = m_RmlDocument->GetElementById("dock-splitter-left");
    Rml::Element* rightSplitter = m_RmlDocument->GetElementById("dock-splitter-right");
    Rml::Element* topSplitter = m_RmlDocument->GetElementById("dock-splitter-top");
    Rml::Element* bottomSplitter = m_RmlDocument->GetElementById("dock-splitter-bottom");
    leftSplitter->SetClass("dock-splitter-visible", leftVisible);
    rightSplitter->SetClass("dock-splitter-visible", rightVisible);
    topSplitter->SetClass("dock-splitter-visible", topVisible);
    bottomSplitter->SetClass("dock-splitter-visible", bottomVisible);
    setPx(leftSplitter, Rml::PropertyId::Left, left - 3.0f);
    leftSplitter->SetProperty("right", "auto");
    setPx(rightSplitter, Rml::PropertyId::Right, right - 3.0f);
    rightSplitter->SetProperty("left", "auto");
    setPx(topSplitter, Rml::PropertyId::Top, top - 3.0f);
    topSplitter->SetProperty("bottom", "auto");
    setPx(bottomSplitter, Rml::PropertyId::Bottom, bottom - 3.0f);
    bottomSplitter->SetProperty("top", "auto");
    setPx(topSplitter, Rml::PropertyId::Left, left);
    setPx(topSplitter, Rml::PropertyId::Right, right);
    setPx(bottomSplitter, Rml::PropertyId::Left, left);
    setPx(bottomSplitter, Rml::PropertyId::Right, right);
}

void Editor::ResizeDockRegion(const EDockSlot slot, const float mouseX, const float mouseY)
{
    Rml::Element* viewport = m_RmlDocument->GetElementById("viewport");
    const Rml::Vector2f origin = viewport->GetAbsoluteOffset();
    const float width = viewport->GetClientWidth();
    const float height = viewport->GetClientHeight();
    const float minimumSideWidth = std::min(180.0f, width * 0.45f);
    const float minimumDockHeight = std::min(140.0f, height * 0.45f);
    const float localX = mouseX - origin.x;
    const float localY = mouseY - origin.y;
    switch (slot)
    {
        case EDockSlot::Left: m_LeftDockWidth = std::clamp(localX, minimumSideWidth, width * 0.55f); break;
        case EDockSlot::Right: m_RightDockWidth = std::clamp(width - localX, minimumSideWidth, width * 0.55f); break;
        case EDockSlot::Top: m_TopDockHeight = std::clamp(localY, minimumDockHeight, height * 0.55f); break;
        case EDockSlot::Bottom: m_BottomDockHeight = std::clamp(height - localY, minimumDockHeight, height * 0.65f); break;
        default: return;
    }
    UpdateDockLayout();
}

void Editor::SetFloatingPanelPosition(
    const Rml::String& panelId,
    const float mouseX,
    const float mouseY
)
{
    Rml::Element* panel = m_RmlDocument->GetElementById(panelId);
    Rml::Element* shell = m_RmlDocument->GetElementById("editor-shell");
    if (!panel || !shell)
        return;

    const float width = panelId == "content-browser" ? 900.0f : 820.0f;
    const float height = panelId == "content-browser" ? 320.0f : 510.0f;
    const float left = std::clamp(mouseX - m_DockDragOffset.x, 0.0f, std::max(shell->GetClientWidth() - width, 0.0f));
    const float top = std::clamp(mouseY - m_DockDragOffset.y, 90.0f, std::max(shell->GetClientHeight() - height - 24.0f, 90.0f));
    panel->SetClass("tool-window-hidden", false);
    panel->SetProperty("right", "auto");
    panel->SetProperty("bottom", "auto");
    panel->SetProperty("margin-left", "0px");
    panel->SetProperty("margin-top", "0px");
    panel->SetProperty(Rml::PropertyId::Left, Rml::Property(left, Rml::Property::PX));
    panel->SetProperty(Rml::PropertyId::Top, Rml::Property(top, Rml::Property::PX));
    panel->SetProperty(Rml::PropertyId::Width, Rml::Property(width, Rml::Property::PX));
    panel->SetProperty(Rml::PropertyId::Height, Rml::Property(height, Rml::Property::PX));
}

void Editor::SetDockPreview(const EDockSlot slot, const bool visible)
{
    Rml::Element* preview = m_RmlDocument->GetElementById("dock-preview");
    Rml::Element* viewport = m_RmlDocument->GetElementById("viewport");
    if (!preview || !viewport || !visible || slot == EDockSlot::Floating)
    {
        if (preview)
            preview->SetClass("dock-preview-visible", false);
        return;
    }

    const float width = viewport->GetClientWidth();
    const float height = viewport->GetClientHeight();
    float left = 0.0f;
    float top = 0.0f;
    float previewWidth = width;
    float previewHeight = height;
    if (slot == EDockSlot::Left || slot == EDockSlot::Right)
        previewWidth = width * 0.30f;
    if (slot == EDockSlot::Top || slot == EDockSlot::Bottom)
        previewHeight = height * 0.30f;
    if (slot == EDockSlot::Right)
        left = width - previewWidth;
    if (slot == EDockSlot::Bottom)
        top = height - previewHeight;

    preview->SetProperty(Rml::PropertyId::Left, Rml::Property(left, Rml::Property::PX));
    preview->SetProperty(Rml::PropertyId::Top, Rml::Property(top, Rml::Property::PX));
    preview->SetProperty(Rml::PropertyId::Width, Rml::Property(previewWidth, Rml::Property::PX));
    preview->SetProperty(Rml::PropertyId::Height, Rml::Property(previewHeight, Rml::Property::PX));
    preview->SetClass("dock-preview-visible", true);
}

void Editor::SetWorkspaceTabActive(const bool materialEditorActive)
{
    m_RmlDocument->GetElementById("workspace-tab-level")
        ->SetClass("workspace-tab-active", !materialEditorActive);
    m_RmlDocument->GetElementById("workspace-tab-material")
        ->SetClass("workspace-tab-active", materialEditorActive);
}

void Editor::SetDockOverlayVisible(const bool visible)
{
    if (Rml::Element* overlay = m_RmlDocument->GetElementById("dock-overlay"))
    {
        overlay->SetClass("dock-overlay-visible", visible);
        if (!visible)
        {
            constexpr std::array dropZoneIds {
                "dock-drop-center", "dock-drop-left", "dock-drop-right",
                "dock-drop-top", "dock-drop-bottom"
            };
            for (const char* id : dropZoneIds)
                m_RmlDocument->GetElementById(id)->SetClass("dock-drop-hover", false);
            SetDockPreview(EDockSlot::Floating, false);
            m_HoveredDockSlot = EDockSlot::Floating;
        }
    }
}

void Editor::OpenMaterialEditor()
{
    SetWindowMenuOpen(false);

    Rml::Element* materialEditor = m_RmlDocument->GetElementById("material-editor");
    m_RmlDocument->GetElementById("workspace-tab-material")
        ->SetClass("workspace-tab-hidden", false);
    if (materialEditor->IsClassSet("docked-window"))
        ActivateDockTab("material-editor");
    else
    {
        materialEditor->SetClass("tool-window-hidden", false);
        SetWorkspaceTabActive(true);
    }

    if (Rml::Element* status = m_RmlDocument->GetElementById("status-left"))
        status->SetInnerRML("Material Editor opened");
}

void Editor::OpenEffectMaterial(const std::filesystem::path& effectPath)
{
    using namespace Elixir::Aether;

    const Ref<System> effect = Effect::LoadEffectFile(effectPath);
    if (!effect)
    {
        if (Rml::Element* status = m_RmlDocument->GetElementById("status-left"))
            status->SetInnerRML("Could not open effect: " + EscapeRml(effectPath.filename().string()));
        return;
    }

    const Emitter* materialEmitter = nullptr;
    for (const auto& emitter : effect->GetEmitters())
    {
        if (emitter->GetMaterialDescription())
        {
            materialEmitter = emitter.get();
            break;
        }
    }

    if (!materialEmitter)
    {
        if (Rml::Element* status = m_RmlDocument->GetElementById("status-left"))
        {
            status->SetInnerRML(
                "Effect has no authored material: " + EscapeRml(effect->GetName())
            );
        }
        return;
    }

    const auto& material = *materialEmitter->GetMaterialDescription();
    const std::string materialName = effect->GetName() + " / " + materialEmitter->GetName();
    EE_CORE_TRACE(
        "Opening effect material: [Effect = {}, Emitter = {}, RenderMode = {}, Texture = {}].",
        effect->GetName(),
        materialEmitter->GetName(),
        RenderModeName(materialEmitter->GetRenderMode()),
        material.BaseColorTexturePath.empty() ? "None" : material.BaseColorTexturePath
    )

    if (Rml::Element* list = m_RmlDocument->GetElementById("material-list-rows"))
    {
        list->SetInnerRML(
            "<div class=\"material-row material-row-selected\">"
            + EscapeRml(materialName) + "</div>"
        );
    }
    if (Rml::Element* tab = m_RmlDocument->GetElementById("workspace-material-label"))
        tab->SetInnerRML(EscapeRml(materialEmitter->GetName()));
    if (Rml::Element* header = m_RmlDocument->GetElementById("material-editor-title"))
        header->SetInnerRML(EscapeRml(materialName));
    if (Rml::Element* textureNode = m_RmlDocument->GetElementById("material-texture-name"))
    {
        const std::string textureName = material.BaseColorTexturePath.empty()
            ? "No texture"
            : std::filesystem::path(material.BaseColorTexturePath).filename().string();
        textureNode->SetInnerRML(EscapeRml(textureName));
    }
    if (Rml::Element* constantNode = m_RmlDocument->GetElementById("material-constant-value"))
        constantNode->SetInnerRML("Color: " + FormatVector(material.BaseColor));

    const bool hasTexture = !material.BaseColorTexturePath.empty();
    m_RmlDocument->GetElementById("material-node-texture")
        ->SetClass("material-node-hidden", !hasTexture);
    m_RmlDocument->GetElementById("material-node-constant")
        ->SetClass("material-node-hidden", false);
    m_MaterialConnections = { hasTexture, true, true };
    if (!m_PendingMaterialOutputPort.empty())
    {
        m_RmlDocument->GetElementById(m_PendingMaterialOutputPort)
            ->SetClass("material-port-pending", false);
        m_PendingMaterialOutputPort.clear();
    }
    SelectMaterialNode(hasTexture ? "material-node-texture" : "material-node-constant");
    UpdateMaterialConnections();

    if (Rml::Element* properties = m_RmlDocument->GetElementById("material-asset-properties"))
    {
        properties->SetInnerRML(std::format(
            "<div class=\"material-property\">Effect: {}</div>"
            "<div class=\"material-property\">Emitter: {}</div>"
            "<div class=\"material-property\">Render mode: {}</div>"
            "<div class=\"material-property\">Base color: {}</div>"
            "<div class=\"material-property\">Opacity: {:.2f}</div>"
            "<div class=\"material-property\">Emissive: {}</div>"
            "<div class=\"material-property\">Texture: {}</div>",
            EscapeRml(effect->GetName()),
            EscapeRml(materialEmitter->GetName()),
            RenderModeName(materialEmitter->GetRenderMode()),
            FormatVector(material.BaseColor),
            material.Opacity,
            FormatVector(material.Emissive),
            EscapeRml(material.BaseColorTexturePath.empty() ? "None" : material.BaseColorTexturePath)
        ));
    }

    OpenMaterialEditor();
    if (Rml::Element* status = m_RmlDocument->GetElementById("status-left"))
        status->SetInnerRML("Opened effect material: " + EscapeRml(materialName));
}

void Editor::CloseMaterialEditor()
{
    Rml::Element* materialEditor = m_RmlDocument->GetElementById("material-editor");
    if (materialEditor->IsClassSet("docked-window"))
        UndockPanel("material-editor");
    materialEditor->SetClass("tool-window-hidden", true);

    m_RmlDocument->GetElementById("workspace-tab-material")
        ->SetClass("workspace-tab-hidden", true);
    SetWorkspaceTabActive(false);

    if (Rml::Element* status = m_RmlDocument->GetElementById("status-left"))
        status->SetInnerRML("Material Editor closed");
}

void Editor::SetWindowMenuOpen(const bool open)
{
    m_RmlDocument->GetElementById("window-dropdown")->SetClass("dropdown-open", open);
    m_RmlDocument->GetElementById("menu-window")->SetClass("menu-open", open);
}

void Editor::ProcessMaterialNodeEvent(Rml::Event& event)
{
    Rml::Element* header = event.GetCurrentElement();
    if (!header)
        return;

    const Rml::String nodeId = header->GetAttribute<Rml::String>("data-node", "");
    Rml::Element* node = m_RmlDocument->GetElementById(nodeId);
    Rml::Element* canvas = m_RmlDocument->GetElementById("material-canvas");
    if (!node || !canvas)
        return;

    const float mouseX = static_cast<float>(event.GetParameter<int>("mouse_x", 0));
    const float mouseY = static_cast<float>(event.GetParameter<int>("mouse_y", 0));

    switch (event.GetId())
    {
        case Rml::EventId::Dragstart:
        {
            SelectMaterialNode(nodeId);
            const Rml::Vector2f nodeOffset = node->GetAbsoluteOffset();
            m_MaterialNodeDragOffsetX = mouseX - nodeOffset.x;
            m_MaterialNodeDragOffsetY = mouseY - nodeOffset.y;
            node->SetClass("material-node-dragging", true);
            break;
        }
        case Rml::EventId::Drag:
        {
            const Rml::Vector2f canvasOffset = canvas->GetAbsoluteOffset();
            const float maxLeft = std::max(0.0f, canvas->GetClientWidth() - node->GetOffsetWidth());
            const float maxTop = std::max(0.0f, canvas->GetClientHeight() - node->GetOffsetHeight());
            const float left = std::clamp(
                mouseX - canvasOffset.x - m_MaterialNodeDragOffsetX,
                0.0f,
                maxLeft
            );
            const float top = std::clamp(
                mouseY - canvasOffset.y - m_MaterialNodeDragOffsetY,
                38.0f,
                maxTop
            );
            node->SetProperty("right", "auto");
            node->SetProperty(Rml::PropertyId::Left, Rml::Property(left, Rml::Property::PX));
            node->SetProperty(Rml::PropertyId::Top, Rml::Property(top, Rml::Property::PX));
            break;
        }
        case Rml::EventId::Dragend:
            node->SetClass("material-node-dragging", false);
            if (Rml::Element* status = m_RmlDocument->GetElementById("status-left"))
                status->SetInnerRML("Moved material node: " + node->GetAttribute<Rml::String>("data-title", "Node"));
            break;
        default: break;
    }
}

void Editor::SelectMaterialNode(const Rml::String& nodeId)
{
    m_SelectedMaterialNode = nodeId;
    for (const char* otherNodeId : MaterialNodeIds)
        m_RmlDocument->GetElementById(otherNodeId)->SetClass("material-node-selected", nodeId == otherNodeId);

    Rml::Element* selection = m_RmlDocument->GetElementById("material-selected-node");
    if (nodeId.empty())
    {
        selection->SetInnerRML("No node selected");
        return;
    }

    Rml::Element* node = m_RmlDocument->GetElementById(nodeId);
    selection->SetInnerRML("Selected: " + node->GetAttribute<Rml::String>("data-title", "Node"));
}

void Editor::HandleMaterialPortClick(Rml::Event& event, Rml::Element* port)
{
    event.StopPropagation();

    const Rml::String portId = port->GetId();
    const Rml::String direction = port->GetAttribute<Rml::String>("data-port-direction", "");
    Rml::Element* status = m_RmlDocument->GetElementById("status-left");

    if (direction == "output")
    {
        if (!m_PendingMaterialOutputPort.empty())
            m_RmlDocument->GetElementById(m_PendingMaterialOutputPort)->SetClass("material-port-pending", false);

        if (m_PendingMaterialOutputPort == portId)
        {
            m_PendingMaterialOutputPort.clear();
            if (status)
                status->SetInnerRML("Material connection cancelled");
        }
        else
        {
            m_PendingMaterialOutputPort = portId;
            port->SetClass("material-port-pending", true);
            if (status)
                status->SetInnerRML("Select a compatible input port");
        }
        return;
    }

    bool changed = false;
    if (m_PendingMaterialOutputPort.empty())
    {
        for (size_t index = 0; index < MaterialConnections.size(); ++index)
        {
            if (portId == MaterialConnections[index].InputPortId && m_MaterialConnections[index])
            {
                m_MaterialConnections[index] = false;
                changed = true;
            }
        }
        if (status)
            status->SetInnerRML(changed ? "Material connection removed" : "Select an output port first");
    }
    else
    {
        for (size_t index = 0; index < MaterialConnections.size(); ++index)
        {
            const SMaterialConnectionDefinition& connection = MaterialConnections[index];
            if (m_PendingMaterialOutputPort == connection.OutputPortId && portId == connection.InputPortId)
            {
                m_MaterialConnections[index] = true;
                changed = true;
                break;
            }
        }

        m_RmlDocument->GetElementById(m_PendingMaterialOutputPort)
            ->SetClass("material-port-pending", false);
        m_PendingMaterialOutputPort.clear();
        if (status)
            status->SetInnerRML(changed ? "Material ports connected" : "Incompatible material ports");
    }

    UpdateMaterialConnections();
}

void Editor::UpdateMaterialConnections()
{
    Rml::Element* materialEditor = m_RmlDocument->GetElementById("material-editor");
    if (materialEditor->IsClassSet("tool-window-hidden"))
        return;

    Rml::Element* canvas = m_RmlDocument->GetElementById("material-canvas");
    if (!canvas)
        return;

    const Rml::Vector2f canvasOffset = canvas->GetAbsoluteOffset();
    for (size_t index = 0; index < MaterialConnections.size(); ++index)
    {
        const SMaterialConnectionDefinition& definition = MaterialConnections[index];
        Rml::Element* line = m_RmlDocument->GetElementById(definition.ElementId);
        Rml::Element* outputNode = m_RmlDocument->GetElementById(definition.OutputNodeId);
        Rml::Element* inputNode = m_RmlDocument->GetElementById(definition.InputNodeId);
        const bool visible = m_MaterialConnections[index]
            && !outputNode->IsClassSet("material-node-hidden")
            && !inputNode->IsClassSet("material-node-hidden");
        line->SetClass("material-link-hidden", !visible);
        if (!visible)
            continue;

        Rml::Element* outputPort = m_RmlDocument->GetElementById(definition.OutputPortId);
        Rml::Element* inputPort = m_RmlDocument->GetElementById(definition.InputPortId);
        const Rml::Vector2f outputOffset = outputPort->GetAbsoluteOffset();
        const Rml::Vector2f inputOffset = inputPort->GetAbsoluteOffset();
        const float startX = outputOffset.x - canvasOffset.x + outputPort->GetOffsetWidth() * 0.5f;
        const float startY = outputOffset.y - canvasOffset.y + outputPort->GetOffsetHeight() * 0.5f;
        const float endX = inputOffset.x - canvasOffset.x + inputPort->GetOffsetWidth() * 0.5f;
        const float endY = inputOffset.y - canvasOffset.y + inputPort->GetOffsetHeight() * 0.5f;
        const float tangent = std::max(72.0f, std::abs(endX - startX) * 0.5f);
        const float control1X = startX + tangent;
        const float control1Y = startY;
        const float control2X = endX - tangent;
        const float control2Y = endY;

        const auto evaluateCurve = [&](const float t)
        {
            const float inverseT = 1.0f - t;
            const float inverseTSquared = inverseT * inverseT;
            const float tSquared = t * t;
            return Rml::Vector2f {
                inverseTSquared * inverseT * startX
                    + 3.0f * inverseTSquared * t * control1X
                    + 3.0f * inverseT * tSquared * control2X
                    + tSquared * t * endX,
                inverseTSquared * inverseT * startY
                    + 3.0f * inverseTSquared * t * control1Y
                    + 3.0f * inverseT * tSquared * control2Y
                    + tSquared * t * endY
            };
        };

        const int segmentCount = line->GetNumChildren();
        for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
        {
            const float startT = static_cast<float>(segmentIndex) / static_cast<float>(segmentCount);
            const float endT = static_cast<float>(segmentIndex + 1) / static_cast<float>(segmentCount);
            const Rml::Vector2f segmentStart = evaluateCurve(startT);
            const Rml::Vector2f segmentEnd = evaluateCurve(endT);
            const float deltaX = segmentEnd.x - segmentStart.x;
            const float deltaY = segmentEnd.y - segmentStart.y;
            const float length = std::hypot(deltaX, deltaY) + 1.0f;
            const float angle = std::atan2(deltaY, deltaX) * 57.2957795f;

            Rml::Element* segment = line->GetChild(segmentIndex);
            segment->SetProperty(
                Rml::PropertyId::Left,
                Rml::Property(segmentStart.x, Rml::Property::PX)
            );
            segment->SetProperty(
                Rml::PropertyId::Top,
                Rml::Property(segmentStart.y, Rml::Property::PX)
            );
            segment->SetProperty(Rml::PropertyId::Width, Rml::Property(length, Rml::Property::PX));
            segment->SetProperty("transform", std::format("rotate({:.2f}deg)", angle));
        }
    }
}

void Editor::InitializeMaterialConnectionSegments()
{
    Rml::String segmentMarkup;
    for (int index = 0; index < MaterialConnectionSegmentCount; ++index)
        segmentMarkup += "<div class=\"material-link-segment\"></div>";

    for (const SMaterialConnectionDefinition& connection : MaterialConnections)
        m_RmlDocument->GetElementById(connection.ElementId)->SetInnerRML(segmentMarkup);
}

void Editor::ShowMaterialNode(const Rml::String& nodeId)
{
    Rml::Element* node = m_RmlDocument->GetElementById(nodeId);
    node->SetClass("material-node-hidden", false);
    SelectMaterialNode(nodeId);
    if (Rml::Element* status = m_RmlDocument->GetElementById("status-left"))
        status->SetInnerRML("Material node available: " + node->GetAttribute<Rml::String>("data-title", "Node"));
}

void Editor::DeleteSelectedMaterialNode()
{
    Rml::Element* status = m_RmlDocument->GetElementById("status-left");
    if (m_SelectedMaterialNode.empty())
    {
        if (status)
            status->SetInnerRML("No material node selected");
        return;
    }

    if (m_SelectedMaterialNode == "material-node-output")
    {
        if (status)
            status->SetInnerRML("Material Output cannot be deleted");
        return;
    }

    for (size_t index = 0; index < MaterialConnections.size(); ++index)
    {
        const SMaterialConnectionDefinition& connection = MaterialConnections[index];
        if (m_SelectedMaterialNode == connection.OutputNodeId || m_SelectedMaterialNode == connection.InputNodeId)
            m_MaterialConnections[index] = false;
    }

    m_RmlDocument->GetElementById(m_SelectedMaterialNode)->SetClass("material-node-hidden", true);
    if (!m_PendingMaterialOutputPort.empty())
        m_RmlDocument->GetElementById(m_PendingMaterialOutputPort)->SetClass("material-port-pending", false);
    m_PendingMaterialOutputPort.clear();
    SelectMaterialNode("");
    UpdateMaterialConnections();
    if (status)
        status->SetInnerRML("Material node deleted");
}

Elixir::Application* Elixir::CreateApplication()
{
    return new Editor();
}
