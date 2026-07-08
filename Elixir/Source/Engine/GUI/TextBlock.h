#pragma once

#include <Engine/GUI/Definitions.h>
#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    class RenderBatch;

    class ELIXIR_API TextBlock final : public Widget
    {
      public:
        explicit TextBlock(const std::string& text);

        glm::vec2 ComputeDesiredSize() override;

        const std::string& GetText() const { return m_Text; }
        void SetText(const std::string& text);

        const Ref<Font>& GetFont() const { return m_Font; }
        void SetFont(const Ref<Font>& font);

        const SColor& GetColor() const { return m_Color; }
        void SetColor(const SColor& color);

        float GetFontSize() const { return m_FontSize; }
        void SetFontSize(float size);

    protected:
        void Draw(RenderBatch& batch, int zOrder) override;

        void UpdateTextSize();

        std::string ProcessText(const std::string& text, float availableWidth) const;

      private:
        std::string m_Text;
        SColor m_Color{ 1.0, 1.0, 1.0, 1.0 };
        Ref<Font> m_Font;
        float m_FontSize = 16.0f;
    };
}
