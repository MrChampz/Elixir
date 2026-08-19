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

            const glm::vec2 childConstraint = {
                innerAvailable.x - margin.GetTotalHorizontal(),
                innerAvailable.y
            };

            auto childSize = slot->GetWidget()->Measure(childConstraint);
            childSize.x += margin.GetTotalHorizontal();

            // Width is the maximum
            totalSize.x = std::max(totalSize.x, childSize.x);

            // Height accumulates using the SAME per-slot rule LayoutChildren applies below,
            // not the raw measured size: a Fixed slot occupies its configured pixels
            // regardless of what its content measures to, and a Fill slot has no intrinsic
            // size of its own - it just stretches into whatever LayoutChildren ends up
            // giving it - so only Auto slots use their measured height. Using the raw
            // measured height unconditionally here made a container (and anything reading
            // its desired size, e.g. a ScrollBox wrapping it) under-report how much space it
            // actually occupies whenever a Fixed slot's content measured smaller than its
            // configured size.
            switch (sizeRule.Rule)
            {
                case SSizeParam::ERule::Fill:
                    break;
                case SSizeParam::ERule::Fixed:
                    totalSize.y += sizeRule.Value + margin.GetTotalVertical();
                    break;
                case SSizeParam::ERule::Auto:
                default:
                    totalSize.y += childSize.y + margin.GetTotalVertical();
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
        std::vector<glm::vec2> childSizes;
        childSizes.reserve(m_Slots.size());

        for (const auto& slot : m_Slots)
        {
            if (!slot->GetWidget()->TakesSpace()) continue;

            const auto margin = slot->GetMargin();

            const glm::vec2 childConstraint = {
                innerSpace.Size.x - margin.GetTotalHorizontal(),
                UnconstrainedSize
            };

            childSizes.push_back(slot->GetWidget()->Measure(childConstraint));
        }

        // First pass: space already spoken for by Auto/Fixed children (main axis = height),
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
                    fixedSpace += sizeRule.Value + margin.GetTotalVertical();
                    break;
                case SSizeParam::ERule::Auto:
                default:
                    fixedSpace += childSizes[i].y + margin.GetTotalVertical();
                    break;
            }
        }

        // Calculate space available for Fill slots.
        const float fillSpace = std::max(0.0f, innerSpace.Size.y - fixedSpace);

        // Second: Arrange children
        float currentY = innerSpace.Position.y;

        for (size_t i = 0; i < m_Slots.size(); ++i)
        {
            const auto& slot = m_Slots[i];
            if (!slot->GetWidget()->TakesSpace()) continue;

            const glm::vec2 childSize = childSizes[i];
            const auto margin = slot->GetMargin();
            const auto hAlignment = slot->GetHorizontalAlignment();
            const auto sizeRule = slot->GetSizeRule();
            const auto minSize = slot->GetMinSize();
            const auto maxSize = slot->GetMaxSize();

            // Calculate child height from its sizing rule.
            float childHeight;

            switch (sizeRule.Rule)
            {
                case SSizeParam::ERule::Fill:
                    // Guard: if no sibling claims a Fill ratio, no extra space is handed out.
                    childHeight = totalFillRatio > 0.0f
                        ? fillSpace * (sizeRule.Value / totalFillRatio) - margin.GetTotalVertical()
                        : 0.0f;
                    break;
                case SSizeParam::ERule::Fixed:
                    childHeight = sizeRule.Value;
                    break;
                case SSizeParam::ERule::Auto:
                default:
                    childHeight = childSize.y;
                    break;
            }

            childHeight = std::max(minSize.y, std::min(maxSize.y, childHeight));

            // Clamp the desired width; EHorizontalAlignment::Fill overrides it below with the
            // full available width regardless of this value (see Widget::AlignHorizontally).
            const float childWidth = std::max(minSize.x, std::min(maxSize.x, childSize.x));

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