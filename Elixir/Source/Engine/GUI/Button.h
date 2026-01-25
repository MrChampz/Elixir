#pragma once

#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    class ELIXIR_API Button : public Widget
    {
      public:
        explicit Button(const std::string& text = "");

        glm::vec2 ComputeDesiredSize() override;
        void GenerateDrawCommands(RenderBatch& batch, int zOrder) override;

        const std::string& GetText() const { return m_Text; }
        void SetText(const std::string& text) { m_Text = text; }

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

        SColor GetNormalColor() const { return m_NormalColor; }
        void SetNormalColor(const SColor& color) { m_NormalColor = color; }

        SColor GetTextColor() const { return m_TextColor; }
        void SetTextColor(const SColor& color) { m_TextColor = color; }

      private:
        std::string m_Text;

        // top-left, top-right, bottom-right, bottom-left
        glm::vec4 m_CornerRadius = {0.0f, 0.0f, 0.0f, 0.0f};

        SColor m_NormalColor{0.3f, 0.3f, 0.8f, 1.0f};
        SColor m_TextColor{1.0f, 1.0f, 1.0f, 1.0f};
    };
}