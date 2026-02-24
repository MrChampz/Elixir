#pragma once

#include <Engine/Font/Font.h>
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

        SColor GetTextColor() const { return m_TextColor; }
        void SetTextColor(const SColor& color) { m_TextColor = color; }

        const Ref<SFont>& GetFont() const { return m_Font; }
        void SetFont(const Ref<SFont>& font) { m_Font = font; }

        float GetFontSize() const { return m_FontSize; }
        void SetFontSize(const float size) { m_FontSize = size; }

        SPadding GetPadding() const { return m_Padding; }
        void SetPadding(const SPadding& padding) { m_Padding = padding; }

        /**
         * Get corner radius for each corner individually.
         * @return vector (top-left, top-right, bottom-right, bottom-left)
         */
        glm::vec4 GetCornerRadius() const { return m_CornerRadius; }

        /**
         * Set the same radius for all corners.
         * @param radius corner radius in pixels
         */
        void SetCornerRadius(const float radius)
        {
            SetCornerRadius({ radius, radius, radius, radius });
        }

        /**
         * Set a radius for each corner individually.
         * @param radius vector (top-left, top-right, bottom-right, bottom-left)
         */
        void SetCornerRadius(const glm::vec4& radius) { m_CornerRadius = radius; }

        SColor GetNormalColor() const { return m_NormalColor; }
        void SetNormalColor(const SColor& color) { m_NormalColor = color; }

        SColor GetHoverColor() const { return m_HoverColor; }
        void SetHoverColor(const SColor& color) { m_HoverColor = color; }

        const Ref<Texture2D>& GetNormalBackground() const { return m_NormalBackground; }
        void SetNormalBackground(const Ref<Texture2D>& texture) { m_NormalBackground = texture; }

      protected:
        virtual std::string ProcessText(const std::string& text, float availableWidth);
        virtual glm::vec2 MeasureTextSize(const std::string& text);
        virtual glm::vec2 CalculateTextPosition(glm::vec2 textSize);

        void HandleMouseEnter() override;
        void HandleMouseLeave() override;

      private:
        std::string m_Text;
        SColor m_TextColor{1.0f, 0.0f, 0.0f, 1.0f};
        Ref<SFont> m_Font;
        float m_FontSize = 16.0f;

        SPadding m_Padding;

        // top-left, top-right, bottom-right, bottom-left
        glm::vec4 m_CornerRadius = {0.0f, 0.0f, 0.0f, 0.0f};

        // Colors for different states
        SColor m_NormalColor{0.3f, 0.3f, 0.8f, 1.0f};
        SColor m_HoverColor{1.0f, 0.0f, 0.0f, 1.0f};

        // Textures for different states
        Ref<Texture2D> m_NormalBackground;
    };
}