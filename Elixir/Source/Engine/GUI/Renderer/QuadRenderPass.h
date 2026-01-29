#pragma once

#include <Engine/GUI/Renderer/RenderBatch.h>
#include <Engine/GUI/Renderer/RenderPass.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir::GUI
{
    class QuadRenderPass final : public RenderPass
    {
      public:
        static constexpr size_t MAX_QUADS = 10000;

        QuadRenderPass(
            const GraphicsContext* context,
            const ShaderLoader* shaderLoader,
            const Ref<UniformBuffer>& perFrameCB
        );

        void GenerateDrawCommands(const RenderBatch& batch) override;
        void Render(const Ref<CommandBuffer>& cmd) override;
        bool HasData() const override;
        void Clear() override;

      private:
        void InitRenderPass(const ShaderLoader* shaderLoader);
        void BindShaderParameters() const;

        void BuildRectGeometry(const SDrawCommand& cmd);

        struct SQuad
        {
            glm::vec2 Position;
            glm::vec2 Size;

            // top-left, top-right, bottom-right, bottom-left
            glm::vec4 CornerRadius;

            SColor Color;
        };

        std::vector<SQuad> m_Quads;

        Ref<Shader> m_Shader;
        Ref<GraphicsPipeline> m_Pipeline;
        Ref<DynamicVertexBuffer> m_QuadBuffer;

        Ref<Texture2D> m_WhiteTexture;

        Ref<UniformBuffer> m_PerFrameConstantBuffer;
        const GraphicsContext* m_GraphicsContext;
    };
}