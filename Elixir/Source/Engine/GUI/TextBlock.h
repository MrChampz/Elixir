#pragma once

#include <Engine/Font/FontManager.h>
#include <Engine/GUI/Definitions.h>
#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    class RenderBatch;

    class ELIXIR_API TextBlock final : public Widget
    {
      public:
        explicit TextBlock(const std::string& text) : m_Text(text)
        {
            m_Font = FontManager::GetDefaultFont();
            UpdateTextSize();
        }

        glm::vec2 ComputeDesiredSize() override
        {
            return m_DesiredSize;
        }

        void GenerateDrawCommands(RenderBatch& batch, const int zOrder) override
        {
            if (!m_Text.empty())
            {
                const float availableWidth = m_Geometry.Size.x;
                const auto displayText = ProcessText(m_Text, availableWidth);

                batch.AddText(
                    displayText,
                    m_Geometry,
                    m_Font,
                    m_FontSize,
                    m_Color,
                    zOrder
                );
            }
        }

        const std::string& GetText() const { return m_Text; }
        void SetText(const std::string& text)
        {
            m_Text = text;
            UpdateTextSize();
        }

        const Ref<Font>& GetFont() const { return m_Font; }
        void SetFont(const Ref<Font>& font)
        {
            m_Font = font;
            UpdateTextSize();
        }

        const SColor& GetColor() const { return m_Color; }
        void SetColor(const SColor& color) { m_Color = color; }

        float GetFontSize() const { return m_FontSize; }
        void SetFontSize(const float size)
        {
            m_FontSize = size;
            UpdateTextSize();
        }

    protected:
        void UpdateTextSize()
        {
            m_DesiredSize = FontManager::MeasureText(m_Text, m_Font, m_FontSize);
        }

        std::string ProcessText(const std::string& text, const float availableWidth) const
        {
            auto size = FontManager::MeasureText(text, m_Font, m_FontSize);
            if (size.x <= availableWidth)
                return text;

            const std::string ellipsis = "...";
            const float ellipsisWidth = FontManager::MeasureText(ellipsis, m_Font, m_FontSize).x;

            if (ellipsisWidth >= availableWidth)
                return ellipsis;

            std::string truncated = text;
            while (!truncated.empty())
            {
                truncated.pop_back();
                size = FontManager::MeasureText(truncated, m_Font, m_FontSize);
                if (size.x + ellipsisWidth <= availableWidth)
                    return truncated + ellipsis;
            }

            return ellipsis;
        }

      private:
        std::string m_Text;
        SColor m_Color{ 1.0, 1.0, 1.0, 1.0 };
        Ref<Font> m_Font;
        float m_FontSize = 16.0f;
    };
}