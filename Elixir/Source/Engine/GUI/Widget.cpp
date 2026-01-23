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
        return Visibility == EVisibility::Visible && Opacity > 0.0f;
    }

    // void Widget::AddChild(const Ref<Widget>& child)
    // {
    //     m_Children.push_back(child);
    //     child->m_Parent = this;
    // }
}