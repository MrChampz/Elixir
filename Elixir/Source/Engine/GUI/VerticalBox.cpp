#include "epch.h"
#include "VerticalBox.h"

namespace Elixir::GUI
{
    glm::vec2 VerticalBox::ComputeDesiredSize(const glm::vec2& availableSize)
    {
        const glm::vec2 innerAvailable = {
            availableSize.x - m_Padding.GetTotalHorizontal(),
            UnconstrainedSize
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
                innerAvailable.x - margin.GetTotalHorizontal(),
                innerAvailable.y
            };

            auto childSize = slot->GetWidget()->Measure(childConstraint);
            childSize.x += margin.GetTotalHorizontal();

            // Width is the maximum
            totalSize.x = std::max(totalSize.x, childSize.x);

            // Height accumulates using the same rule and main-axis constraints as layout.
            // Fill has no measured intrinsic height, but its minimum and margins still
            // reserve space because LayoutChildren enforces that minimum.
            switch (sizeRule.Rule)
            {
                case SSizeParam::ERule::Fill:
                    totalSize.y += minSize.y + margin.GetTotalVertical();
                    break;
                case SSizeParam::ERule::Fixed:
                    totalSize.y += std::max(minSize.y, std::min(maxSize.y, sizeRule.Value)) + margin.GetTotalVertical();
                    break;
                case SSizeParam::ERule::Auto:
                default:
                    totalSize.y += std::max(minSize.y, std::min(maxSize.y, childSize.y)) + margin.GetTotalVertical();
                    break;
            }
        }

        // Add panel padding
        totalSize.x += m_Padding.GetTotalHorizontal();
        totalSize.y += m_Padding.GetTotalVertical();

        return totalSize;
    }

    void VerticalBox::LayoutChildren(const SRect& allocatedSpace)
    {
        // Calculate available space after padding
        const SRect innerSpace = ApplyPadding(allocatedSpace, m_Padding);

        // Measure every child exactly once, with its real constraint, and reuse the result in
        // both loops below. Fill/Fixed children still get measured on the cross axis (width)
        // - their main-axis (height) entry is only actually used below for Auto children.
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
                innerSpace.Size.x - margin.GetTotalHorizontal(),
                UnconstrainedSize
            };

            childSizes[i] = slot->GetWidget()->Measure(childConstraint);
        }

        // First reserve every constrained size. Fill slots start at their minimum so those
        // constraints cannot make the final arrangement overflow after space is divided.
        std::vector<float> childHeights(m_Slots.size());
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
                    childHeights[i] = minSize.y;
                    break;
                case SSizeParam::ERule::Fixed:
                    childHeights[i] = std::max(minSize.y, std::min(maxSize.y, sizeRule.Value));
                    break;
                case SSizeParam::ERule::Auto:
                default:
                    childHeights[i] = std::max(minSize.y, std::min(maxSize.y, childSizes[i].y));
                    break;
            }

            occupiedSpace += childHeights[i] + margin.GetTotalVertical();
        }

        // Divide the remaining space by Fill ratio. A slot that reaches MaxSize is removed
        // from later rounds, so its unused share is redistributed to its siblings.
        float remainingSpace = std::max(0.0f, innerSpace.Size.y - occupiedSpace);
        while (remainingSpace > 0.0f)
        {
            float activeFillRatio = 0.0f;
            for (size_t i = 0; i < m_Slots.size(); ++i)
            {
                const auto& slot = m_Slots[i];
                if (!slot->GetWidget()->TakesSpace()) continue;

                const auto sizeRule = slot->GetSizeRule();
                if (sizeRule.Rule != SSizeParam::ERule::Fill || sizeRule.Value <= 0.0f) continue;
                if (childHeights[i] >= slot->GetMaxSize().y) continue;

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

                const float capacity = slot->GetMaxSize().y - childHeights[i];
                if (capacity <= 0.0f) continue;

                const float share = remainingSpace * (sizeRule.Value / activeFillRatio);
                const float addedSize = std::min(share, capacity);
                childHeights[i] += addedSize;
                distributedSpace += addedSize;
            }

            if (distributedSpace <= 0.0f) break;
            remainingSpace -= distributedSpace;
        }

        // Second: Arrange children
        float currentY = innerSpace.Position.y;

        for (size_t i = 0; i < m_Slots.size(); ++i)
        {
            const auto& slot = m_Slots[i];
            if (!slot->GetWidget()->TakesSpace()) continue;

            const auto margin = slot->GetMargin();
            const auto hAlignment = slot->GetHorizontalAlignment();
            const auto minSize = slot->GetMinSize();
            const auto maxSize = slot->GetMaxSize();

            const float childHeight = childHeights[i];

            // Clamp the desired width; EHorizontalAlignment::Fill overrides it below with the
            // full available width regardless of this value (see Widget::AlignHorizontally).
            const float childWidth = std::max(minSize.x, std::min(maxSize.x, childSizes[i].x));

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
