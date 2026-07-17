#pragma once

#include <Engine/Camera/Camera.h>
#include <Engine/Graphics/Model.h>
#include <Engine/Graphics/Material/MaterialDrawCommandCache.h>
#include <Engine/Graphics/Material/MaterialGPUParameterCache.h>
#include <Engine/Graphics/Material/MaterialGraph.h>
#include <Engine/Graphics/Material/MaterialShaderPermutation.h>

#include <mutex>
#include <Engine/Graphics/Environment.h>
#include <Engine/Graphics/TextureSet.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Graphics/Pipeline/Pipeline.h>
#include <unordered_set>

namespace Elixir
{
    // Renders glTF models with a Cook-Torrance metallic-roughness PBR base pass.
    class ELIXIR_API MeshRenderer
    {
      public:
        MeshRenderer(const GraphicsContext* context, const ShaderLoader* shaderLoader);

        void Render(const Ref<Model>& model, const Camera& camera);

        // Swap the shading used for the whole model (e.g. a node-graph material
        // compiled at runtime). The new shader must expose the same bindings.
        void SetShader(const Ref<Shader>& shader);

        // Override the shading of a single material slot (a node-graph material for
        // just that part). Pass a null shader to revert the slot to the default.
        void SetMaterialShader(uint32_t materialIndex, const Ref<Shader>& shader,
            EMaterialBlendMode blendMode = EMaterialBlendMode::Opaque);

        // Two-phase apply so the expensive pipeline creation (Metal compile on
        // MoltenVK) happens off the render thread. Render installs the prepared
        // shader and immutable parameter snapshot together at a frame boundary.
        void PrepareMaterialShader(uint32_t materialIndex, const Ref<Shader>& shader,
            EMaterialBlendMode blendMode, const SMaterialShaderPermutation& permutation,
            const Ref<const MaterialRenderProxy>& proxy,
            size_t revision, size_t variantKey);
        [[nodiscard]] bool HasMaterialShaderVariant(
            uint32_t materialIndex, const SMaterialShaderPermutation& permutation,
            size_t revision, size_t variantKey) const;
        void PrepareCachedMaterialShader(uint32_t materialIndex,
            const SMaterialShaderPermutation& permutation, size_t revision, size_t variantKey,
            const Ref<const MaterialRenderProxy>& proxy);
        void InstallPendingShaders();

      private:
        void CreatePipelines();
        void CreatePipelinesFor(const Ref<Shader>& shader, Ref<GraphicsPipeline>& opaque, Ref<GraphicsPipeline>& transparent,
            EMaterialBlendMode blendMode = EMaterialBlendMode::Translucent);
        void BindResources();
        void BindResourcesTo(const Ref<Shader>& shader);

        struct alignas(16) SFrameData
        {
            glm::mat4 View;
            glm::mat4 Proj;
            glm::mat4 ViewProj;
            glm::vec3 CameraPos;
            float Time = 0.0f; // seconds since start, drives animated graph materials
            uint32_t EnvIndex = 0xffffffffu;
            uint32_t IrradianceIndex = 0xffffffffu;
            float EnvIntensity = 1.0f;
            float EnvMaxLod = 0.0f;
            uint32_t PrefIndex = 0xffffffffu;
            uint32_t SceneColorIndex = 0xffffffffu; // grabbed scene colour for refraction
            float ScreenWidth = 1.0f;
            float ScreenHeight = 1.0f;
            glm::vec4 LightDirection = glm::vec4(0.0f); // xyz = direction TO the light
            glm::vec4 LightColor = glm::vec4(0.0f);     // rgb = color, w = intensity
        };

        struct SModelPushConstants
        {
            glm::mat4 Model;
            uint32_t MaterialIndex;
        };

        // std430-packed material, mirrored by the shader's StructuredBuffer.
        struct SGPUMaterial
        {
            glm::vec4 BaseColorFactor;
            glm::vec4 EmissiveMetallic;     // xyz = emissive, w = metallic
            glm::vec4 RoughOccNormalCutoff; // x = roughness, y = occlusion, z = normalScale, w = alphaCutoff
            glm::uvec4 TexIndex0;           // baseColor, metallicRoughness, normal, emissive
            glm::uvec4 TexIndex1;           // occlusion, alphaMode, unused...
            glm::vec4 BaseColorTransform;   // uv scale.xy, offset.zw
            glm::vec4 MetallicRoughnessTransform;
            glm::vec4 NormalTransform;
            glm::vec4 EmissiveTransform;
            glm::vec4 OcclusionTransform;
            glm::vec4 Clearcoat;            // x = factor, y = roughness
            glm::vec4 Specular;            // rgb = specular color factor, w = specular factor
        };

        // Immutable portion of one indexed draw. Camera data and transparent order
        // remain dynamic, while resource selection and geometry bindings are rebuilt
        // only when the corresponding material slot changes.
        struct SMaterialDrawCommand
        {
            Ref<Shader> Shader;
            Ref<GraphicsPipeline> Pipeline;
            Ref<VertexBuffer> Vertices;
            Ref<IndexBuffer> Indices;
            SModelPushConstants PushConstants;
            glm::vec3 SortOrigin{ 0.0f };
            uint32_t IndexCount = 0;
            bool BlendPass = false;
        };

        struct SModelRenderState
        {
            MaterialGPUParameterCache Parameters;
            std::vector<SGPUMaterial> PackedMaterials;
            Ref<StorageBuffer> Buffer;

            MaterialDrawCommandCache DrawCommandCache;
            std::vector<SMaterialDrawCommand> DrawCommands;
            std::vector<uint32_t> BasePassCommands;
            std::vector<uint32_t> BlendPassCommands;
            std::vector<uint32_t> SortedBlendPassCommands;
        };

        static constexpr uint32_t PUSH_CONSTANT_SIZE =
            sizeof(glm::mat4) + sizeof(uint32_t);
        static constexpr uint32_t NO_TEXTURE = 0xffffffffu;

        uint32_t ResolveTexture(const Ref<Texture>& texture);
        SGPUMaterial PackMaterial(const Ref<const MaterialRenderProxy>& material);
        Ref<StorageBuffer> CreateMaterialBuffer(const std::vector<SGPUMaterial>& materials);
        SMaterialDrawCommand BuildDrawCommand(const SModelPrimitive& primitive,
            const MaterialGPUParameterCache::ProxyList& materialProxies) const;
        void UpdateDrawCommands(const Ref<Model>& model,
            const MaterialGPUParameterCache::ProxyList& materialProxies,
            SModelRenderState& renderState);
        void InvalidateDefaultDrawCommands();
        void InvalidateMaterialDrawCommands(uint32_t materialIndex);

        const GraphicsContext* m_GraphicsContext;

        Ref<Shader> m_Shader;
        Ref<GraphicsPipeline> m_Pipeline;            // opaque + masked (depth write)
        Ref<GraphicsPipeline> m_TransparentPipeline; // blend (alpha, no depth write)

        // Per-material shader overrides (e.g. a node graph applied to one slot).
        static constexpr uint32_t MAX_GRAPH_PARAMS = 8;
        struct SShaderVariant
        {
            Ref<Shader> Shader;
            Ref<GraphicsPipeline> Opaque;
            Ref<GraphicsPipeline> Transparent;
            Ref<UniformBuffer> ParamBuffer; // cbGraphParams (exposed params)
            EMaterialBlendMode BlendMode = EMaterialBlendMode::Opaque;
            SMaterialShaderPermutation Permutation;
            size_t Revision = 0;
            size_t VariantKey = 0;
            Ref<const MaterialRenderProxy> Proxy;
            Ref<const MaterialRenderProxy> UploadedProxy;
        };
        std::unordered_map<uint32_t, SShaderVariant> m_MaterialShaders;

        // The permutations built for one slot's current graph revision, so toggling a
        // static parameter back is a swap rather than a rebuild. Bounded by the graph's
        // static parameter count: a new revision retires the whole generation.
        struct SVariantCache
        {
            size_t Revision = 0;
            std::unordered_map<size_t, SShaderVariant> Variants;
        };
        std::unordered_map<uint32_t, SVariantCache> m_MaterialShaderCache;

        // Release every permutation cached for a slot, and forget its keys.
        void RetireVariantCache(uint32_t materialIndex);

        // Worker-visible index of the above. The actual GPU objects remain
        // render-thread-owned; workers only use this to avoid recompilation.
        struct SVariantKeys
        {
            size_t Revision = 0;
            std::unordered_set<size_t> Keys;
        };
        mutable std::mutex m_VariantKeysMutex;
        std::unordered_map<uint32_t, SVariantKeys> m_VariantKeys;

        // Deferred destruction: a swapped-out shader/pipeline may still be referenced
        // by in-flight frames, so keep it alive a few frames before releasing it --
        // avoids a WaitDeviceIdle stall on every shader swap.
        static constexpr int RETIRE_FRAMES = 4; // > frames in flight
        struct SRetired
        {
            SShaderVariant Variant;
            int FramesLeft;
        };
        std::vector<SRetired> m_Retired;
        struct SRetiredBuffer
        {
            Ref<Buffer> Buffer;
            int FramesLeft;
        };
        std::vector<SRetiredBuffer> m_RetiredBuffers;
        void RetireBuffer(Ref<Buffer> buffer);
        void Retire(SShaderVariant&& variant);
        void TickRetired();

        // Variants built by a worker, waiting to be installed from Render.
        struct SPendingVariant
        {
            uint32_t Slot;
            SShaderVariant Variant;
            Ref<const MaterialRenderProxy> Proxy;
            SMaterialShaderPermutation Permutation;
            size_t Revision = 0;
            size_t VariantKey = 0;
            bool UseCached = false;
        };
        std::vector<SPendingVariant> m_PendingVariants;
        std::mutex m_PendingMutex;

        Ref<UniformBuffer> m_FrameBuffer;
        SFrameData m_FrameData{};

        Ref<Sampler> m_Sampler;
        Ref<Sampler> m_SamplerClamp;
        Ref<Sampler> m_SamplerPoint;
        Ref<Sampler> m_SamplerPointClamp;
        Ref<TextureSet> m_Textures;
        std::unordered_map<Ref<Texture>, uint32_t> m_TextureIndices;

        Ref<Environment> m_Environment;
        uint32_t m_EnvIndex = 0xffffffffu;
        uint32_t m_IrradianceIndex = 0xffffffffu;
        uint32_t m_PrefIndex = 0xffffffffu;
        float m_EnvMaxLod = 0.0f;

        // Grabbed scene colour (opaque pass) that refractive glass samples.
        Ref<Texture2D> m_SceneColor;
        uint32_t m_SceneColorIndex = 0xffffffffu;
        Extent3D m_SceneColorExtent{};

        uint64_t m_NextDrawBindingRevision = 1;
        uint64_t m_DefaultDrawBindingRevision = 1;
        std::unordered_map<uint32_t, uint64_t> m_MaterialDrawBindingRevisions;
        std::unordered_map<const Model*, SModelRenderState> m_ModelRenderStates;
        const Model* m_BoundModel = nullptr;
    };
}
