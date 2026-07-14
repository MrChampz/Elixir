#pragma once

#include <Engine/Camera/Camera.h>
#include <Engine/Graphics/Model.h>
#include <Engine/Graphics/Environment.h>
#include <Engine/Graphics/TextureSet.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Graphics/Pipeline/Pipeline.h>

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

      private:
        void CreatePipelines();
        void BindResources();

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

        static constexpr uint32_t PUSH_CONSTANT_SIZE =
            sizeof(glm::mat4) + sizeof(uint32_t);
        static constexpr uint32_t NO_TEXTURE = 0xffffffffu;

        uint32_t ResolveTexture(const Ref<Texture>& texture);
        Ref<StorageBuffer> BuildMaterialBuffer(const Ref<Model>& model);

        const GraphicsContext* m_GraphicsContext;

        Ref<Shader> m_Shader;
        Ref<GraphicsPipeline> m_Pipeline;            // opaque + masked (depth write)
        Ref<GraphicsPipeline> m_TransparentPipeline; // blend (alpha, no depth write)

        Ref<UniformBuffer> m_FrameBuffer;
        SFrameData m_FrameData{};

        Ref<Sampler> m_Sampler;
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

        std::unordered_map<const Model*, Ref<StorageBuffer>> m_MaterialBuffers;
        const Model* m_BoundModel = nullptr;
    };
}
