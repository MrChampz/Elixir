#pragma once

#include <Engine/Materials/MaterialProxyCache.h>
#include <Engine/Materials/MaterialProxyResolver.h>
#include <Engine/Materials/Rendering/MaterialRenderScene.h>
#include <Engine/Materials/Rendering/Renderer.h>

namespace Elixir { class ShaderLoader; }

namespace Elixir::Materials
{
    using namespace Rendering;

    /**
     * @brief Configures the initial storage used for frame material data.
     */
    struct SMaterialSystemConfig
    {
        /** @brief Initial number of material entries supported by a frame snapshot. */
        uint32_t InitialFrameCapacity = 256;
    };

    /**
     * @brief Resolves material instances for rendering.
     */
    class ELIXIR_API MaterialSystem final
    {
    public:
        /**
         * @brief Creates a material system.
         * @param context Graphics context used by the material renderer.
         * @param shaderLoader Loader used to obtain material shaders.
         * @param config Initial frame-data storage configuration.
         * @pre context and shaderLoader are valid.
         * @pre config.InitialFrameCapacity is greater than zero.
         */
        MaterialSystem(
            const GraphicsContext* context,
            const ShaderLoader* shaderLoader,
            SMaterialSystemConfig config
        );

        /** @brief Starts material collection for the current graphics frame. */
        void BeginFrame();

        /**
         * @brief Adds a scene to the current material frame.
         * @param scene Frame-local material draw data.
         * @pre BeginFrame was called for the current graphics frame.
         */
        void Submit(MaterialRenderScene scene);

        /**
         * @brief Records all scenes submitted for the current graphics frame.
         * @return Counts of prepared materials, batches, and draws.
         * @pre BeginFrame was called for the current graphics frame.
         */
        SRenderResult RenderFrame();

    private:
        /**
         * @brief Resolves material proxies for one scene.
         * @param scene Scene that provides material render items.
         * @return Scene items with immutable material proxies.
         */
        SPreparedScene PrepareScene(const MaterialRenderScene& scene);

        /**
         * @brief Resolves an instance into a render-ready material proxy.
         * @param instance Material instance to resolve.
         * @return The render proxy, or null when the instance cannot be resolved.
         */
        Ref<const MaterialRenderProxy> ResolveMaterialProxy(const Ref<MaterialInstance>& instance);

        MaterialProxyResolver m_ProxyResolver;
        MaterialProxyCache m_ProxyCache;
        Scope<Renderer> m_Renderer;

        std::vector<MaterialRenderScene> m_SubmittedScenes;

        bool m_IsCollectingFrame = false;
    };
}
