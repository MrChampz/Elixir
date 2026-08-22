#pragma once

#include <Engine/Font/Font.h>
#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    /** @brief Button visual data for one interactive state. */
    struct SButtonAppearance : SAppearance
    {
        SColor Foreground{};
    };

    /** @brief Complete visual style for Button. */
    struct SButtonStyle final : SStyle, TStateStyles<SButtonAppearance>{};

    class ELIXIR_API Button : public ContentWidget
    {
      public:
        /**
         * @brief Construct a button with a copy of the current default button style.
         * @param text Initial label.
         */
        explicit Button(const std::string& text = "");

        /**
         * @brief Replace this button's complete style.
         * @param style Style to copy.
         */
        void SetStyle(const SButtonStyle& style);

        const std::string& GetText() const { return m_Text; }
        void SetText(const std::string& text);

        const Ref<Font>& GetFont() const { return m_Font; }
        void SetFont(const Ref<Font>& font);

        float GetFontSize() const { return m_FontSize; }
        void SetFontSize(float size);

        SPadding GetPadding() const { return m_Padding; }
        void SetPadding(const SPadding& padding);

        /**
         * @brief Set one legacy layer's text color.
         * @param layer Legacy interaction layer to change.
         * @param color Text color for that layer.
         */
        void SetTextColor(EStyleLayer layer, const SColor& color);

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

        const SAppearance& GetResolvedAppearance() const override;
        SBrush& GetMutableBackgroundBrush(EStyleLayer layer) override;

      private:
        std::string m_Text;
        Ref<Font> m_Font;
        float m_FontSize = 16.0f;

        SPadding m_Padding;
        glm::vec2 m_MinDesiredSize{ 120.0f, 40.0f };
        SButtonStyle m_Style;
    };
}
