#pragma once

#include <Engine/Material/MaterialInstance.h>
#include <Engine/Material/MaterialRenderProxy.h>

namespace Elixir
{
    // Boundary used by scene compilers to publish immutable GPU material state.
    class ELIXIR_API MaterialResolver
    {
    public:
        virtual ~MaterialResolver() = default;

        virtual Ref<const MaterialRenderProxy> Resolve(
            const Ref<MaterialInstance>& instance
        ) = 0;
    };
}