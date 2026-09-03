#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/Pipeline/Pipeline.h>
#include <Engine/Materials/Compilation/CompilationCache.h>
#include <Engine/Materials/Rendering/MaterialRenderProxy.h>
#include <Engine/Materials/Rendering/TextureRegistry.h>

namespace Elixir { class ShaderLoader; }

namespace Elixir::Materials::Rendering
{
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
     * @brief Compiles material instances and prepares their render passes.
     *
     * The renderer caches compiled materials, descriptor bindings, and graphics
     * pipelines for reuse across render items.
     */
    class ELIXIR_API Renderer final
    {
    public:
        /**
         * @brief Creates a material renderer.
         * @param context Graphics context used to create pipelines.
         * @param textures Registry that provides material textures and a sampler.
         * @param shaderLoader Loader used to compile material shaders.
         * @pre All arguments are valid for the renderer lifetime.
         */
        Renderer(
            const GraphicsContext* context,
            const TextureRegistry& textures,
            const ShaderLoader* shaderLoader
        );

        /**
         * @brief Prepares a shader and graphics pipeline for a material pass.
         * @param request Pass requirements and external resources.
         * @return Prepared pass, or no value if the request is invalid or unsupported.
         * @pre `request.Material` is not null.
         * @pre `request.Pipeline.VertexLayout` is not null.
         */
        std::optional<SPreparedPass> Prepare(const SPassRequest& request);

        /**
         * @brief Resolves an instance into render-ready material data.
         * @param instance Material instance to resolve.
         * @return Render proxy, or null if the instance cannot be compiled.
         */
        Ref<const MaterialRenderProxy> Resolve(const Ref<MaterialInstance>& instance);

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

        /** Returns a cached pipeline or creates one for the request. */
        Ref<GraphicsPipeline> GetPipeline(
            EMaterialPass pass,
            const Ref<Shader>& shader,
            const SPipelineRequest& request
        );

        /** Binds and validates the descriptor resources for a shader. */
        bool BindDescriptorResources(const Ref<Shader>& shader, const SPassRequest& request);

        const TextureRegistry& m_Textures;
        std::unordered_map<SPipelineKey, Ref<GraphicsPipeline>, SPipelineKeyHasher> m_Pipelines;
        std::unordered_map<const Shader*, SDescriptorBindingState> m_DescriptorBindings;
        CompilationCache m_CompilationCache;

        const GraphicsContext* m_Context = nullptr;
    };
}
