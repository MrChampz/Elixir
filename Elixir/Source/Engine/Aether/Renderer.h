#pragma once

#include <Engine/Aether/System.h>
#include <Engine/Camera/Camera.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir::Aether
{
    struct SFrameData
    {
        glm::mat4 ViewProj;
    };

    class ELIXIR_API Renderer final
    {
      public:
        static constexpr size_t MAX_PARTICLES = 10000;

        Renderer(const GraphicsContext* context, const ShaderLoader* shaderLoader);

        void Render(const std::vector<SRenderParticle>& renderables, const Camera& camera);

      private:
        void InitRenderPass(const ShaderLoader* shaderLoader);
        void InitPerFrameData();
        void BindShaderParameters() const;

        void BeginRendering(const Ref<CommandBuffer>& cmd) const;
        void EndRendering(const Ref<CommandBuffer>& cmd) const;

        SFrameData m_FrameData{};
        Ref<UniformBuffer> m_FrameConstantBuffer;

        struct SVertex
        {
            glm::vec2 Position{};
            glm::vec4 Color{};
            float Size = 1.0f;
        };

        std::vector<SVertex> m_Vertices;

        Ref<Shader> m_Shader;
        Ref<GraphicsPipeline> m_Pipeline;
        Ref<DynamicVertexBuffer> m_VertexBuffer;

        Extent2D m_RenderExtent{};
        const GraphicsContext* m_GraphicsContext;
    };
}