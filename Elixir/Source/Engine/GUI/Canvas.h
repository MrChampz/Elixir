#pragma once

#include <Engine/GUI/Panel.h>

namespace Elixir::GUI
{
    class ELIXIR_API CanvasSlot final : public Slot
    {
      public:
        explicit CanvasSlot(const Ref<Widget>& widget)
        {
            m_Widget = widget;
            m_Constraint.Size = widget->GetDesiredSize();
        }

        const SAnchors& GetAnchors() const { return m_Anchors; }
        CanvasSlot& SetAnchors(const SAnchors& anchors)
        {
            m_Anchors = anchors;
            return *this;
        }

        const SConstraint& GetConstraint() const { return m_Constraint; }

        CanvasSlot& SetPosition(const glm::vec2& pos)
        {
            m_Constraint.Position = pos;
            return *this;
        }

        CanvasSlot& SetSize(const glm::vec2& size)
        {
            m_Constraint.Size = size;
            return *this;
        }

        CanvasSlot& SetOffsets(
            const float left,
            const float top,
            const float right,
            const float bottom
        )
        {
            m_Constraint.Offsets = { left, top, right, bottom };
            return *this;
        }

        CanvasSlot& SetAlignment(const glm::vec2& alignment)
        {
            m_Constraint.Alignment = alignment;
            return *this;
        }

      private:
        SAnchors m_Anchors = SAnchors::TopLeft();
        SConstraint m_Constraint;
    };

    class ELIXIR_API Canvas final : public Panel
    {
      public:
        Canvas();

        CanvasSlot& AddChild(const Ref<Widget>& child);

        glm::vec2 ComputeDesiredSize() override;
        void ArrangeChildren(const SRect& allocatedSpace) override;

      private:
        SRect ComputeChildGeometry(const Ref<CanvasSlot>& slot, const glm::vec2& canvasSize) const;
    };
}