#include "epch.h"
#include "Overlay.h"

#include "Types.h"

namespace Elixir::GUI
{
    glm::vec2 Overlay::ComputeDesiredSize()
    {
        glm::vec2 totalSize = { 0, 0 };

        for (auto& slot : m_Slots)
        {
            auto childSize = slot.Content->ComputeDesiredSize();

            // Add margin
            childSize.x += slot.Margin.GetTotalHorizontal();
            childSize.y += slot.Margin.GetTotalVertical();

            // Width and height     are the maximum
            totalSize.x = std::max(totalSize.x, childSize.x);
            totalSize.y = std::max(totalSize.y, childSize.y);
        }

        // Add panel padding
        totalSize.x += m_Padding.GetTotalHorizontal();
        totalSize.y += m_Padding.GetTotalVertical();

        m_DesiredSize = totalSize;
        return totalSize;
    }

    void Overlay::ArrangeChildren(const SRect& allocatedSpace)
    {
        m_Geometry = allocatedSpace;

        // Calculate available space after padding
        SRect innerSpace;
        innerSpace.Position.x = allocatedSpace.Position.x + m_Padding.Left;
        innerSpace.Position.y = allocatedSpace.Position.y + m_Padding.Top;
        innerSpace.Size.x = allocatedSpace.Size.x - m_Padding.GetTotalHorizontal();
        innerSpace.Size.y = allocatedSpace.Size.y - m_Padding.GetTotalVertical();

        for (auto& slot : m_Slots)
        {
            const glm::vec2 childSize = slot.Content->ComputeDesiredSize();

            // Handle fill alignment
            float childWidth = (slot.HAlignment == EHorizontalAlignment::Stretch)
                ? innerSpace.Size.x - slot.Margin.GetTotalHorizontal() : childSize.x;
            float childHeight = (slot.VAlignment == EVerticalAlignment::Stretch)
                ? innerSpace.Size.y - slot.Margin.GetTotalVertical() : childSize.y;

            // Align within the overlay space
            SRect childGeometry = AlignChild(
                glm::vec2(childWidth, childHeight),
                innerSpace,
                slot.HAlignment,
                slot.VAlignment,
                slot.Margin
            );

            // Arrange the child
            slot.Content->ArrangeChildren(childGeometry);
        }
    }
}