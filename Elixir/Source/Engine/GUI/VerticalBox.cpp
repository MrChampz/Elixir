#include "epch.h"
#include "VerticalBox.h"

namespace Elixir::GUI
{
    LayoutSlot& VerticalBox::AddChild(const Ref<Widget>& child)
    {
        const auto slot = CreateRef<LayoutSlot>(child);
        m_Slots.push_back(slot);
        return *slot;
    }

    glm::vec2 VerticalBox::ComputeDesiredSize()
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

            // Width is the maximum
            totalSize.x = std::max(totalSize.x, childSize.x);

            // Height accumulates
            totalSize.y += childSize.y;
        }

        // Add panel padding
        totalSize.x += m_Padding.GetTotalHorizontal();
        totalSize.y += m_Padding.GetTotalVertical();

        m_DesiredSize = totalSize;
        return totalSize;
    }

    void VerticalBox::ArrangeChildren(const SRect& allocatedSpace)
    {
        m_Geometry = allocatedSpace;

        // Calculate available space after padding
        SRect innerSpace;
        innerSpace.Position.x = allocatedSpace.Position.x + m_Padding.Left;
        innerSpace.Position.y = allocatedSpace.Position.y + m_Padding.Top;
        innerSpace.Size.x = allocatedSpace.Size.x - m_Padding.GetTotalHorizontal();
        innerSpace.Size.y = allocatedSpace.Size.y - m_Padding.GetTotalVertical();

        // First: calculate fixed sizes
        float usedSpace = 0.0f;

        for (auto& slot : m_Slots)
        {
            const auto layoutSlot = std::static_pointer_cast<LayoutSlot>(slot);

            const auto margin = layoutSlot->GetMargin();
            const auto vAlignment = layoutSlot->GetVerticalAlignment();

            if (m_Stretching)
            {
                const glm::vec2 childSize = slot->GetWidget()->ComputeDesiredSize();
                usedSpace += childSize.y + margin.GetTotalVertical();
            }
        }

        // Calculate space available for fill slots
        const float availableForFill = std::max(0.0f, innerSpace.Size.y - usedSpace);

        // Second: Arrange children
        float currentY = innerSpace.Position.y;

        for (auto& slot : m_Slots)
        {
            const auto layoutSlot = std::static_pointer_cast<LayoutSlot>(slot);

            const glm::vec2 childSize = slot->GetWidget()->ComputeDesiredSize();
            const auto margin = layoutSlot->GetMargin();
            const auto hAlignment = layoutSlot->GetHorizontalAlignment();
            const auto vAlignment = layoutSlot->GetVerticalAlignment();
            const auto fillRatio = layoutSlot->GetFillRatio();
            const auto minSize = layoutSlot->GetMinSize();
            const auto maxSize = layoutSlot->GetMaxSize();

            // Calculate child height
            float childHeight;

            if (m_Stretching && fillRatio > 0.0f)
            {
                // Proportional fill
                childHeight = availableForFill * fillRatio - margin.GetTotalVertical();
            }
            else
            {
                // Use the desired height
                childHeight = childSize.y;
            }

            // Clamp to min/max constraints
            childHeight = std::max(minSize.y, std::min(maxSize.y, childHeight));

            // Calculate child width based on alignment
            float childWidth;

            if (m_Stretching)
            {
                childWidth = innerSpace.Size.x - margin.GetTotalHorizontal();
            }
            else
            {
                childWidth = childSize.x;
            }

            childWidth = std::max(minSize.x, std::min(maxSize.x, childWidth));

            // Create available space for this child
            SRect childAvailableSpace;
            childAvailableSpace.Position.x = innerSpace.Position.x;
            childAvailableSpace.Position.y = currentY;
            childAvailableSpace.Size.x = innerSpace.Size.x;
            childAvailableSpace.Size.y = childHeight + margin.GetTotalVertical();

            // Align child within available space
            SRect childGeometry = AlignChild(
                glm::vec2(childWidth, childHeight),
                childAvailableSpace,
                hAlignment,
                EVerticalAlignment::Top,
                margin
            );

            // Arrange the child
            slot->GetWidget()->ArrangeChildren(childGeometry);

            // Advance position
            currentY += childHeight + margin.GetTotalVertical();
        }
    }
}