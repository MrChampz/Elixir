#pragma once

#include <Engine/GUI/Types.h>

namespace Elixir::GUI
{
    struct SDrawCommand
    {
        enum class EType : uint8_t
        {
            Rect, RectOutline, Text, Texture
        };

        EType Type;
        SRect Geometry;
        SColor Color;
        float BorderWidth = 1.0f;

        // For text rendering
        std::string Text;
        float FontSize = 16.0f;

        // For texture rendering
        SRect TexCoords;

        // Z-order for sorting
        int ZOrder = 0;
    };

    class ELIXIR_API RenderBatch final
    {
      public:
        void Sort();
        void Clear();

        void AddRect(const SRect& rect, const SColor& color, int zOrder = 0);

        const std::vector<SDrawCommand>& GetCommands() const { return m_Commands; }

      private:
        std::vector<SDrawCommand> m_Commands;
    };
}