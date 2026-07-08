#pragma once

#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    class Manager;

    class ELIXIR_API Panel : public Widget
    {
      public:
        void Update(Timestep frameTime) override;

        /**
         * Remove a child widget from this panel: drops the slot holding it and clears the
         * child's parent back-pointer (via DetachChild), marking layout dirty. No-op if the
         * widget is not a child of this panel. Promotes the Widget hook to public API.
         * @param child the widget to remove.
         */
        void RemoveChild(const Ref<Widget>& child) override;

        /**
         * Remove all children from this panel, detaching each, and mark layout dirty.
         */
        void ClearChildren();

        SPadding GetPadding() const { return m_Padding; }
        void SetPadding(const SPadding& padding);

        SColor GetBackground() const { return m_Background; }
        void SetBackground(const SColor& color);

        /**
         * Get corner radius for each corner individually.
         * @return vector(top-left, top-right, bottom-right, bottom-left)
         */
        glm::vec4 GetCornerRadius() const { return m_CornerRadius; }

        /**
         * Set same radius for all corners.
         * @param radius corner radius in pixels
         */
        void SetCornerRadius(const float radius)
        {
            SetCornerRadius({ radius, radius, radius, radius });
        }

        /**
         * Set radius for each corner individually.
         * @param radius vector(top-left, top-right, bottom-right, bottom-left)
         */
        void SetCornerRadius(const glm::vec4& radius);

        const std::vector<Ref<Slot>>& GetSlots() const { return m_Slots; }

      protected:
        /**
         * Invoke the fn for each direct child of this widget.
         * Container widgets override this to expose their children; leaf widgets keep the
         * default no-op. Lets callers walk the widget tree without knowing concrete types.
         * @param fn callback invoked once per child widget.
         */
        void ForEachChild(const std::function<void(const Ref<Widget>&)>& fn) const override;

        void Draw(RenderBatch& batch, int zOrder) override;

        SPadding m_Padding;
        SColor m_Background;

        // top-le   ft, top-right, bottom-right, bottom-left
        glm::vec4 m_CornerRadius = {0.0f, 0.0f, 0.0f, 0.0f};

        std::vector<Ref<Slot>> m_Slots;
    };
}