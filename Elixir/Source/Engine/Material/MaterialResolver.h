#pragma once

#include <Engine/Material/MaterialInstance.h>
#include <Engine/Material/MaterialRenderProxy.h>

namespace Elixir
{
    /**
     * @brief Resolves a material instance into render-ready material data.
     *
     * Implementations publish an immutable proxy that render code can safely use.
     */
    class ELIXIR_API MaterialResolver
    {
    public:
        /** @brief Destroys the resolver. */
        virtual ~MaterialResolver() = default;

        /**
         * @brief Resolves an instance into a render proxy.
         * @param instance Material instance to resolve.
         * @return The render proxy, or null when the instance cannot be resolved.
         */
        virtual Ref<const MaterialRenderProxy> Resolve(
            const Ref<MaterialInstance>& instance
        ) = 0;
    };
}
