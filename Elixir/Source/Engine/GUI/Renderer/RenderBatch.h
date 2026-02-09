#pragma once

#include <Engine/GUI/Types.h>
#include <Engine/Graphics/Texture.h>

namespace Elixir::GUI
{
    struct SDrawCommand
    {
        enum class EType : uint8_t
        {
            Rect, Text, DebugRect
        };

        EType Type;
        SRect Geometry;
        SColor Color;

        /**
         * When texture is used, this represents the borders of 9-patch texture.
         * Border mapping = (left, top, right, bottom).
         *
         * When not used, this represents the corner radius of the quad.
         * Corner Radius mapping = (top-left, top-right, bottom-right, bottom-left).
         */
        glm::vec4 Border = glm::vec4(0.0f);

        /**
         * Shadow offset (x, y), blur (z) and intensity (w).
         */
        glm::vec4 InsetShadow = glm::vec4(0.0f);

        /**
         * Shadow offset (x, y), blur (z) and intensity (w).
         */
        glm::vec4 DropShadow = glm::vec4(0.0f);

        SOutline Outline;

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

        void AddRect(
            const SRect& rect,
            const SColor& color,
            glm::vec4 cornerRadius,
            glm::vec4 insetShadow,
            glm::vec4 dropShadow,
            SOutline outline,
            int zOrder = 0
        );

        void AddText(const std::string& text, const glm::vec2& position, float fontSize, const SColor& color, int zOrder = 0);
        void AddTexture(const Ref<Texture2D>& texture, const SRect& rect, const SColor& tint, int zOrder = 0);

        void AddDebugRect(const SRect& rect, const SColor& color = { 1.0f, 0.0f, 0.0f, 1.0f });

        const std::vector<SDrawCommand>& GetCommands() const { return m_Commands; }

      private:
        std::vector<SDrawCommand> m_Commands;
    };
}