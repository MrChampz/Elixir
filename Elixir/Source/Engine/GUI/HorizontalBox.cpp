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

            const glm::vec2 childConstraint = {
                innerAvailable.x,
                innerAvailable.y - margin.GetTotalVertical()
            };

            auto childSize = slot->GetWidget()->Measure(childConstraint);
            childSize.y += margin.GetTotalVertical();

            // Height is the maximum
            totalSize.y = std::max(totalSize.y, childSize.y);

            // Width accumulates using the SAME per-slot rule LayoutChildren applies below,
            // not the raw measured size: a Fixed slot occupies its configured pixels
            // regardless of what its content measures to, and a Fill slot has no intrinsic
            // size of its own - it just stretches into whatever LayoutChildren ends up
            // giving it - so only Auto slots use their measured width. Using the raw
            // measured width unconditionally here made a container (and anything reading its
            // desired size, e.g. a ScrollBox wrapping it) under-report how much space it
            // actually occupies whenever a Fixed slot's content measured smaller than its
            // configured size.
            switch (sizeRule.Rule)
            {
                case SSizeParam::ERule::Fill:
                    break;
                case SSizeParam::ERule::Fixed:
                    totalSize.x += sizeRule.Value + margin.GetTotalHorizontal();
                    break;
                case SSizeParam::ERule::Auto:
                default:
                    totalSize.x += childSize.x + margin.GetTotalHorizontal();
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
        std::vector<glm::vec2> childSizes;
        childSizes.reserve(m_Slots.size());

        for (const auto& slot : m_Slots)
        {
            if (!slot->GetWidget()->TakesSpace()) continue;

            const auto margin = slot->GetMargin();

            const glm::vec2 childConstraint = {
                UnconstrainedSize,
                innerSpace.Size.y - margin.GetTotalVertical()
            };

            childSizes.push_back(slot->GetWidget()->Measure(childConstraint));
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