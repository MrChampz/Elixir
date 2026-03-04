#include "epch.h"
#include "Widget.h"

#include <Engine/GUI/Slot.h>

namespace Elixir::GUI
{
    /* Widget */

    bool Widget::IsVisible() const
    {
        return m_Visibility == EVisibility::Visible && m_Opacity > 0.0f;
    }



    void Widget::HandleMouseEnter()
    {
        m_Hovered = true;
        if (m_OnMouseEnterCallback) m_OnMouseEnterCallback();
    }

    void Widget::HandleMouseLeave()
    {
        m_Hovered = false;
        if (m_OnMouseLeaveCallback) m_OnMouseLeaveCallback();
    }

    void Widget::HandleMouseDown(const MouseButtonPressedEvent& event)
    {
        m_Pressed = true;
        if (m_OnMouseDownCallback) m_OnMouseDownCallback();
    }

    void Widget::HandleMouseUp(const MouseButtonReleasedEvent& event)
    {
        if (m_Pressed)
        {
            if (m_OnMouseUpCallback)
                m_OnMouseUpCallback();

            if (m_Hovered)
                HandleClick();
        }

        m_Pressed = false;
    }

    void Widget::HandleFocus()
    {
        m_Focused = true;
        if (m_OnFocusCallback) m_OnFocusCallback();
    }

    void Widget::HandleLostFocus()
    {
        m_Focused = false;
        if (m_OnLostFocusCallback) m_OnLostFocusCallback();
    }

    void Widget::HandleClick()
    {
        if (m_OnClickCallback) m_OnClickCallback();
    }

    SRect Widget::ApplyPadding(const SRect& availableSpace, const SPadding& padding)
    {
        SRect result;
        result.Position.x = availableSpace.Position.x + padding.Left;
        result.Position.y = availableSpace.Position.y + padding.Top;
        result.Size.x = availableSpace.Size.x - padding.GetTotalHorizontal();
        result.Size.y = availableSpace.Size.y - padding.GetTotalVertical();

        return result;
    }

    SRect Widget::ApplyMargin(const SRect& availableSpace, const SMargin& margin)
    {
        SRect result;
        result.Position.x = availableSpace.Position.x + margin.Left;
        result.Position.y = availableSpace.Position.y + margin.Top;
        result.Size.x = availableSpace.Size.x - margin.GetTotalHorizontal();
        result.Size.y = availableSpace.Size.y - margin.GetTotalVertical();

        return result;
    }

    SRect Widget::AlignChild(
        const glm::vec2& childSize,
        const SRect& availableSpace,
        const EHorizontalAlignment hAlignment,
        const EVerticalAlignment vAlignment,
        const SMargin& margin
    )
    {
        SRect result = ApplyMargin(availableSpace, margin);
        result = AlignHorizontally(childSize, result, hAlignment);
        result = AlignVertically(childSize, result, vAlignment);

        return result;
    }

    SRect Widget::AlignHorizontally(
        const glm::vec2& childSize,
        const SRect& availableSpace,
        const EHorizontalAlignment alignment
    )
    {
        SRect result = availableSpace;

        switch (alignment)
        {
            case EHorizontalAlignment::Left:
                result.Position.x = availableSpace.Position.x;
                result.Size.x = childSize.x;
                break;
            case EHorizontalAlignment::Center:
                result.Position.x = availableSpace.Position.x + (availableSpace.Size.x - childSize.x) * 0.5f;
                result.Size.x = childSize.x;
                break;
            case EHorizontalAlignment::Right:
                result.Position.x = availableSpace.Position.x + availableSpace.Size.x - childSize.x;
                result.Size.x = childSize.x;
                break;
        }

        return result;
    }

    SRect Widget::AlignVertically(
        const glm::vec2& childSize,
        const SRect& availableSpace,
        const EVerticalAlignment alignment
    )
    {
        SRect result = availableSpace;

        switch (alignment)
        {
            case EVerticalAlignment::Top:
                result.Position.y = availableSpace.Position.y;
                result.Size.y = childSize.y;
                break;
            case EVerticalAlignment::Center:
                result.Position.y = availableSpace.Position.y + (availableSpace.Size.y - childSize.y) * 0.5f;
                result.Size.y = childSize.y;
                break;
            case EVerticalAlignment::Bottom:
                result.Position.y = availableSpace.Position.y + availableSpace.Size.y - childSize.y;
                result.Size.y = childSize.y;
                break;
        }

        return result;
    }

    /* ContentWidget */

    void ContentWidget::Update(const Timestep frameTime)
    {
        if (m_ContentSlot && m_ContentSlot->IsVisible())
        {
            m_ContentSlot->GetWidget()->Update(frameTime);
        }
    }
}