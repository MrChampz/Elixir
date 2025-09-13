#pragma once

#include <Engine/Core/Core.h>

namespace Elixir
{
    struct SShaderBinding
    {
        uint32_t Set;
        uint32_t Binding;

        bool operator==(const SShaderBinding& other) const noexcept
        {
            return Set == other.Set && Binding == other.Binding;
        }

        auto GetHashParams() const
        {
            return std::tie(Set, Binding);
        }
    };
}

GENERATE_HASH_FUNCTION(Elixir::SShaderBinding)