#include "epch.h"
#include "HorizontalBox.h"

namespace Elixir::GUI
{
    LayoutSlot& HorizontalBox::AddChild(const Ref<Widget>& child)
    {
        const auto slot = CreateRef<LayoutSlot>(child);
        m_Slots.push_back(slot);
        return *slot;
    }

    glm::vec2 HorizontalBox::ComputeDesiredSize()
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
            const auto layoutSlot = std::static_pointer_cast<LayoutSlot>(slot);

            const auto margin = layoutSlot->GetMargin();
            const auto hAlignment = layoutSlot->GetHorizontalAlignment();

            if (m_Stretching)
            {
                const glm::vec2 childSize = slot->GetWidget()->ComputeDesiredSize();
                usedSpace += childSize.x + margin.GetTotalHorizontal();
            }
        }

        // Calculate space available for fill slots
        const float availableForFill = std::max(0.0f, innerSpace.Size.x - usedSpace);

        // Second: Arrange children
        float currentX = innerSpace.Position.x;

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

            // Calculate child width
            float childWidth;

            if (m_Stretching && fillRatio > 0.0f)
            {
                // Proportional fill
                childWidth = availableForFill * fillRatio - margin.GetTotalHorizontal();
            }
            else
            {
                // Use the desired width
                childWidth = childSize.x;
            }

            // Clamp to min/max constraints
            childWidth = std::max(minSize.x, std::min(maxSize.x, childWidth));

            // Calculate child height based on alignment
            float childHeight;

            if (m_Stretching)
            {
                childHeight = innerSpace.Size.y - margin.GetTotalVertical();
            }
            else
            {
                childHeight = childSize.y;
            }

            childHeight = std::max(minSize.y, std::min(maxSize.y, childHeight));

            // Create available space for this child
            SRect childAvailableSpace;
            childAvailableSpace.Position.x = currentX;
            childAvailableSpace.Position.y = innerSpace.Position.y;
            childAvailableSpace.Size.x = childWidth + margin.GetTotalHorizontal();
            childAvailableSpace.Size.y = innerSpace.Size.y;

            // Align child within available space
            SRect childGeometry = AlignChild(
                glm::vec2(childWidth, childHeight),
                childAvailableSpace,
                EHorizontalAlignment::Left,
                vAlignment,
                margin
            );

            // Arrange the child
            slot->GetWidget()->ArrangeChildren(childGeometry);

            // Advance position
            currentX += childWidth + margin.GetTotalHorizontal();
        }
    }
}