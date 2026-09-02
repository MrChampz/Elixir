#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Engine/Materials/Rendering/MaterialResolver.h>
#include <Engine/Materials/Rendering/MaterialRenderScene.h>
#include <Engine/Materials/Rendering/FrameTable.h>
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

    /**
     * @brief Stores material data prepared for one submitted frame.
     */
    struct SMaterialFrameSnapshot
    {
        /** @brief Maps resolved material proxies to their frame data. */
        Ref<const FrameTable> Table;

        /** @brief Number of materials stored in Table. */
        uint32_t MaterialCount = 0;

        /** @brief Serial that identifies the submission that owns this snapshot. */
        uint64_t SubmissionSerial = 0;
    };

    /**
     * @brief Reports the work recorded by a material render pass.
     */
    struct SMaterialRenderResult
    {
        /** @brief Number of material batches rendered. */
        uint32_t BatchCount = 0;

        /** @brief Number of draw commands recorded. */
        uint32_t DrawCount = 0;
    };

    /**
     * @brief Prepares frame material data and records material draw commands.
     *
     * The system owns shared material buffers, texture bindings, and render-state
     * preparation for a graphics context.
     */
    class ELIXIR_API MaterialSystem final : public MaterialResolver
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
         * @brief Builds and uploads material data for a scene submission.
         * @param scene Scene that provides material render items.
         * @param submissionSerial Serial that identifies the submission.
         * @return A snapshot that contains the resolved frame material data.
         */
        SMaterialFrameSnapshot BuildFrameSnapshot(
            const MaterialRenderScene& scene,
            uint64_t submissionSerial
        );

        /**
         * @brief Gets the shader program key required for a material pass.
         * @param pass Material pass to prepare.
         * @param material Resolved material data.
         * @return The program key, or no value when the pass is unsupported.
         */
        std::optional<SProgramKey> GetProgramKey(
            EMaterialPass pass,
            const MaterialRenderProxy& material
        ) const;

        /**
         * @brief Prepares GPU state for one material pass request.
         * @param request Material pass and geometry requirements.
         * @return Prepared pass state, or no value when preparation fails.
         */
        std::optional<SPreparedPass> PrepareMaterialPass(const SPassRequest& request) const;

        /**
         * @brief Records draw commands for the scene materials.
         * @param cmd Command buffer that receives the draw commands.
         * @param scene Scene that provides render items.
         * @param snapshot Material data built for this scene submission.
         * @return Counts of rendered batches and recorded draw commands.
         */
        SMaterialRenderResult Render(
            const Ref<CommandBuffer>& cmd,
            const MaterialRenderScene& scene,
            const SMaterialFrameSnapshot& snapshot
        ) const;

        /**
         * @brief Resolves an instance into a render-ready material proxy.
         * @param instance Material instance to resolve.
         * @return The render proxy, or null when the instance cannot be resolved.
         */
        Ref<const MaterialRenderProxy> Resolve(
            const Ref<MaterialInstance>& instance
        ) override;

        /** @brief Gets the buffer that stores frame material data. */
        const Ref<DynamicStorageBuffer>& GetFrameBuffer() const { return m_FrameBuffer; }

        /** @brief Gets the texture set used by material rendering. */
        const Ref<TextureSet>& GetTextureSet() const { return m_Textures.GetTextureSet(); }

        /** @brief Gets the sampler used by material textures. */
        const Ref<Sampler>& GetSampler() const { return m_Textures.GetSampler(); }

    private:
        uint32_t m_MaterialCapacity = 0;
        Ref<DynamicStorageBuffer> m_FrameBuffer;
        TextureRegistry m_Textures;
        Scope<Renderer> m_Renderer;
    };
}
