#include "epch.h"
#include "Overlay.h"

namespace Elixir::GUI
{
    glm::vec2 Overlay::ComputeDesiredSize(const glm::vec2& availableSize)
    {
        const glm::vec2 innerAvailable = {
            availableSize.x - m_Padding.GetTotalHorizontal(),
            availableSize.y - m_Padding.GetTotalVertical()
        };

        glm::vec2 totalSize = { 0, 0 };

        for (const auto& slot : m_Slots)
        {
            if (!slot->GetWidget()->TakesSpace()) continue;

            const auto margin = slot->GetMargin();

            const glm::vec2 childConstraint = {
                innerAvailable.x - margin.GetTotalHorizontal(),
                innerAvailable.y - margin.GetTotalVertical()
            };

            auto childSize = slot->GetWidget()->Measure(childConstraint);

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

        return totalSize;
    }

    void Overlay::LayoutChildren(const SRect& allocatedSpace)
    {
        // Calculate available space after padding
        const SRect innerSpace = ApplyPadding(allocatedSpace, m_Padding);

        // Overlay has no main axis - every child gets the full inner space and is placed by
        // alignment alone. EHorizontalAlignment::Fill / EVerticalAlignment::Fill stretch a
        // child across that space; SSizeParam does not apply here (see LayoutSlot).
        for (const auto& slot : m_Slots)
        {
            if (!slot->GetWidget()->TakesSpace()) continue;

            const auto margin = slot->GetMargin();
            const auto hAlignment = slot->GetHorizontalAlignment();
            const auto vAlignment = slot->GetVerticalAlignment();

            const glm::vec2 childConstraint = {
                innerSpace.Size.x - margin.GetTotalHorizontal(),
                innerSpace.Size.y - margin.GetTotalVertical()
            };

            const glm::vec2 childSize = slot->GetWidget()->Measure(childConstraint);

            // Align within the overlay space
            SRect childGeometry = AlignChild(
                childSize,
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