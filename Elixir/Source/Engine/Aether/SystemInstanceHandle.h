#pragma once

#include <Engine/Core/UUID.h>

namespace Elixir::Aether
{
    struct SSystemInstanceHandle
    {
        UUID Id;

        bool operator==(const SSystemInstanceHandle&) const = default;
    };
}