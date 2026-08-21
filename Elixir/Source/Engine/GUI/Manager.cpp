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
        m_LastExtent = extent;

        if (m_Layers.empty() || !m_Layers[0].Root)
            return;

        const SRect screenRect = { { 0, 0 }, { extent.Width, extent.Height } };
        m_Layers[0].Root->ArrangeChildren(screenRect);

        for (size_t i = 1; i < m_Layers.size(); ++i)
        {
            const auto& layer = m_Layers[i];
            if (!layer.Root) continue;

            const glm::vec2 desiredSize = layer.Root->Measure({ UnconstrainedSize, UnconstrainedSize });
            const SRect popupRect = ComputePopupRect(layer.Anchor, desiredSize, screenRect);
            layer.Root->ArrangeChildren(popupRect);
        }
    }

    void Manager::Update(const Timestep frameTime)
    {
        ProcessInput();

        for (const auto& layer : m_Layers)
        {
            if (layer.Root)
                layer.Root->Update(frameTime);
        }
    }

    void Manager::Render()
    {
        if (m_Layers.empty() || !m_Layers[0].Root || !m_Layers[0].Root->IsRenderVisible())
            return;

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
        dispatcher.Dispatch<MouseScrolledEvent>(EE_BIND_EVENT_FN(Manager::HandleMouseScrolled));
    }

    void Manager::SetRoot(const Ref<Panel>& root)
    {
        if (m_Layers.empty())
            m_Layers.push_back({ root, {}, false });
        else
            m_Layers[0] = { root, {}, false };

        ++m_LayerStackVersion;
    }

    void Manager::PushPopup(const Ref<Widget>& widget, const SRect& anchor)
    {
        m_Layers.push_back({ widget, anchor, true });
        ++m_LayerStackVersion;

        if (widget)
        {
            const SRect screenRect = { { 0, 0 }, { m_LastExtent.Width, m_LastExtent.Height } };
            const glm::vec2 desiredSize = widget->Measure({ UnconstrainedSize, UnconstrainedSize });
            const SRect popupRect = ComputePopupRect(anchor, desiredSize, screenRect);
            widget->ArrangeChildren(popupRect);
        }
    }

    void Manager::PopPopup()
    {
        if (m_Layers.size() <= 1) return;

        m_Layers.pop_back();
        ++m_LayerStackVersion;
    }

    void Manager::ClearPopups()
    {
        if (m_Layers.size() <= 1) return;

        m_Layers.resize(1);
        ++m_LayerStackVersion;
    }

    bool Manager::WantsMouse() const
    {
        return !m_HoverPath.empty() || !m_MouseCapture.expired();
    }

    void Manager::AssembleFrame()
    {
        m_RenderBatch.Clear();

        int zCursor = 0;
        bool rebuilt = false;

        // Same zCursor continuing across layers: layer 0 occupies the low z-bands, and each
        // popup above it starts its own CollectDrawCommands walk above everything the layers
        // below it used — reusing the existing per-subtree z-banding, so popups always end
        // up on top without a magic z offset.
        for (const auto& layer : m_Layers)
        {
            if (layer.Root && layer.Root->IsRenderVisible())
                layer.Root->CollectDrawCommands(
                    m_RenderBatch,
                    zCursor,
                    rebuilt,
                    {{ -1, -1 }, { -1, -1 }}
                );
        }

        // No default focus visual here on purpose: a widget's own IsFocused() is already
        // enough for it to render its own focus state - color swap, outline, background
        // texture, whatever fits - inside its own BuildDrawCommands, the same way Button
        // already reacts to m_Hovered. A widget that doesn't opt in just has no focus visual,
        // rather than the Manager imposing a generic ring on every widget regardless of type.
        m_RenderBatch.Sort();
    }

    bool Manager::NeedsRebuild() const
    {
        return Widget::CurrentDirtyEpoch() != m_LastRenderedEpoch
            || m_LayerStackVersion != m_LastRenderedLayerVersion;
    }

    void Manager::MarkRebuilt()
    {
        m_LastRenderedEpoch = Widget::CurrentDirtyEpoch();
        m_LastRenderedLayerVersion = m_LayerStackVersion;
    }

    bool Manager::HandleFramebufferResize(const FramebufferResizeEvent& event) const
    {
        const Extent2D extent = { event.GetWidth(), event.GetHeight() };
        m_Renderer->Resize(extent);

        return true;
    }

    bool Manager::HandleKeyPressed(const KeyPressedEvent& event)
    {
        if (event.GetKeyCode() == EE_KEY_TAB)
        {
            if (event.IsShiftPressed())
                FocusPrevious();
            else
                FocusNext();

            return true;
        }

        if (event.GetKeyCode() == EE_KEY_ESCAPE)
        {
            SetFocusedWidget(nullptr);
            return true;
        }

        // SetEnabled(false) has no way to clear m_FocusedWidget itself - Widget has no
        // back-reference to the Manager - so a widget disabled while focused stays focused,
        // just Disabled-styled. Its own contract ("stops accepting input events") still has
        // to hold for the keyboard, not only for HandleMouseDown/HandleClick, or a disabled
        // TextField that was focused before being disabled would keep taking keystrokes.
        if (m_FocusedWidget && !m_FocusedWidget->IsEnabled())
            return false;

        for (auto widget = m_FocusedWidget; widget; widget = widget->GetParent())
        {
            if (widget->HandleKeyPressed(event).EventHandled)
                return true;
        }

        return false;
    }

    bool Manager::HandleKeyTyped(const KeyTypedEvent& event) const
    {
        // See the matching guard in HandleKeyPressed for why this checks IsEnabled() at all.
        if (m_FocusedWidget && !m_FocusedWidget->IsEnabled())
            return false;

        for (auto widget = m_FocusedWidget; widget; widget = widget->GetParent())
        {
            if (widget->HandleKeyTyped(event).EventHandled)
                return true;
        }

        return false;
    }

    bool Manager::HandleMouseScrolled(const MouseScrolledEvent& event) const
    {
        for (auto it = m_HoverPath.rbegin(); it != m_HoverPath.rend(); ++it)
        {
            if ((*it)->HandleMouseScrolled(event).EventHandled)
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

        if (m_Layers.empty()) return;

        if (m_MousePressed)
            DismissPopupsOutside(m_MousePos);

        const Ref<Widget>& activeRoot = GetTopmostHitLayer(m_MousePos).Root;
        if (!activeRoot) return;

        std::vector<Ref<Widget>> hitPath;
        activeRoot->HitTest(m_MousePos, hitPath);

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

    void Manager::CollectFocusOrder(
        const Ref<Widget>& widget,
        std::vector<Ref<Widget>>& out
    )
    {
        if (!widget) return;

        const auto visibility = widget->GetVisibility();
        if (visibility == EVisibility::HitTestInvisible ||
            visibility == EVisibility::Hidden ||
            visibility == EVisibility::Collapsed)
            return;

        // Excludes a SelfHitTestVisible widget from the order itself while still walking
        // into its children - same treatment for a disabled one: Tab must not be able to
        // land somewhere a mouse click already can't (Widget::HandleMouseDown's own
        // IsEnabled() check), even though a widget already focused before being disabled
        // stays in m_FocusedWidget (see the guard in Manager::HandleKeyPressed).
        if (widget->IsFocusable() && widget->IsSelfHitTestVisible() && widget->IsEnabled())
            out.push_back(widget);

        widget->ForEachChild([&](const Ref<Widget>& child)
        {
            CollectFocusOrder(child, out);
        });
    }

    const std::vector<Ref<Widget>>& Manager::GetFocusOrder()
    {
        const uint64_t epoch = Widget::CurrentDirtyEpoch();
        if (epoch == m_FocusOrderEpoch && m_LayerStackVersion == m_FocusOrderLayerVersion)
            return m_FocusOrder;

        m_FocusOrder.clear();

        if (!m_Layers.empty())
            CollectFocusOrder(m_Layers.back().Root, m_FocusOrder);

        m_FocusOrderEpoch = epoch;
        m_FocusOrderLayerVersion = m_LayerStackVersion;

        return m_FocusOrder;
    }

    void Manager::FocusNext()
    {
        const auto& order = GetFocusOrder();

        // Nothing to Tab to: leave m_FocusedWidget exactly as it is.
        if (order.empty()) return;

        const auto it = std::ranges::find(order, m_FocusedWidget);
        const size_t nextIndex = (it == order.end())
            ? 0
            : (size_t(it - order.begin()) + 1) % order.size();

        SetFocusedWidget(order[nextIndex]);
    }

    void Manager::FocusPrevious()
    {
        const auto& order = GetFocusOrder();

        // Nothing to back focus to: leave m_FocusedWidget exactly as it is.
        if (order.empty()) return;

        const auto it = std::ranges::find(order, m_FocusedWidget);
        const size_t currIndex = (it == order.end()) ? 0 : size_t(it - order.begin());
        const size_t prevIndex = (currIndex == 0) ? order.size() - 1 : currIndex - 1;

        SetFocusedWidget(order[prevIndex]);
    }

    const SLayer& Manager::GetTopmostHitLayer(const glm::vec2& point) const
    {
        for (size_t i = m_Layers.size(); i-- > 1;)
        {
            if (m_Layers[i].Root && m_Layers[i].Root->GetGeometry().Contains(point))
                return m_Layers[i];
        }

        return m_Layers[0]; // the UI root always hits
    }

    void Manager::DismissPopupsOutside(const glm::vec2& point)
    {
        while (m_Layers.size() > 1)
        {
            const auto& top = m_Layers.back();
            if (!top.DismissOnClickOutside) break;
            if (top.Root && top.Root->GetGeometry().Contains(point)) break;

            PopPopup();
        }
    }

    SRect Manager::ComputePopupRect(
        const SRect& anchor,
        const glm::vec2& desiredSize,
        const SRect& screenRect
    )
    {
        // Default: flush against the anchor's left edge, opening below it.
        glm::vec2 position = { anchor.Position.x, anchor.Position.y + anchor.Size.y };

        // Doesn't fit below -> flip above the anchor.
        if (position.y + desiredSize.y > screenRect.Position.y + screenRect.Size.y)
            position.y = anchor.Position.y - desiredSize.y;

        // Clamp fully on-screen as a last resort (flipping alone doesn't help when the
        // screen itself is smaller than the popup, or the anchor is near the top with
        // nothing to flip into).
        const glm::vec2 maxPosition = glm::max(
            screenRect.Position,
            screenRect.Position + screenRect.Size - desiredSize
        );

        position = glm::clamp(position, screenRect.Position, maxPosition);

        return { position, desiredSize };
    }
}
