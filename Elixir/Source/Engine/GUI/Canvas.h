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
            if (m_Widget)
                m_Constraint.Size = m_Widget->GetDesiredSize();
        }

        const SAnchors& GetAnchors() const { return m_Anchors; }
        CanvasSlot& SetAnchors(const SAnchors& anchors)
        {
            m_Anchors = anchors;
            InvalidateOwnerLayout();
            return *this;
        }

        const SConstraint& GetConstraint() const { return m_Constraint; }

        CanvasSlot& SetPosition(const glm::vec2& pos)
        {
            m_Constraint.Position = pos;
            InvalidateOwnerLayout();
            return *this;
        }

        CanvasSlot& SetSize(const glm::vec2& size)
        {
            m_Constraint.Size = size;
            InvalidateOwnerLayout();
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
            InvalidateOwnerLayout();
            return *this;
        }

        CanvasSlot& SetAlignment(const glm::vec2& alignment)
        {
            m_Constraint.Alignment = alignment;
            InvalidateOwnerLayout();
            return *this;
        }

      private:
        SAnchors m_Anchors = SAnchors::TopLeft();
        SConstraint m_Constraint;
    };

    extern template class ELIXIR_API TPanel<CanvasSlot>;

    class ELIXIR_API Canvas final : public TPanel<CanvasSlot>
    {
      public:
        Canvas();

        CanvasSlot& AddChild(const Ref<Widget>& child);

      protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override;
        void LayoutChildren(const SRect& allocatedSpace) override;

      private:
        SRect ComputeChildGeometry(const CanvasSlot& slot, const glm::vec2& canvasSize) const;

        // Canvas has no intrinsic content-driven size (children are absolutely positioned),
        // so ComputeDesiredSize just reports this fixed fallback.
        glm::vec2 m_DefaultDesiredSize;
    };
}