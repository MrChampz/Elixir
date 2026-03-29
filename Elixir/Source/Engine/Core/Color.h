#pragma once

#include <Engine/Core/Core.h>

namespace Elixir
{
    class ELIXIR_API Color
    {
      public:
        static constexpr uint32_t White							= 0xFFFFFF;
        static constexpr uint32_t WhiteAlpha					= 0xFFFFFFFF;
        static constexpr uint32_t Magenta						= 0xFFFF00;
        static constexpr uint32_t MagentaAlpha					= 0xFFFF00FF;
        static constexpr uint32_t Yellow						= 0x00FFFF;
        static constexpr uint32_t YellowAlpha					= 0xFF00FFFF;
    };
}