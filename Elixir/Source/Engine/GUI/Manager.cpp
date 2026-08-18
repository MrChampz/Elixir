#include "epch.h"
#include "Manager.h"

#include <Engine/GUI/Panel.h>
#include <Engine/Input/InputCodes.h>
#include <Engine/Input/InputManager.h>

namespace Elixir::GUI
{
    void Manager::Initialize(
        const GraphicsContext* context,
        const ShaderLoader* shaderLoader,
        const Extent2D& extent
    )
    {
        m_Renderer = CreateScope<Renderer>(context, shaderLoader, extent);
        m_Initialized = true;
    }

    void Manager::Shutdown()
    {
        m_Initialized = false;
    }

    void Manager::ArrangeLayout(const Extent2D& extent) const
    {
        if (m_RootWidget)
        {
            const SRect rootGeometry = { { 0, 0 }, { extent.Width, extent.Height } };
            m_RootWidget->ArrangeChildren(rootGeometry);
        }
    }

    void Manager::Update(const Timestep frameTime)
    {
        ProcessInput();

        if (m_RootWidget)
            m_RootWidget->Update(frameTime);
    }

    void Manager::Render()
    {
        if (!m_RootWidget || !m_RootWidget->IsRenderVisible()) return;

        if (NeedsRebuild())
        {
            AssembleFrame();
            m_Renderer->Rebuild(m_RenderBatch);
            MarkRebuilt();
        }

        m_Renderer->Draw();
    }

    void Manager::ProcessEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<FramebufferResizeEvent>(EE_BIND_EVENT_FN(Manager::HandleFramebufferResize));
        dispatcher.Dispatch<KeyPressedEvent>(EE_BIND_EVENT_FN(Manager::HandleKeyPressed));
        dispatcher.Dispatch<KeyTypedEvent>(EE_BIND_EVENT_FN(Manager::HandleKeyTyped));
    }

    bool Manager::WantsMouse() const
    {
        return !m_HoverPath.empty() || !m_MouseCapture.expired();
    }

    void Manager::AssembleFrame()
    {
        m_RenderBatch.Clear();

        if (m_RootWidget && m_RootWidget->IsRenderVisible())
        {
            int zCursor = 0;
            bool rebuilt = false;
            m_RootWidget->CollectDrawCommands(m_RenderBatch, zCursor, rebuilt);
        }

        m_RenderBatch.Sort();
    }

    bool Manager::NeedsRebuild() const
    {
        return Widget::CurrentDirtyEpoch() != m_LastRenderedEpoch
            || m_LastRenderedRoot.lock() != m_RootWidget;
    }

    void Manager::MarkRebuilt()
    {
        m_LastRenderedEpoch = Widget::CurrentDirtyEpoch();
        m_LastRenderedRoot = m_RootWidget;
    }

    bool Manager::HandleFramebufferResize(const FramebufferResizeEvent& event) const
    {
        const Extent2D extent = { event.GetWidth(), event.GetHeight() };
        m_Renderer->Resize(extent);

        return true;
    }

    bool Manager::HandleKeyPressed(const KeyPressedEvent& event) const
    {
        for (auto widget = m_FocusedWidget; widget; widget = widget->GetParent())
        {
            if (widget->HandleKeyPressed(event).EventHandled)
                return true;
        }

        return false;
    }

    bool Manager::HandleKeyTyped(const KeyTypedEvent& event) const
    {
        for (auto widget = m_FocusedWidget; widget; widget = widget->GetParent())
        {
            if (widget->HandleKeyTyped(event).EventHandled)
                return true;
        }

        return false;
    }

    void Manager::ProcessInput()
    {
        const auto [x, y] = InputManager::GetMousePosition();
        m_LastMousePos = m_MousePos;
        m_MousePos = { x, y };
        m_MouseMoved = m_MousePos != m_LastMousePos;

        const auto isMouseDown = InputManager::IsMouseButtonDown(EE_MOUSE_BUTTON_LEFT);

        m_MousePressed = isMouseDown && !m_WasMouseDown;
        m_MouseReleased = !isMouseDown && m_WasMouseDown;
        m_WasMouseDown = isMouseDown;

        if (!m_RootWidget) return;

        std::vector<Ref<Widget>> hitPath;
        m_RootWidget->HitTest(m_MousePos, hitPath);

        UpdateHoverPath(hitPath);

        if (m_MousePressed)
            ProcessMousePress(hitPath);

        if (m_MouseReleased)
            ProcessMouseRelease(hitPath);

        if (m_MouseMoved)
            ProcessMouseMove(hitPath);
    }

    void Manager::UpdateHoverPath(const std::vector<Ref<Widget>>& path)
    {
        // Leave widgets that were hovered but fell out of the path, deepest (leaf) first.
        for (auto it = m_HoverPath.rbegin(); it != m_HoverPath.rend(); ++it)
        {
            if (std::ranges::find(path, *it) == path.end())
                (*it)->HandleMouseLeave();
        }

        // Enter widgets newly under the cursor, root first.
        for (const auto& widget : path)
        {
            if (std::ranges::find(m_HoverPath, widget) == m_HoverPath.end())
                widget->HandleMouseEnter();
        }

        m_HoverPath = path;
    }

    void Manager::ProcessMousePress(const std::vector<Ref<Widget>>& path)
    {
        const auto event = MouseButtonPressedEvent(EE_MOUSE_BUTTON_LEFT, m_MousePos);

        for (auto it = path.rbegin(); it != path.rend(); ++it)
        {
            const auto& widget = *it;
            const SInputReply reply = widget->HandleMouseDown(event);

            if (reply.EventHandled)
            {
                if (reply.CaptureMouse)
                    m_MouseCapture = widget;

                m_PressedWidget = widget;
                SetFocusedWidget(widget);
                return;
            }
        }

        // Nobody under the cursor wanted the press: treat it as "clicked outside".
        SetFocusedWidget(nullptr);
    }

    void Manager::ProcessMouseRelease(const std::vector<Ref<Widget>>& path)
    {
        const auto event = MouseButtonReleasedEvent(EE_MOUSE_BUTTON_LEFT, m_MousePos);

        if (const auto captured = m_MouseCapture.lock())
        {
            captured->HandleMouseUp(event);
        }
        else
        {
            for (auto it = path.rbegin(); it != path.rend(); ++it)
                if ((*it)->HandleMouseUp(event).EventHandled)
                    break;
        }

        if (m_PressedWidget && std::ranges::find(path, m_PressedWidget) != path.end())
            m_PressedWidget->HandleClick();

        m_MouseCapture.reset();
        m_PressedWidget = nullptr;
    }

    void Manager::ProcessMouseMove(const std::vector<Ref<Widget>>& path)
    {
        const auto event = MouseMovedEvent(m_MousePos);

        if (const auto captured = m_MouseCapture.lock())
        {
            captured->HandleMouseMove(event);
            return;
        }

        for (auto it = path.rbegin(); it != path.rend(); ++it)
        {
            if ((*it)->HandleMouseMove(event).EventHandled)
                break;
        }
    }

    void Manager::SetFocusedWidget(const Ref<Widget>& widget)
    {
        if (m_FocusedWidget == widget) return;

        if (m_FocusedWidget)
            m_FocusedWidget->HandleLostFocus();

        m_FocusedWidget = widget;

        if (m_FocusedWidget)
            m_FocusedWidget->HandleFocus();
    }
}