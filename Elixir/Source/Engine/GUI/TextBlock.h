#pragma once

#include <Engine/GUI/Definitions.h>
#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    class RenderBatch;

    /**
     * @brief How TextBlock handles text that does not fit its allocated width.
     */
    enum class ETextOverflow
    {
        /** Truncate the first logical line and append an ellipsis when needed. */
        Ellipsis,
        /** Preserve explicit line breaks and wrap additional lines within the allocated rect. */
        Wrap,
        /** Truncate the first logical line at the allocated width. */
        Clip,
    };

    class ELIXIR_API TextBlock final : public Widget
    {
      public:
        explicit TextBlock(const std::string& text);

        const std::string& GetText() const { return m_Text; }
        void SetText(const std::string& text);

        const Ref<Font>& GetFont() const { return m_Font; }
        void SetFont(const Ref<Font>& font);

        const SColor& GetColor() const { return m_Color; }
        void SetColor(const SColor& color);

        float GetFontSize() const { return m_FontSize; }
        void SetFontSize(float size);

        ETextOverflow GetOverflow() const { return m_Overflow; }
        void SetOverflow(ETextOverflow overflow);

    protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override;
        void LayoutChildren(const SRect& allocatedSpace) override;

        void BuildDrawCommands(RenderBatch& batch, int zOrder) override;

        std::string ClipText(const std::string& text, float availableWidth) const;
        std::string EllipsizeText(
            const std::string& text,
            float availableWidth,
            bool appendEllipsis
        ) const;

        glm::vec2 UpdateWrappedDisplayText(float maxWidth, float maxHeight = UnconstrainedSize);

      private:
        std::string m_Text;
        std::string m_DisplayText;

        SColor m_Color{ 1.0, 1.0, 1.0, 1.0 };
        Ref<Font> m_Font;
        float m_FontSize = 16.0f;
        ETextOverflow m_Overflow = ETextOverflow::Ellipsis;

    };
}
