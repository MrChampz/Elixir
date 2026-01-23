#pragma once

#include <Engine/GUI/Panel.h>

namespace Elixir::GUI
{
    class ELIXIR_API CanvasSlot final : public Slot
    {
      public:
        explicit CanvasSlot(const Ref<Widget>& widget) { m_Widget = widget; }

        const SAnchors& GetAnchors() const { return Anchors; }
        CanvasSlot& SetAnchors(const SAnchors& anchors)
        {
            Anchors = anchors;
            return *this;
        }

        const SConstraint& GetConstraint() const { return Constraint; }

        CanvasSlot& SetPosition(const glm::vec2& pos)
        {
            Constraint.Position = pos;
            return *this;
        }

        CanvasSlot& SetSize(const glm::vec2& size)
        {
            Constraint.Size = size;
            return *this;
        }

        CanvasSlot& SetOffsets(
            const float left,
            const float top,
            const float right,
            const float bottom
        )
        {
            Constraint.Offsets = { left, top, right, bottom };
            return *this;
        }

        CanvasSlot& SetAlignment(const glm::vec2& alignment)
        {
            Constraint.Alignment = alignment;
            return *this;
        }

      private:
        SAnchors Anchors = SAnchors::TopLeft();
        SConstraint Constraint;
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