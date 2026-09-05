#pragma once

#include <Engine/Materials/Rendering/MaterialResolver.h>

namespace Elixir::Materials
{
    using namespace Rendering;

    /**
     * @brief Reuses render proxies while their source instances remain current.
     *
     * Entries are invalidated when the instance, its parent material, or either
     * revision changes.
     */
    class ELIXIR_API MaterialProxyCache final : public MaterialResolver
    {
    public:
        /**
         * @brief Creates a cache that resolves misses through another resolver.
         * @param resolver Resolver used when no current proxy is cached.
         */
        explicit MaterialProxyCache(MaterialResolver& resolver);

        /**
         * @brief Resolves an instance from the cache or through the resolver.
         * @param instance Material instance to resolve.
         * @return Immutable render proxy, or null when resolution fails.
         */
        Ref<const MaterialRenderProxy> Resolve(
            const Ref<MaterialInstance>& instance
        ) override;

        /** @brief Removes entries whose source instances no longer exist. */
        void PruneExpired();

    private:
        struct SEntry
        {
            WeakRef<MaterialInstance> Instance;
            Ref<const MaterialRenderProxy> Proxy;
            const Material* Parent = nullptr;
            uint32_t InstanceRevision = 0;
            uint32_t MaterialRevision = 0;
        };

        MaterialResolver& m_Resolver;
        std::unordered_map<const MaterialInstance*, SEntry> m_Entries;
    };
}