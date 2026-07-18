#pragma once

#include <Engine/Camera/Camera.h>
#include <Engine/Graphics/MeshPassProcessor.h>
#include <Engine/Graphics/MeshRenderScene.h>
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
    // Identifies one material slot inside one registered model. Slot indices are only
    // local to a model, so renderer caches must never use the slot alone as a key.
    struct SModelMaterialHandle
    {
        SModelRenderHandle Model;
        uint32_t Slot = 0;

        bool operator==(const SModelMaterialHandle&) const = default;
    };

    struct SModelMaterialHandleHash
    {
        size_t operator()(const SModelMaterialHandle& handle) const
        {
            size_t hash = SModelRenderHandleHash{}(handle.Model);
            hash ^= std::hash<uint32_t>{}(handle.Slot)
                + 0x9e3779b9u + (hash << 6) + (hash >> 2);
            return hash;
        }
    };

    // Renders glTF models with a Cook-Torrance metallic-roughness PBR base pass.
    class ELIXIR_API MeshRenderer
    {
      public:
        MeshRenderer(const GraphicsContext* context, const ShaderLoader* shaderLoader);

        // Registration is thread-safe. The render thread installs the model at the
        // next frame boundary as an immutable scene proxy.
        void RegisterModel(const Ref<Model>& model);
        // Publish a replacement proxy after geometry, transforms, or material
        // snapshots change. The active scene is swapped only at a frame boundary.
        void UpdateModel(const Ref<Model>& model);
        void Render(const Camera& camera);

        // Removal is queued and applied by Render at the next frame boundary. A model
        // released without an explicit unregister expires the proxy's lifetime token.
        void UnregisterModel(SModelRenderHandle handle);

        // Swap the built-in shading used by every registered model. The new shader
        // must expose the same bindings.
        void SetShader(const Ref<Shader>& shader);

        // Override one model's material slot. Pass a null shader to revert that slot
        // to the scene's default static-mesh shader.
        void SetMaterialShader(SModelRenderHandle model, uint32_t materialIndex,
            const Ref<Shader>& shader,
            EMaterialBlendMode blendMode = EMaterialBlendMode::Opaque);

        // Two-phase apply so the expensive pipeline creation (Metal compile on
        // MoltenVK) happens off the render thread. Render installs the prepared
        // shader and immutable parameter snapshot together at a frame boundary.
        void PrepareMaterialShader(SModelRenderHandle model, uint32_t materialIndex,
            const Ref<Shader>& shader,
            EMaterialBlendMode blendMode, const SMaterialShaderPermutation& permutation,
            const Ref<const MaterialRenderProxy>& proxy,
            size_t revision, size_t variantKey);
        [[nodiscard]] bool HasMaterialShaderVariant(
            SModelRenderHandle model, uint32_t materialIndex,
            const SMaterialShaderPermutation& permutation,
            size_t revision, size_t variantKey) const;
        void PrepareCachedMaterialShader(SModelRenderHandle model,
            uint32_t materialIndex,
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

        struct SModelRenderState
        {
            Ref<const MeshSceneProxy::PrimitiveList> PrimitiveSnapshot;
            MaterialGPUParameterCache Parameters;
            std::vector<SGPUMaterial> PackedMaterials;
            uint32_t MaterialOffset = 0;

            MaterialDrawCommandCache DrawCommandCache;
            std::vector<SMeshDrawCommand> DrawCommands;
            std::vector<uint32_t> BasePassCommands;
            std::vector<uint32_t> SortedPassCommands;
        };

        static constexpr uint32_t PUSH_CONSTANT_SIZE =
            sizeof(glm::mat4) + sizeof(uint32_t);
        static constexpr uint32_t NO_TEXTURE = 0xffffffffu;

        uint32_t ResolveTexture(const Ref<Texture>& texture);
        SGPUMaterial PackMaterial(const Ref<const MaterialRenderProxy>& material);
        Ref<StorageBuffer> CreateMaterialBuffer(const std::vector<SGPUMaterial>& materials);
        SMeshDrawCommand BuildDrawCommand(SModelRenderHandle model,
            uint32_t materialOffset, const SModelPrimitive& primitive,
            const MaterialGPUParameterCache::ProxyList& materialProxies) const;
        void UpdateDrawCommands(SModelRenderHandle handle,
            const MeshSceneProxy& proxy,
            const MaterialGPUParameterCache::ProxyList& materialProxies,
            SModelRenderState& renderState);
        void InvalidateDefaultDrawCommands();
        void InvalidateMaterialDrawCommands(SModelMaterialHandle material);

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
        std::unordered_map<SModelMaterialHandle, SShaderVariant,
            SModelMaterialHandleHash> m_MaterialShaders;

        // The permutations built for one slot's current graph revision, so toggling a
        // static parameter back is a swap rather than a rebuild. Bounded by the graph's
        // static parameter count: a new revision retires the whole generation.
        struct SVariantCache
        {
            size_t Revision = 0;
            std::unordered_map<size_t, SShaderVariant> Variants;
        };
        std::unordered_map<SModelMaterialHandle, SVariantCache,
            SModelMaterialHandleHash> m_MaterialShaderCache;

        // Release every permutation cached for a slot, and forget its keys.
        void RetireVariantCache(SModelMaterialHandle material);
        void RetireModelVariants(SModelRenderHandle model);

        // Worker-visible index of the above. The actual GPU objects remain
        // render-thread-owned; workers only use this to avoid recompilation.
        struct SVariantKeys
        {
            size_t Revision = 0;
            std::unordered_set<size_t> Keys;
        };
        mutable std::mutex m_VariantKeysMutex;
        std::unordered_map<SModelMaterialHandle, SVariantKeys,
            SModelMaterialHandleHash> m_VariantKeys;

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
        struct SRetiredModelState
        {
            SModelRenderState State;
            int FramesLeft;
        };
        std::vector<SRetiredModelState> m_RetiredModelStates;
        void RetireBuffer(Ref<Buffer> buffer);
        void Retire(SShaderVariant&& variant);
        void RetireModelState(SModelRenderHandle handle);
        void ProcessModelLifecycle();
        void TickRetired();

        MeshRenderScene m_RenderScene;

        // Variants built by a worker, waiting to be installed from Render.
        struct SPendingVariant
        {
            SModelRenderHandle Model;
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

        // Every registered model occupies one contiguous range. Draw commands store
        // the global index, so a single immutable descriptor can serve a frame-wide
        // pass even when transparent commands from different models interleave.
        std::vector<SGPUMaterial> m_PackedSceneMaterials;
        Ref<StorageBuffer> m_SceneMaterialBuffer;

        uint64_t m_NextDrawBindingRevision = 1;
        uint64_t m_DefaultDrawBindingRevision = 1;
        std::unordered_map<SModelMaterialHandle, uint64_t,
            SModelMaterialHandleHash> m_MaterialDrawBindingRevisions;
        std::unordered_map<SModelRenderHandle, SModelRenderState,
            SModelRenderHandleHash> m_ModelRenderStates;
    };
}
