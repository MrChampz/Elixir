#pragma once

#include <Engine/GUI/Types.h>
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
            batch.AddText(m_Text, m_Geometry.Position, m_FontSize, m_Color, zOrder);
        }

        const std::string& GetText() const { return m_Text; }
        void SetText(const std::string& text) { m_Text = text; }

        const SColor& GetColor() const { return m_Color; }
        void SetColor(const SColor& color) { m_Color = color; }

        float GetFontSize() const { return m_FontSize; }
        void SetFontSize(const float size) { m_FontSize = size; }

      private:
        std::string m_Text;
        SColor m_Color{ 1.0, 1.0, 1.0, 1.0 };
        float m_FontSize = 16.0f;
    };
}