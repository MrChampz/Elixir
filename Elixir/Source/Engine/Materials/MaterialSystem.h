#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/FrameSlotState.h>
#include <Engine/Materials/MaterialProxyCache.h>
#include <Engine/Materials/MaterialProxyResolver.h>
#include <Engine/Materials/Rendering/MaterialRenderScene.h>
#include <Engine/Materials/Rendering/Renderer.h>
#include <Engine/Materials/Rendering/TextureRegistry.h>

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

    /** @brief Stores resolved render items for one submitted scene. */
    struct SPreparedScene
    {
        std::vector<SResolvedRenderItem> Items;
        uint32_t MaterialCount = 0;
    };

    /**
     * @brief Prepares frame material data and records material draw commands.
     *
     * The system owns shared material buffers, texture bindings, and render-state
     * preparation for a graphics context.
     */
    class ELIXIR_API MaterialSystem final
    {
    public:
        /**
         * @brief Creates a material system.
         * @param context Graphics context that owns material resources.
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

        /**
         * @brief Starts material collection for the current graphics frame.
         */
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
         * @brief Resolves material proxies and uploads material data for one scene.
         * @param scene Scene that provides material render items.
         * @return Render-ready scene items with their proxies and frame-buffer indices.
         */
        SPreparedScene PrepareScene(const MaterialRenderScene& scene);

        /**
         * @brief Resolves an instance into a render-ready material proxy.
         * @param instance Material instance to resolve.
         * @return The render proxy, or null when the instance cannot be resolved.
         */
        Ref<const MaterialRenderProxy> ResolveMaterialProxy(const Ref<MaterialInstance>& instance);

        /** @brief Gets the buffer that stores material data for the current frame slot. */
        const Ref<DynamicStorageBuffer>& GetActiveFrameBuffer() const;

        /** @brief Stores material resources that are safe to reuse with one frame slot. */
        struct SFrameSlot
        {
            Ref<DynamicStorageBuffer> Buffer;
        };

        uint32_t m_MaterialCapacity = 0;
        FrameSlotState<SFrameSlot> m_FrameSlots;
        TextureRegistry m_Textures;
        MaterialProxyResolver m_ProxyResolver;
        MaterialProxyCache m_ProxyCache;
        Scope<Renderer> m_Renderer;

        std::vector<MaterialRenderScene> m_SubmittedScenes;
        uint64_t m_CurrentFrameNumber = UINT64_MAX;

        const GraphicsContext* m_GraphicsContext = nullptr;
    };
}
