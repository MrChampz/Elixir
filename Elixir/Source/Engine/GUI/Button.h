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
        SColor GetNormalColor() const { return m_NormalColor; }
        void SetNormalColor(const SColor& color) { m_NormalColor = color; }
        SColor GetTextColor() const { return m_TextColor; }
        void SetTextColor(const SColor& color) { m_TextColor = color; }

      private:
        std::string m_Text;

        SColor m_NormalColor{0.3f, 0.3f, 0.8f, 1.0f};
        SColor m_TextColor{1.0f, 1.0f, 1.0f, 1.0f};
    };
}