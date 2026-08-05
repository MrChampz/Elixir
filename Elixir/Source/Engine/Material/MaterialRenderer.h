#pragma once

#include <variant>

#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/Pipeline/Pipeline.h>
#include <Engine/Material/MaterialRenderProxy.h>
#include <Engine/Material/MaterialTextureRegistry.h>
#include <Engine/Material/MaterialCompilationCache.h>

namespace Elixir
{
    class ShaderLoader;

    enum class EMaterialPass : uint8_t
    {
        ParticleSprite,
        ParticleRibbon,
        ParticleMesh,
    };

    struct SMaterialProgramKey
    {
        const void* Identity = nullptr;

        explicit operator bool() const { return Identity != nullptr; }
        bool operator==(const SMaterialProgramKey&) const = default;
    };

    struct SMaterialPipelineRequest
    {
        uint64_t VertexLayoutKey = 0;
        const BufferLayout* VertexLayout = nullptr;
    };

    struct SMaterialConstantBufferBinding
    {
        std::string_view Name;
        Ref<UniformBuffer> Buffer;
    };

    using MaterialStorageBuffer = std::variant<Ref<StorageBuffer>, Ref<DynamicStorageBuffer>>;

    struct SMaterialStorageBufferBinding
    {
        std::string_view Name;
        MaterialStorageBuffer Buffer;
    };

    struct SMaterialExternalResources
    {
        std::span<const SMaterialConstantBufferBinding> ConstantBuffers;
        std::span<const SMaterialStorageBufferBinding> StorageBuffers;

        uint32_t GetResourceCount() const
        {
            return (uint32_t)(ConstantBuffers.size() + StorageBuffers.size());
        }
    };

    struct SMaterialPassRequest
    {
        EMaterialPass Pass = EMaterialPass::ParticleSprite;
        const MaterialRenderProxy* Material = nullptr;
        SMaterialPipelineRequest Pipeline;
        SMaterialExternalResources ExternalResources;
        std::span<const std::byte> InitialPushConstants;
    };

    struct SPreparedMaterialPass
    {
        Ref<Shader> Shader;
        Ref<GraphicsPipeline> Pipeline;

        explicit operator bool() const { return Shader && Pipeline; }
    };

    class ELIXIR_API MaterialRenderer final
    {
    public:
        MaterialRenderer(
            const GraphicsContext* context,
            Ref<DynamicStorageBuffer> frameBuffer,
            const MaterialTextureRegistry& textures,
            const ShaderLoader* shaderLoader
        );

        std::optional<SPreparedMaterialPass> Prepare(
            const SMaterialPassRequest& request
        );

        Ref<const MaterialRenderProxy> Resolve(const Ref<MaterialInstance>& instance);

        static std::optional<SMaterialProgramKey> GetProgramKey(
            EMaterialPass pass,
            const MaterialRenderProxy& material
        );

        static EMaterialUsage GetUsage(EMaterialPass pass);
        static uint32_t GetPassOrder(EMaterialPass pass);

    private:
        enum class EDescriptorBindingType : uint8_t
        {
            ConstantBuffer,
            StorageBuffer,
            DynamicStorageBuffer,
        };

        struct SDescriptorBinding
        {
            std::string Name;
            const void* Resource = nullptr;
            EDescriptorBindingType Type = EDescriptorBindingType::ConstantBuffer;

            bool operator==(const SDescriptorBinding&) const = default;
        };

        struct SDescriptorBindingState
        {
            EMaterialPass Pass = EMaterialPass::ParticleSprite;
            std::vector<SDescriptorBinding> ExternalResources;

            bool operator==(const SDescriptorBindingState&) const = default;
        };

        struct SPipelineKey
        {
            EMaterialPass Pass = EMaterialPass::ParticleSprite;
            const Shader* Shader = nullptr;
            uint64_t VertexLayoutKey = 0;

            bool operator==(const SPipelineKey&) const = default;
        };

        struct SPipelineKeyHasher
        {
            size_t operator()(const SPipelineKey& key) const
            {
                size_t hash = std::hash<uint32_t>{}(uint32_t(key.Pass));
                hash ^= std::hash<const Shader*>{}(key.Shader) +
                    0x9e3779b9 + (hash << 6) + (hash >> 2);
                hash ^= std::hash<uint64_t>{}(key.VertexLayoutKey) +
                    0x9e3779b9 + (hash << 6) + (hash >> 2);
                return hash;
            }
        };

        Ref<GraphicsPipeline> GetPipeline(
            EMaterialPass pass,
            const Ref<Shader>& shader,
            const SMaterialPipelineRequest& request
        );

        bool BindDescriptorResources(
            const Ref<Shader>& shader,
            const SMaterialPassRequest& request
        );

        Ref<DynamicStorageBuffer> m_FrameBuffer;
        const MaterialTextureRegistry& m_Textures;
        std::unordered_map<SPipelineKey, Ref<GraphicsPipeline>, SPipelineKeyHasher> m_Pipelines;
        std::unordered_map<const Shader*, SDescriptorBindingState> m_DescriptorBindings;
        MaterialCompilationCache m_CompilationCache;

        const GraphicsContext* m_Context = nullptr;
    };
}
