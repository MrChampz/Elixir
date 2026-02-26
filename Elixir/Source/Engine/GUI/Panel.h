#pragma once

#include <Engine/GUI/Slot.h>

namespace Elixir::GUI
{
    class Manager;

    class ELIXIR_API Panel : public Widget
    {
      public:
        void Update(Timestep frameTime) override;

        void GenerateDrawCommands(RenderBatch& batch, int zOrder = 0) override;

        SPadding GetPadding() const { return m_Padding; }
        void SetPadding(const SPadding& padding) { m_Padding = padding; }

        SColor GetBackground() const { return m_Background; }
        void SetBackground(const SColor& color) { m_Background = color; }

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
        void SetCornerRadius(const glm::vec4& radius) { m_CornerRadius = radius; }

        const std::vector<Ref<Slot>>& GetSlots() const { return m_Slots; }

      protected:
        SPadding m_Padding;
        SColor m_Background;

        // top-le   ft, top-right, bottom-right, bottom-left
        glm::vec4 m_CornerRadius = {0.0f, 0.0f, 0.0f, 0.0f};

        std::vector<Ref<Slot>> m_Slots;
    };
}