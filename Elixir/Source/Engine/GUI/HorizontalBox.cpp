#include "epch.h"
#include "HorizontalBox.h"

namespace Elixir::GUI
{
    glm::vec2 HorizontalBox::ComputeDesiredSize(const glm::vec2& availableSize)
    {
        const glm::vec2 innerAvailable = {
            UnconstrainedSize,
            availableSize.y - m_Padding.GetTotalVertical()
        };

        glm::vec2 totalSize = { 0, 0 };

        for (const auto& slot : m_Slots)
        {
            if (!slot->GetWidget()->TakesSpace()) continue;

            const auto margin = slot->GetMargin();
            const auto sizeRule = slot->GetSizeRule();
            const auto minSize = slot->GetMinSize();
            const auto maxSize = slot->GetMaxSize();

            const glm::vec2 childConstraint = {
                innerAvailable.x,
                innerAvailable.y - margin.GetTotalVertical()
            };

            auto childSize = slot->GetWidget()->Measure(childConstraint);
            childSize.y += margin.GetTotalVertical();

            // Height is the maximum
            totalSize.y = std::max(totalSize.y, childSize.y);

            // Width accumulates using the same rule and main-axis constraints as layout.
            // Fill has no measured intrinsic width, but its minimum and margins still
            // reserve space because LayoutChildren enforces that minimum.
            switch (sizeRule.Rule)
            {
                case SSizeParam::ERule::Fill:
                    totalSize.x += minSize.x + margin.GetTotalHorizontal();
                    break;
                case SSizeParam::ERule::Fixed:
                    totalSize.x += std::max(minSize.x, std::min(maxSize.x, sizeRule.Value)) + margin.GetTotalHorizontal();
                    break;
                case SSizeParam::ERule::Auto:
                default:
                    totalSize.x += std::max(minSize.x, std::min(maxSize.x, childSize.x)) + margin.GetTotalHorizontal();
                    break;
            }
        }

        // Add panel padding
        totalSize.x += m_Padding.GetTotalHorizontal();
        totalSize.y += m_Padding.GetTotalVertical();

        return totalSize;
    }

    void HorizontalBox::LayoutChildren(const SRect& allocatedSpace)
    {
        // Calculate available space after padding
        const SRect innerSpace = ApplyPadding(allocatedSpace, m_Padding);

        // Measure every child exactly once, with its real constraint, and reuse the result in
        // both loops below. Fill/Fixed children still get measured on the cross axis (height)
        // - their main-axis (width) entry is only actually used below for Auto children.
        // Keep measurements indexed by slot. Collapsed children are skipped by both later
        // passes, but they still need an unused entry so a preceding collapsed slot cannot
        // shift the measurement belonging to a visible sibling.
        std::vector<glm::vec2> childSizes(m_Slots.size());

        for (size_t i = 0; i < m_Slots.size(); ++i)
        {
            const auto& slot = m_Slots[i];
            if (!slot->GetWidget()->TakesSpace()) continue;

            const auto margin = slot->GetMargin();

            const glm::vec2 childConstraint = {
                UnconstrainedSize,
                innerSpace.Size.y - margin.GetTotalVertical()
            };

            childSizes[i] = slot->GetWidget()->Measure(childConstraint);
        }

        // First reserve every constrained size. Fill slots start at their minimum so those
        // constraints cannot make the final arrangement overflow after space is divided.
        std::vector<float> childWidths(m_Slots.size());
        float occupiedSpace = 0.0f;

        for (size_t i = 0; i < m_Slots.size(); ++i)
        {
            const auto& slot = m_Slots[i];
            if (!slot->GetWidget()->TakesSpace()) continue;

            const auto margin = slot->GetMargin();
            const auto sizeRule = slot->GetSizeRule();
            const auto minSize = slot->GetMinSize();
            const auto maxSize = slot->GetMaxSize();

            switch (sizeRule.Rule)
            {
                case SSizeParam::ERule::Fill:
                    childWidths[i] = minSize.x;
                    break;
                case SSizeParam::ERule::Fixed:
                    childWidths[i] = std::max(minSize.x, std::min(maxSize.x, sizeRule.Value));
                    break;
                case SSizeParam::ERule::Auto:
                default:
                    childWidths[i] = std::max(minSize.x, std::min(maxSize.x, childSizes[i].x));
                    break;
            }

            occupiedSpace += childWidths[i] + margin.GetTotalHorizontal();
        }

        // Divide the remaining space by Fill ratio. A slot that reaches MaxSize is removed
        // from later rounds, so its unused share is redistributed to its siblings.
        float remainingSpace = std::max(0.0f, innerSpace.Size.x - occupiedSpace);
        while (remainingSpace > 0.0f)
        {
            float activeFillRatio = 0.0f;
            for (size_t i = 0; i < m_Slots.size(); ++i)
            {
                const auto& slot = m_Slots[i];
                if (!slot->GetWidget()->TakesSpace()) continue;

                const auto sizeRule = slot->GetSizeRule();
                if (sizeRule.Rule != SSizeParam::ERule::Fill || sizeRule.Value <= 0.0f) continue;
                if (childWidths[i] >= slot->GetMaxSize().x) continue;

                activeFillRatio += sizeRule.Value;
            }

            if (activeFillRatio <= 0.0f) break;

            float distributedSpace = 0.0f;
            for (size_t i = 0; i < m_Slots.size(); ++i)
            {
                const auto& slot = m_Slots[i];
                if (!slot->GetWidget()->TakesSpace()) continue;

                const auto sizeRule = slot->GetSizeRule();
                if (sizeRule.Rule != SSizeParam::ERule::Fill || sizeRule.Value <= 0.0f) continue;

                const float capacity = slot->GetMaxSize().x - childWidths[i];
                if (capacity <= 0.0f) continue;

                const float share = remainingSpace * (sizeRule.Value / activeFillRatio);
                const float addedSize = std::min(share, capacity);
                childWidths[i] += addedSize;
                distributedSpace += addedSize;
            }

            if (distributedSpace <= 0.0f) break;
            remainingSpace -= distributedSpace;
        }

        // Second: Arrange children
        float currentX = innerSpace.Position.x;

        for (size_t i = 0; i < m_Slots.size(); ++i)
        {
            const auto& slot = m_Slots[i];
            if (!slot->GetWidget()->TakesSpace()) continue;

            const auto margin = slot->GetMargin();
            const auto vAlignment = slot->GetVerticalAlignment();
            const auto minSize = slot->GetMinSize();
            const auto maxSize = slot->GetMaxSize();

            const float childWidth = childWidths[i];

            // Clamp the desired height; EVerticalAlignment::Fill overrides it below with the
            // full available height regardless of this value (see Widget::AlignVertically).
            const float childHeight = std::max(minSize.y, std::min(maxSize.y, childSizes[i].y));

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
