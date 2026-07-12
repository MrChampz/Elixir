#pragma once

#include <Engine/Camera/Camera.h>
#include <Engine/Graphics/Model.h>
#include <Engine/Graphics/TextureSet.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Graphics/Pipeline/Pipeline.h>

namespace Elixir
{
    // Renders glTF models with simple directional lighting + base-color texture.
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
        };

        struct SModelPushConstants
        {
            glm::mat4 Model;
            glm::vec4 BaseColorFactor;
            uint32_t TextureIndex;
        };

        // The bytes actually consumed by the shader's push-constant block; the
        // struct's own sizeof may be padded larger.
        static constexpr uint32_t PUSH_CONSTANT_SIZE =
            sizeof(glm::mat4) + sizeof(glm::vec4) + sizeof(uint32_t);

        uint32_t ResolveTexture(const Ref<Texture>& texture);

        const GraphicsContext* m_GraphicsContext;

        Ref<Shader> m_Shader;
        Ref<GraphicsPipeline> m_Pipeline;

        Ref<UniformBuffer> m_FrameBuffer;
        SFrameData m_FrameData{};

        Ref<Sampler> m_Sampler;
        Ref<TextureSet> m_Textures;
        std::unordered_map<Ref<Texture>, uint32_t> m_TextureIndices;
    };
}
