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

        // First pass: space already spoken for by Auto/Fixed children (main axis = width),
        // and the total ratio claimed by Fill children.
        float fixedSpace = 0.0f;
        float totalFillRatio = 0.0f;

        for (size_t i = 0; i < m_Slots.size(); ++i)
        {
            const auto& slot = m_Slots[i];
            if (!slot->GetWidget()->TakesSpace()) continue;

            const auto margin = slot->GetMargin();
            const auto sizeRule = slot->GetSizeRule();

            switch (sizeRule.Rule)
            {
                case SSizeParam::ERule::Fill:
                    totalFillRatio += sizeRule.Value;
                    break;
                case SSizeParam::ERule::Fixed:
                    fixedSpace += sizeRule.Value + margin.GetTotalHorizontal();
                    break;
                case SSizeParam::ERule::Auto:
                default:
                    fixedSpace += childSizes[i].x + margin.GetTotalHorizontal();
                    break;
            }
        }

        // Calculate space available for Fill slots
        const float fillSpace = std::max(0.0f, innerSpace.Size.x - fixedSpace);

        // Second: Arrange children
        float currentX = innerSpace.Position.x;

        for (size_t i = 0; i < m_Slots.size(); ++i)
        {
            const auto& slot = m_Slots[i];
            if (!slot->GetWidget()->TakesSpace()) continue;

            const glm::vec2 childSize = childSizes[i];
            const auto margin = slot->GetMargin();
            const auto vAlignment = slot->GetVerticalAlignment();
            const auto sizeRule = slot->GetSizeRule();
            const auto minSize = slot->GetMinSize();
            const auto maxSize = slot->GetMaxSize();

            // Calculate child width from its sizing rule
            float childWidth;

            switch (sizeRule.Rule)
            {
                case SSizeParam::ERule::Fill:
                    // Guard: if no sibling claims a Fill ratio, no extra space is handed out.
                    childWidth = totalFillRatio > 0.0f
                        ? fillSpace * (sizeRule.Value / totalFillRatio) - margin.GetTotalHorizontal()
                        : 0.0f;
                    break;
                case SSizeParam::ERule::Fixed:
                    childWidth = sizeRule.Value;
                    break;
                case SSizeParam::ERule::Auto:
                default:
                    childWidth = childSize.x;
                    break;
            }

            childWidth = std::max(minSize.x, std::min(maxSize.x, childWidth));

            // Clamp the desired height; EVerticalAlignment::Fill overrides it below with the
            // full available height regardless of this value (see Widget::AlignVertically).
            float childHeight = std::max(minSize.y, std::min(maxSize.y, childSize.y));

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
