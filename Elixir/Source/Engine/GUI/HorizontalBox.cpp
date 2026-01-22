#include "epch.h"
#include "HorizontalBox.h"

namespace Elixir::GUI
{
    glm::vec2 HorizontalBox::ComputeDesiredSize()
    {
        glm::vec2 totalSize = { 0, 0 };

        for (auto& slot : m_Slots)
        {
            auto childSize = slot.Content->ComputeDesiredSize();

            // Add margin
            childSize.x += slot.Margin.GetTotalHorizontal();
            childSize.y += slot.Margin.GetTotalVertical();

            // Width accumulates
            totalSize.x += childSize.x;

            // Height is the maximum
            totalSize.y = std::max(totalSize.y, childSize.y);
        }

        // Add panel padding
        totalSize.x += m_Padding.GetTotalHorizontal();
        totalSize.y += m_Padding.GetTotalVertical();

        m_DesiredSize = totalSize;
        return totalSize;
    }

    void HorizontalBox::ArrangeChildren(const SRect& allocatedSpace)
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
            if (slot.HAlignment != EHorizontalAlignment::Stretch)
            {
                const glm::vec2 childSize = slot.Content->ComputeDesiredSize();
                usedSpace += childSize.x + slot.Margin.GetTotalHorizontal();
            }
        }

        // Calculate space available for fill slots
        const float availableForFill = std::max(0.0f, innerSpace.Size.x - usedSpace);

        // Second: Arrange children
        float currentX = innerSpace.Position.x;

        for (auto& slot : m_Slots)
        {
            const glm::vec2 childSize = slot.Content->ComputeDesiredSize();

            // Calculate child width
            float childWidth;

            if (slot.HAlignment == EHorizontalAlignment::Stretch && slot.FillRatio > 0.0f)
            {
                // Proportional fill
                childWidth = availableForFill * slot.FillRatio - slot.Margin.GetTotalHorizontal();
            }
            else
            {
                // Use the desired width
                childWidth = childSize.x;
            }

            // Clamp to min/max constraints
            childWidth = std::max(slot.MinSize.x, std::min(slot.MaxSize.x, childWidth));

            // Calculate child height based on alignment
            float childHeight;

            if (slot.VAlignment == EVerticalAlignment::Stretch)
            {
                childHeight = innerSpace.Size.y - slot.Margin.GetTotalVertical();
            }
            else
            {
                childHeight = childSize.y;
            }

            childHeight = std::max(slot.MinSize.y, std::min(slot.MaxSize.y, childHeight));

            // Create available space for this child
            SRect childAvailableSpace;
            childAvailableSpace.Position.x = currentX;
            childAvailableSpace.Position.y = innerSpace.Position.y;
            childAvailableSpace.Size.x = childWidth + slot.Margin.GetTotalHorizontal();
            childAvailableSpace.Size.y = innerSpace.Size.y;

            // Align child within available space
            SRect childGeometry = AlignChild(
                glm::vec2(childWidth, childHeight),
                childAvailableSpace,
                EHorizontalAlignment::Left,
                slot.VAlignment,
                slot.Margin
            );

            // Arrange the child
            slot.Content->ArrangeChildren(childGeometry);

            // Advance position
            currentX += childWidth + slot.Margin.GetTotalHorizontal();
        }
    }
}