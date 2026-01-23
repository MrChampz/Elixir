#include "Canvas.h"

namespace Elixir::GUI
{
    Canvas::Canvas()
    {
        m_DesiredSize = { 800.0f, 600.0f };
    }

    CanvasSlot& Canvas::AddChild(const Ref<Widget>& child)
    {
        const auto slot = CreateRef<CanvasSlot>(child);
        m_Slots.push_back(slot);
        return *slot;
    }

    glm::vec2 Canvas::ComputeDesiredSize()
    {
        return m_DesiredSize;
    }

    void Canvas::ArrangeChildren(const SRect& allocatedSpace)
    {
        m_Geometry = allocatedSpace;

        // Arrange each child based on its anchors and constraints
        for (const auto& slot : m_Slots)
        {
            const auto canvasSlot = std::static_pointer_cast<CanvasSlot>(slot);
            SRect childGeometry = ComputeChildGeometry(canvasSlot, allocatedSpace.Size);
            slot->GetWidget()->ArrangeChildren(childGeometry);
        }
    }

    SRect Canvas::ComputeChildGeometry(const Ref<CanvasSlot>& slot, const glm::vec2& canvasSize) const
    {
        const SAnchors& anchors = slot->GetAnchors();
        const SConstraint& constraint = slot->GetConstraint();

        SRect result;

        // Calculate anchor positions in canvas space
        const glm::vec2 anchorMin = {
            canvasSize.x * anchors.MinX,
            canvasSize.y * anchors.MinY
        };

        const glm::vec2 anchorMax = {
            canvasSize.x * anchors.MaxX,
            canvasSize.y * anchors.MaxY
        };

        if (anchors.IsNonStretching())
        {
            // NON-STRETCHING MODE
            // Widget has explicit size and position offset from anchor

            // Start at the anchor point
            const glm::vec2 anchorPoint = anchorMin; // min == max for non-stretching

            // Apply position offset
            glm::vec2 position = anchorPoint + constraint.Position;

            // Apply alignment (pivot point)
            position.x -= constraint.Size.x * constraint.Alignment.x;
            position.y -= constraint.Size.y * constraint.Alignment.y;

            result.Position = m_Geometry.Position + position;
            result.Size = constraint.Size;
        }
        else
        {
            // STRETCHING MODE
            // Widget stretches between anchor points with margin offsets

            if (anchors.IsStretchingHorizontally())
            {
                // Stretch horizontally
                result.Position.x = m_Geometry.Position.x + anchorMin.x + constraint.Offsets.Left;
                result.Size.x = (anchorMax.x - anchorMin.x) - constraint.Offsets.Left + constraint.Offsets.Right;
            }
            else
            {
                // Fixed width
                const glm::vec2 anchorPoint = anchorMin;
                result.Position.x = m_Geometry.Position.x + anchorPoint.x + constraint.Position.x
                    - constraint.Size.x * constraint.Alignment.x;
                result.Size.x = constraint.Size.x;
            }

            if (anchors.IsStretchingVertically())
            {
                // Stretch vertically
                result.Position.y = m_Geometry.Position.y + anchorMin.y + constraint.Offsets.Top;
                result.Size.y = (anchorMax.y - anchorMin.y) - constraint.Offsets.Top + constraint.Offsets.Bottom;
            }
            else
            {
                // Fixed height
                const glm::vec2 anchorPoint = anchorMin;
                result.Position.y = m_Geometry.Position.y + anchorPoint.y + constraint.Position.y
                    - constraint.Size.y * constraint.Alignment.y;
                result.Size.y = constraint.Size.y;
            }
        }

        return result;
    }
}