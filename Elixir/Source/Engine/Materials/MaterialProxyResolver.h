#pragma once

#include <Engine/Materials/Compilation/CompilationCache.h>
#include <Engine/Materials/Rendering/MaterialResolver.h>

namespace Elixir { class ShaderLoader; }

namespace Elixir::Materials
{
    using namespace Compilation;
    using namespace Rendering;

    /**
     * @brief Resolves material instances into immutable render proxies.
     *
     * The resolver owns compiled-material reuse. It does not cache proxies for
     * individual instances.
     */
    class ELIXIR_API MaterialProxyResolver final : public MaterialResolver
    {
    public:
        /**
         * @brief Creates a resolver for one shader loader.
         * @param shaderLoader Loader used to compile material shaders.
         */
        explicit MaterialProxyResolver(const ShaderLoader* shaderLoader);

        /**
         * @brief Resolves an instance into a render proxy.
         * @param instance Material instance to resolve.
         * @return Immutable render proxy, or null when resolution fails.
         */
        Ref<const MaterialRenderProxy> Resolve(
            const Ref<MaterialInstance>& instance
        ) override;

    private:
        CompilationCache m_CompilationCache;
    };
}