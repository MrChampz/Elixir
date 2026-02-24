#include "epch.h"
#include "Overlay.h"

namespace Elixir::GUI
{
    LayoutSlot& Overlay::AddChild(const Ref<Widget>& child)
    {
        const auto slot = CreateRef<LayoutSlot>(child);
        m_Slots.push_back(slot);
        return *slot;
    }

    glm::vec2 Overlay::ComputeDesiredSize()
    {
        glm::vec2 totalSize = { 0, 0 };

        for (auto& slot : m_Slots)
        {
            const auto layoutSlot = std::static_pointer_cast<LayoutSlot>(slot);

            auto childSize = slot->GetWidget()->ComputeDesiredSize();
            const auto margin = layoutSlot->GetMargin();

            // Add margin
            childSize.x += margin.GetTotalHorizontal();
            childSize.y += margin.GetTotalVertical();

            // Width and height are the maximum
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
            const auto layoutSlot = std::static_pointer_cast<LayoutSlot>(slot);

            const glm::vec2 childSize = slot->GetWidget()->ComputeDesiredSize();
            const auto margin = layoutSlot->GetMargin();
            const auto hAlignment = layoutSlot->GetHorizontalAlignment();
            const auto vAlignment = layoutSlot->GetVerticalAlignment();

            // Handle fill alignment
            const float childWidth = m_Stretching
                ? innerSpace.Size.x - margin.GetTotalHorizontal()
                : childSize.x;
            const float childHeight = m_Stretching
                ? innerSpace.Size.y - margin.GetTotalVertical()
                : childSize.y;

            // Align within the overlay space
            SRect childGeometry = AlignChild(
                glm::vec2(childWidth, childHeight),
                innerSpace,
                hAlignment,
                vAlignment,
                margin
            );

            // Arrange the child
            slot->GetWidget()->ArrangeChildren(childGeometry);
        }
    }
}