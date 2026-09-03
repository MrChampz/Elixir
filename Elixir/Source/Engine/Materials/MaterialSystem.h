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
     * @brief Reports the work recorded by a material render pass.
     */
    struct SMaterialRenderResult
    {
        /** @brief Number of materials prepared for the rendered submission. */
        uint32_t MaterialCount = 0;

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
        SMaterialRenderResult RenderFrame();

        /**
         * @brief Prepares and uploads material data for a scene submission.
         * @param scene Scene that provides material render items.
         * @param submissionSerial Serial that identifies the submission.
         */
        void PrepareFrame(const MaterialRenderScene& scene, uint64_t submissionSerial);

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
         * @param submissionSerial Serial passed to @ref PreparedFrame.
         * @return Counts of rendered batches and recorded draw commands.
         * @pre @ref PrepareFrame was called for @p submissionSerial.
         */
        SMaterialRenderResult Render(
            const Ref<CommandBuffer>& cmd,
            const MaterialRenderScene& scene,
            uint64_t submissionSerial
        );

        /**
         * @brief Resolves an instance into a render-ready material proxy.
         * @param instance Material instance to resolve.
         * @return The render proxy, or null when the instance cannot be resolved.
         */
        Ref<const MaterialRenderProxy> Resolve(
            const Ref<MaterialInstance>& instance
        ) override;

        /** @brief Gets the buffer that stores material data for the current frame slot. */
        const Ref<DynamicStorageBuffer>& GetFrameBuffer() const;

        /** @brief Gets the texture set used by material rendering. */
        const Ref<TextureSet>& GetTextureSet() const { return m_Textures.GetTextureSet(); }

        /** @brief Gets the sampler used by material textures. */
        const Ref<Sampler>& GetSampler() const { return m_Textures.GetSampler(); }

    private:
        // Builds and uploads the material table shared by one or more frame scenes.
        void PrepareScenes(std::span<const MaterialRenderScene> scenes, uint64_t submissionSerial);

        /** @brief Stores material resources that are safe to reuse with one frame slot. */
        struct SFrameSlot
        {
            Ref<DynamicStorageBuffer> Buffer;
            Ref<const FrameTable> Table;
            uint64_t FrameNumber = UINT64_MAX;
        };

        /** @brief Stores material data prepared for the current submission. */
        struct SPreparedFrame
        {
            Ref<const FrameTable> Table;
            uint32_t MaterialCount = 0;
            uint64_t SubmissionSerial = 0;
        };

        /** @brief Stores the latest render proxy resolved for one material instance. */
        struct SCachedMaterial
        {
            Ref<const MaterialRenderProxy> Proxy;
            uint32_t InstanceRevision = 0;
            uint32_t MaterialRevision = 0;
        };

        uint32_t m_MaterialCapacity = 0;
        std::array<SFrameSlot, GraphicsContext::FRAMES> m_FrameSlots;
        TextureRegistry m_Textures;
        Scope<Renderer> m_Renderer;
        std::unordered_map<const MaterialInstance*, SCachedMaterial> m_MaterialCache;
        std::vector<MaterialRenderScene> m_SubmittedScenes;
        const GraphicsContext* m_Context = nullptr;
        uint64_t m_CurrentFrameNumber = UINT64_MAX;
        SPreparedFrame m_PreparedFrame;
    };
}
