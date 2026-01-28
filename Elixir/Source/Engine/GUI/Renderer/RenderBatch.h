#pragma once

#include <Engine/GUI/Types.h>

namespace Elixir::GUI
{
    struct SDrawCommand
    {
        enum class EType : uint8_t
        {
            Rect, Text
        };

        EType Type;
        SRect Geometry;
        SColor Color;

        /**
         * top-left, top-right, bottom-right, bottom-left
         */
        glm::vec4 CornerRadius;

        float BorderWidth = 1.0f;

        // For text rendering
        std::string Text;
        float FontSize = 16.0f;

        // For texture rendering
        Ref<Texture2D> Texture;
        SRect TexCoords;

        // Z-order for sorting
        int ZOrder = 0;
    };

    class ELIXIR_API RenderBatch final
    {
      public:
        void Sort();
        void Clear();

        void AddRect(const SRect& rect, const SColor& color, glm::vec4 cornerRadius, int zOrder = 0);
        void AddText(const std::string& text, const glm::vec2& position, float fontSize, const SColor& color, int zOrder = 0);
        void AddTexture(const Ref<Texture2D>& texture, const SRect& rect, const SColor& tint, int zOrder = 0);

        const std::vector<SDrawCommand>& GetCommands() const { return m_Commands; }

      private:
        std::vector<SDrawCommand> m_Commands;
    };
}