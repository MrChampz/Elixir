#pragma once

#include <Engine/GUI/Renderer/RenderBatch.h>
#include <Engine/GUI/Renderer/RenderPass.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir::GUI
{
    class DebugRenderPass final : public RenderPass
    {
      public:
        static constexpr size_t MAX_LINES = 10000;

        DebugRenderPass(
            const GraphicsContext* context,
            const ShaderLoader* shaderLoader,
            float dpiScale,
            const Ref<UniformBuffer>& perFrameCB
        );

        void GenerateDrawCommands(const RenderBatch& batch) override;
        void Render(const Ref<CommandBuffer>& cmd) override;
        bool HasData() const override;
        void Clear() override;

      private:
        void InitRenderPass(const ShaderLoader* shaderLoader);
        void BindShaderParameters() const;

        void BuildDebugRectGeometry(const SDrawCommand& cmd);

        struct SVertex
        {
            glm::vec2 Position;
            SColor    Color;
        };

        std::vector<SVertex> m_Vertices;

        Ref<Shader> m_Shader;
        Ref<GraphicsPipeline> m_Pipeline;
        Ref<DynamicVertexBuffer> m_VertexBuffer;

        float m_DPIScale;
        Ref<UniformBuffer> m_PerFrameConstantBuffer;
        const GraphicsContext* m_GraphicsContext;
    };
}