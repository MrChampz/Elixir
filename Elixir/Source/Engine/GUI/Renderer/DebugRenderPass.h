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

        void BeginFrame() override;
        void EndFrame() override;

        uint32_t AppendRange(std::span<const SDrawCommand> commands) override;

        void Bind(const Ref<CommandBuffer>& cmd) override;
        void Render(
            const Ref<CommandBuffer>& cmd,
            uint32_t firstInstance,
            uint32_t instanceCount
        ) override;

        bool HasData() const override;
        void Clear() override;

        uint32_t GetInstanceCount() const override;

        EDrawCommandType GetHandleType() const override;

      private:
        void InitRenderPass(const ShaderLoader* shaderLoader);
        void BindShaderParameters() const;
        void EnsureVertexBufferCapacity(size_t requiredCapacity);

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
        std::vector<Ref<DynamicVertexBuffer>> m_RetiredVertexBuffers;

        float m_DPIScale;
        Ref<UniformBuffer> m_PerFrameConstantBuffer;
        const GraphicsContext* m_GraphicsContext;
        size_t m_VertexCapacity = 0;
    };
}
