#pragma once

#include <Engine/Graphics/Material/MaterialIR.h>

#include <string>

namespace Elixir
{
    class ELIXIR_API MaterialHLSLEmitter
    {
      public:
        [[nodiscard]] static std::string Emit(const SMaterialIR& material);
    };
}
