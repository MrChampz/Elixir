#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/FrameSlotState.h>
#include <Engine/Graphics/Pipeline/Pipeline.h>
#include <Engine/Materials/Rendering/FrameTable.h>
#include <Engine/Materials/Rendering/MaterialRenderProxy.h>
#include <Engine/Materials/Rendering/TextureRegistry.h>

namespace Elixir { class ShaderLoader; }
namespace Elixir::Materials { struct SMaterialSystemConfig; }

namespace Elixir::Materials::Rendering
{
    class MaterialRenderScene;
    struct SRenderItem;

    /**
     * @brief Identifies a material render pass.
     */
    enum class EMaterialPass : uint8_t
    {
        ParticleSprite,
        ParticleRibbon,
        ParticleMesh,
    };

    /**
     * @brief Identifies a compiled material program.
     *
     * The identity is suitable for grouping render items that use the same shader.
     */
    struct SProgramKey
    {
        /** Opaque identity of the compiled shader program. */
        const void* Identity = nullptr;

        /** @brief Checks whether this key identifies a program. */
        explicit operator bool() const { return Identity != nullptr; }

        /** @brief Compares two program keys. */
        bool operator==(const SProgramKey&) const = default;
    };

    /**
     * @brief Associates a scene draw with resolved material data.
     */
    struct SResolvedRenderItem
    {
        /** Source draw item owned by the render scene. */
        const SRenderItem* Item = nullptr;

        /** Immutable material data resolved by MaterialSystem. */
        Ref<const MaterialRenderProxy> Proxy;
    };

    /** @brief Stores material proxies resolved for one render scene. */
    struct SPreparedScene
    {
        /** Scene that owns geometry and source draw items. */
        const MaterialRenderScene* Scene = nullptr;

        /** Draw items with their already resolved material proxies. */
        std::vector<SResolvedRenderItem> Items;
    };

    /**
     * @brief Reports the work recorded by a material renderer.
     */
    struct SRenderResult
    {
        /** Number of materials uploaded for the rendered scene. */
        uint32_t MaterialCount = 0;

        /** Number of material batches rendered. */
        uint32_t BatchCount = 0;

        /** Number of draw commands recorded. */
        uint32_t DrawCount = 0;
    };

    /**
     * @brief Describes the vertex input layout for a material pipeline.
     */
    struct SPipelineRequest
    {
        /** Stable key that identifies the vertex layout. */
        uint64_t VertexLayoutKey = 0;

        /** Vertex layout used to create the graphics pipeline. */
        const BufferLayout* VertexLayout = nullptr;
    };

    /**
     * @brief Associates a constant buffer with a shader binding name.
     */
    struct SConstantBufferBinding
    {
        /** Shader binding name. */
        std::string_view Name;

        /** Constant buffer to bind. */
        Ref<UniformBuffer> Buffer;
    };

    /** @brief Holds a storage-buffer type supported by material passes. */
    using MaterialStorageBuffer = std::variant<Ref<StorageBuffer>, Ref<DynamicStorageBuffer>>;

    /**
     * @brief Associates a storage buffer with a shader binding name.
     */
    struct SStorageBufferBinding
    {
        /** Shader binding name. */
        std::string_view Name;

        /** Storage buffer to bind. */
        MaterialStorageBuffer Buffer;
    };

    /**
     * @brief Groups external buffers required by a material pass.
     */
    struct SExternalResources
    {
        std::span<const SConstantBufferBinding> ConstantBuffers;
        std::span<const SStorageBufferBinding> StorageBuffers;

        /**
         * @brief Returns the number of external resource bindings.
         * @return Total number of constant and storage buffer bindings.
         */
        uint32_t GetResourceCount() const
        {
            return (uint32_t)(ConstantBuffers.size() + StorageBuffers.size());
        }
    };

    /**
     * @brief Describes the resources required to prepare one material pass.
     */
    struct SPassRequest
    {
        /** Material pass to prepare. */
        EMaterialPass Pass = EMaterialPass::ParticleSprite;

        /** Resolved material data for the pass. */
        const MaterialRenderProxy* Material = nullptr;

        /** Pipeline requirements for the pass. */
        SPipelineRequest Pipeline;

        /** External buffers required by the pass. */
        SExternalResources ExternalResources;

        /** Per-frame buffer that stores resolved material data. */
        Ref<DynamicStorageBuffer> MaterialBuffer;

        /** Push constants applied before the first draw. */
        std::span<const std::byte> InitialPushConstants;
    };

    /**
     * @brief Stores a prepared shader and graphics pipeline.
     */
    struct SPreparedPass
    {
        /** Shader prepared for the material pass. */
        Ref<Shader> Shader;

        /** Graphics pipeline prepared for the material pass. */
        Ref<GraphicsPipeline> Pipeline;

        /** @brief Checks whether the shader and pipeline are available. */
        explicit operator bool() const { return Shader && Pipeline; }
    };

    /**
     * @brief Owns frame material resources and records material draw commands.
     *
     * The renderer owns texture bindings, per-frame material buffers, descriptor
     * bindings, graphics pipelines, and secondary command buffers.
     */
    class ELIXIR_API Renderer final
    {
    public:
        /**
         * @brief Creates a material renderer.
         * @param context Graphics context used to create pipelines.
         * @param materialCapacity Maximum unique materials supported by one scene.
         * @pre All arguments are valid for the renderer lifetime.
         */
        Renderer(
            const GraphicsContext* context,
            uint32_t materialCapacity
        );

        /** @brief Selects resources for the current graphics frame. */
        void BeginFrame();

        /**
         * @brief Records all resolved material scenes for the current frame.
         * @param scenes Scenes with material proxies resolved by MaterialSystem.
         * @return Counts of materials, batches, and draw commands recorded.
         */
        SRenderResult RenderFrame(std::span<const SPreparedScene> scenes);

        /**
         * @brief Returns the program key for a material pass.
         * @param pass Material pass.
         * @param material Resolved material data.
         * @return Program key, or no value if the material does not support the pass.
         */
        static std::optional<SProgramKey> GetProgramKey(
            EMaterialPass pass,
            const MaterialRenderProxy& material
        );

        /**
         * @brief Returns the material usage required by a render pass.
         * @param pass Material pass.
         * @return Corresponding material usage.
         */
        static EMaterialUsage GetUsage(EMaterialPass pass);

        /**
         * @brief Returns the draw order for a material pass.
         *
         * Lower values are rendered first.
         *
         * @param pass Material pass.
         * @return Render-order value for the pass.
         */
        static uint32_t GetPassOrder(EMaterialPass pass);

    private:
        /** Identifies the type of a cached descriptor binding. */
        enum class EDescriptorBindingType : uint8_t
        {
            ConstantBuffer,
            StorageBuffer,
            DynamicStorageBuffer,
        };

        /** Describes one descriptor binding used by a shader. */
        struct SDescriptorBinding
        {
            std::string Name;
            const void* Resource = nullptr;
            EDescriptorBindingType Type = EDescriptorBindingType::ConstantBuffer;

            bool operator==(const SDescriptorBinding&) const = default;
        };

        /** Stores the descriptor bindings established for a shader. */
        struct SDescriptorBindingState
        {
            EMaterialPass Pass = EMaterialPass::ParticleSprite;
            std::vector<SDescriptorBinding> ExternalResources;

            bool operator==(const SDescriptorBindingState&) const = default;
        };

        /** Identifies a cached graphics pipeline. */
        struct SPipelineKey
        {
            EMaterialPass Pass = EMaterialPass::ParticleSprite;
            const Shader* Shader = nullptr;
            uint64_t VertexLayoutKey = 0;

            bool operator==(const SPipelineKey&) const = default;
        };

        /** Hashes a graphics pipeline key. */
        struct SPipelineKeyHasher
        {
            size_t operator()(const SPipelineKey& key) const
            {
                size_t hash = Hash::Hash<uint32_t>(static_cast<uint32_t>(key.Pass));
                Hash::HashCombine(hash, Hash::Hash<const Shader*>(key.Shader));
                Hash::HashCombine(hash, Hash::Hash<uint64_t>(key.VertexLayoutKey));
                return hash;
            }
        };

        /** Stores data needed to record one material draw. */
        struct SPreparedRenderItem
        {
            const SRenderItem* Item = nullptr;
            Ref<const MaterialRenderProxy> Proxy;
            uint32_t MaterialIndex = UINT32_MAX;
        };

        /** Stores frame-buffer indices assigned to one render scene. */
        struct SPreparedRenderScene
        {
            const MaterialRenderScene* Scene = nullptr;
            std::vector<SPreparedRenderItem> Items;
            uint32_t MaterialCount = 0;
        };

        /** Stores resources that are safe to reuse for one graphics frame slot. */
        struct SFrameSlot
        {
            Ref<DynamicStorageBuffer> MaterialBuffer;
        };

        struct SBatchKey
        {
            EMaterialPass Pass = EMaterialPass::ParticleSprite;
            uint32_t GeometryIndex = UINT32_MAX;
            SProgramKey Program;

            bool operator==(const SBatchKey&) const = default;
        };

        struct SBatch
        {
            SBatchKey Key;
            std::vector<const SPreparedRenderItem*> Items;
        };

        /** Builds and uploads the material table for one resolved scene. */
        SPreparedRenderScene PrepareScene(const SPreparedScene& scene);

        /** Records draw commands for a scene with prepared material indices. */
        SRenderResult RecordScene(
            const Ref<CommandBuffer>& cmd,
            const SPreparedRenderScene& scene
        );

        /** Prepares a shader and graphics pipeline for a material pass. */
        std::optional<SPreparedPass> PreparePass(const SPassRequest& request);

        /** Returns the buffer for the active graphics frame slot. */
        const Ref<DynamicStorageBuffer>& GetActiveMaterialBuffer() const;

        /** Returns a cached pipeline or creates one for the request. */
        Ref<GraphicsPipeline> GetPipeline(
            EMaterialPass pass,
            const Ref<Shader>& shader,
            const SPipelineRequest& request
        );

        /** Binds and validates the descriptor resources for a shader. */
        bool BindDescriptorResources(const Ref<Shader>& shader, const SPassRequest& request);

        uint32_t m_MaterialCapacity = 0;
        FrameSlotState<SFrameSlot> m_FrameSlots;
        TextureRegistry m_Textures;
        std::unordered_map<SPipelineKey, Ref<GraphicsPipeline>, SPipelineKeyHasher> m_Pipelines;
        std::unordered_map<const Shader*, SDescriptorBindingState> m_DescriptorBindings;

        uint64_t m_CurrentFrameNumber = UINT64_MAX;
        const GraphicsContext* m_Context = nullptr;
    };
}
