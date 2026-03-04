#pragma once

#include <Engine/GUI/Renderer/RenderBatch.h>
#include <Engine/GUI/Renderer/RenderPass.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir::GUI
{
    class ELIXIR_API TextRenderPass final : public RenderPass
    {
      public:
        static constexpr size_t MAX_CHARACTERS = 16000;

        TextRenderPass(
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

        void BuildTextGeometry(const SDrawCommand& cmd);
        void BuildTextureGeometry(const SDrawCommand& cmd);

        struct SQuad
        {
            glm::vec2 Position;
            glm::vec2 Size;
            SRect TexCoords; // Position = Tex min, Size = Tex max
            SColor Color;
            uint32_t AtlasIndex = 0;
            glm::vec2 UnitRange;
            SRect ScissorRect;
        };

        std::vector<SQuad> m_Quads;

        Ref<Shader> m_Shader;
        Ref<GraphicsPipeline> m_Pipeline;
        Ref<DynamicVertexBuffer> m_QuadBuffer;

        Ref<UniformBuffer> m_PerFrameConstantBuffer;
        const GraphicsContext* m_GraphicsContext = nullptr;
    };
}