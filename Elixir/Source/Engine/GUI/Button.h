#pragma once

#include <Engine/Font/Font.h>
#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    class ELIXIR_API Button : public ContentWidget
    {
      public:
        explicit Button(const std::string& text = "");

        const std::string& GetText() const { return m_Text; }
        void SetText(const std::string& text);

        SColor GetTextColor() const;
        void SetTextColor(const SColor& color);

        const Ref<Font>& GetFont() const { return m_Font; }
        void SetFont(const Ref<Font>& font);

        float GetFontSize() const { return m_FontSize; }
        void SetFontSize(float size);

        SPadding GetPadding() const { return m_Padding; }
        void SetPadding(const SPadding& padding);

        /**
         * Get corner radius for each corner individually.
         * @return vector (top-left, top-right, bottom-right, bottom-left)
         */
        glm::vec4 GetCornerRadius() const;

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
        void SetCornerRadius(const glm::vec4& radius);

        glm::vec4 GetBackgroundBorders() const;
        void SetBackgroundBorders(const glm::vec4& borders);

      protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override;
        void LayoutChildren(const SRect& allocatedSpace) override;
        void BuildDrawCommands(RenderBatch& batch, int zOrder) override;

        virtual std::string ProcessText(const std::string& text, float availableWidth);
        virtual glm::vec2 MeasureTextSize(const std::string& text);
        virtual glm::vec2 CalculateTextPosition(glm::vec2 textSize);

        void HandleMouseEnter() override;
        void HandleMouseLeave() override;
        SInputReply HandleMouseDown(const MouseButtonPressedEvent& event) override;

      private:
        std::string m_Text;
        Ref<Font> m_Font;
        float m_FontSize = 16.0f;

        SPadding m_Padding;
        glm::vec2 m_MinDesiredSize{ 120.0f, 40.0f };
    };
}