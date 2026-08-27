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

    extern template class TPanel<CanvasSlot>;

    class ELIXIR_API Canvas final : public TPanel<CanvasSlot>
    {
      public:
        Canvas();

        CanvasSlot& AddChild(const Ref<Widget>& child) override;

        /**
         * @brief Set the Canvas size.
         *
         * Canvas has no intrinsic content-driven size the way a flow container does -
         * children are absolutely positioned, so there is no general way to derive "how big
         * this panel wants to be" from them. This is the configured value ComputeDesiredSize
         * reports, capped to whatever the parent actually offers (same rule ScrollBox
         * follows) - it does not clip or otherwise constrain children placed outside it.
         *
         * @param size The desired size.
         */
        void SetSize(const glm::vec2& size);

      protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override;
        void LayoutChildren(const SRect& allocatedSpace) override;

      private:
        SRect ComputeChildGeometry(const CanvasSlot& slot, const glm::vec2& canvasSize) const;

        // The size this Canvas wants to occupy in the parent layout if no constraints.
        glm::vec2 m_Size;
    };
}
