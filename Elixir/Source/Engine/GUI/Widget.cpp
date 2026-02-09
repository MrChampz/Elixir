#include "epch.h"
#include "Widget.h"

namespace Elixir::GUI
{
    void Widget::Tick(const Timestep frameTime)
    {
        // for (const auto& child : m_Children)
        // {
        //     if (child->IsVisible())
        //     {
        //         child->Tick(frameTime);
        //     }
        // }
    }

    bool Widget::IsVisible() const
    {
        return m_Visibility == EVisibility::Visible && m_Opacity > 0.0f;
    }

    void Widget::InternalOnMouseEnter()
    {
        m_IsHovered = true;
        if (m_OnMouseEnterCallback) m_OnMouseEnterCallback();
    }

    void Widget::InternalOnMouseLeave()
    {
        m_IsHovered = false;
        if (m_OnMouseLeaveCallback) m_OnMouseLeaveCallback();
    }

    void Widget::InternalOnMouseDown()
    {
        m_IsPressed = true;
        if (m_OnMouseDownCallback) m_OnMouseDownCallback();
    }

    void Widget::InternalOnMouseUp()
    {
        if (m_IsPressed)
        {
            if (m_OnMouseUpCallback)
                m_OnMouseUpCallback();

            if (m_IsHovered)
                InternalOnClick();
        }

        m_IsPressed = false;
    }

    void Widget::InternalOnClick()
    {
        if (m_OnClickCallback) m_OnClickCallback();
    }

    // void Widget::AddChild(const Ref<Widget>& child)
    // {
    //     m_Children.push_back(child);
    //     child->m_Parent = this;
    // }
}