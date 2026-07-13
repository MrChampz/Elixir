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

      private:
        struct alignas(16) SFrameData
        {
            glm::mat4 View;
            glm::mat4 Proj;
            glm::mat4 ViewProj;
            glm::vec3 CameraPos;
            float _Padding = 0.0f;
            uint32_t EnvIndex = 0xffffffffu;
            uint32_t IrradianceIndex = 0xffffffffu;
            float EnvIntensity = 1.0f;
            float _Padding2 = 0.0f;
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

        std::unordered_map<const Model*, Ref<StorageBuffer>> m_MaterialBuffers;
        const Model* m_BoundModel = nullptr;
    };
}
