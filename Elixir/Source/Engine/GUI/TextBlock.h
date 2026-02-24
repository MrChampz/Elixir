#pragma once

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
            m_DesiredSize = { 200, 20 };
        }

        glm::vec2 ComputeDesiredSize() override
        {
            return m_DesiredSize;
        }

        void GenerateDrawCommands(RenderBatch& batch, const int zOrder) override
        {
            const auto textSize = MeasureTextSize(m_Text, m_FontSize);
            batch.AddText(
                m_Text,
                { m_Geometry.Position, textSize },
                m_FontSize,
                m_Color,
                zOrder
            );
        }

        const std::string& GetText() const { return m_Text; }
        void SetText(const std::string& text) { m_Text = text; }

        const SColor& GetColor() const { return m_Color; }
        void SetColor(const SColor& color) { m_Color = color; }

        float GetFontSize() const { return m_FontSize; }
        void SetFontSize(const float size) { m_FontSize = size; }

      private:
        static glm::vec2 MeasureTextSize(const std::string& text, const float fontSize)
        {
            float charWidth = fontSize * 0.6f; // TODO: Get chat width from font metrics
            float width = text.length() * charWidth;
            float height = fontSize * 1.2f; // TODO: Get line height from button properties?

            return { width, height };
        }

        std::string m_Text;
        SColor m_Color{ 1.0, 1.0, 1.0, 1.0 };
        float m_FontSize = 16.0f;
    };
}