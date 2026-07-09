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
        if (!m_RootWidget || !m_RootWidget->IsVisible()) return;

        // Skip re-assembling the batch and re-uploading GPU buffers when nothing in the GUI
        // has changed since the last rendered frame; always re-issue the draws (the GUI is
        // composited over the scene every frame).
        const uint64_t epoch = Widget::CurrentDirtyEpoch();
        if (epoch != m_LastRenderedEpoch)
        {
            AssembleFrame();
            m_Renderer->Rebuild(m_RenderBatch);
            m_LastRenderedEpoch = epoch;
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

    void Manager::AssembleFrame()
    {
        m_RenderBatch.Clear();

        if (m_RootWidget && m_RootWidget->IsVisible())
        {
            int zCursor = 0;
            bool rebuilt = false;
            m_RootWidget->CollectDrawCommands(m_RenderBatch, zCursor, rebuilt);
        }

        m_RenderBatch.Sort();
    }

    bool Manager::HandleFramebufferResize(const FramebufferResizeEvent& event) const
    {
        const Extent2D extent = { event.GetWidth(), event.GetHeight() };
        m_Renderer->Resize(extent);
        //ArrangeLayout(extent); // Should use WINDOW extent, not framebuffer

        return true;
    }

    bool Manager::HandleKeyPressed(const KeyPressedEvent& event) const
    {
        if (m_FocusedWidget)
        {
            ProcessKeyPressedRecursive(m_FocusedWidget, event);
        }

        return true;
    }

    bool Manager::HandleKeyTyped(const KeyTypedEvent& event) const
    {
        if (m_FocusedWidget)
        {
            ProcessKeyTypedRecursive(m_FocusedWidget, event);
        }

        return true;
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

        if (m_RootWidget)
        {
            ProcessInputRecursive(m_RootWidget);

            if (m_MouseReleased && m_PressedWidget)
            {
                const auto event = MouseButtonReleasedEvent(EE_MOUSE_BUTTON_LEFT, m_MousePos);
                m_PressedWidget->HandleMouseUp(event);
                m_PressedWidget = nullptr;
            }

            // If user clicked but nothing captured focus, clear it
            if (m_MousePressed && !m_PressedWidget && m_FocusedWidget)
            {
                m_FocusedWidget->HandleLostFocus();
                m_FocusedWidget = nullptr;
            }

            // Mouse move, notify pressed widget (for dragging/selection)
            if (m_MouseMoved && m_PressedWidget)
            {
                const auto event = MouseMovedEvent(m_MousePos);
                m_PressedWidget->HandleMouseMove(event);
            }
        }
    }

    void Manager::ProcessWidget(const Ref<Widget>& widget)
    {
        if (!widget || !widget->IsVisible()) return;

        const auto geometry = widget->GetGeometry();
        const bool isOver = geometry.Contains(m_MousePos);

        // Hover
        if (isOver && !widget->IsHovered())
        {
            widget->HandleMouseEnter();
        }
        else if (!isOver && widget->IsHovered())
        {
            widget->HandleMouseLeave();
        }

        // Press + Focus
        if (isOver && m_MousePressed)
        {
            const auto event = MouseButtonPressedEvent(EE_MOUSE_BUTTON_LEFT, m_MousePos);
            widget->HandleMouseDown(event);
            m_PressedWidget = widget;

            // Focus: only change if clicking a different widget
            if (m_FocusedWidget != widget)
            {
                if (m_FocusedWidget)
                    m_FocusedWidget->HandleLostFocus();

                m_FocusedWidget = widget;
                m_FocusedWidget->HandleFocus();
            }
        }

        // Click
        if (isOver && m_MouseReleased)
        {
            const auto event = MouseButtonReleasedEvent(EE_MOUSE_BUTTON_LEFT, m_MousePos);
            widget->HandleMouseUp(event);

            if (widget->IsPressed() && m_PressedWidget == widget)
                widget->HandleClick();
        }
    }

    void Manager::ProcessInputRecursive(const Ref<Widget>& widget)
    {
        ProcessWidget(widget);

        widget->ForEachChild([this](const Ref<Widget>& child)
        {
            ProcessInputRecursive(child);
        });
    }

    void Manager::ProcessKeyPressedRecursive(
        const Ref<Widget>& widget,
        const KeyPressedEvent& event
    )
    {
        widget->HandleKeyPressed(event);

        widget->ForEachChild([&event](const Ref<Widget>& child)
        {
            ProcessKeyPressedRecursive(child, event);
        });
    }

    void Manager::ProcessKeyTypedRecursive(const Ref<Widget>& widget, const KeyTypedEvent& event)
    {
        widget->HandleKeyTyped(event);

        widget->ForEachChild([&event](const Ref<Widget>& child)
        {
            ProcessKeyTypedRecursive(child, event);
        });
    }
}