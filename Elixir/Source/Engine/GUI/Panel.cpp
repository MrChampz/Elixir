#include "epch.h"
#include "Panel.h"

namespace Elixir::GUI
{
    void Panel::GenerateDrawCommands(RenderBatch& batch, const int zOrder)
    {
        for (const auto& child : m_Children)
        {
            if (child->IsVisible())
            {
                child->GenerateDrawCommands(batch, zOrder + 1);
            }
        }
    }

    glm::vec2 Panel::ComputeDesiredSize()
    {
        if (m_Children.empty()) return { 0, 0 };

        glm::vec2 size = { 0, 0 };

        switch (m_LayoutMode)
        {
            case ELayoutMode::Horizontal:
                for (const auto& child : m_Children)
                {
                    auto childSize = child->ComputeDesiredSize();
                    size.x += childSize.x + m_Padding;
                    size.y = std::max(size.y, childSize.y);
                }
                size.x -= m_Padding; // Remove last padding
                break;

            case ELayoutMode::Vertical:
                for (const auto& child : m_Children)
                {
                    auto childSize = child->ComputeDesiredSize();
                    size.x = std::max(size.x, childSize.x);
                    size.y += childSize.y + m_Padding;
                }
                size.y -= m_Padding; // Remove last padding
                break;

            case ELayoutMode::Overlay:
                for (const auto& child : m_Children)
                {
                    auto childSize = child->ComputeDesiredSize();
                    size.x = std::max(size.x, childSize.x);
                    size.y = std::max(size.y, childSize.y);
                }
                break;
        }

        return size;
    }

    void Panel::ArrangeChildren() const
    {
        auto currentPos = m_Geometry.Position;

        for (const auto& child : m_Children)
        {
            if (!child->IsVisible()) continue;

            const auto childSize = child->ComputeDesiredSize();
            SRect childGeometry = { currentPos, childSize };

            switch (m_LayoutMode)
            {
                case ELayoutMode::Horizontal:
                    currentPos.x += childSize.x + m_Padding;
                    break;

                case ELayoutMode::Vertical:
                    currentPos.y += childSize.y + m_Padding;
                    break;

                case ELayoutMode::Overlay:
                    break;
            }

            child->SetGeometry(childGeometry);

            // Recursively arrange if the child is also a panel
            if (const auto panel = std::dynamic_pointer_cast<Panel>(child))
            {
                panel->ArrangeChildren();
            }
        }
    }
}