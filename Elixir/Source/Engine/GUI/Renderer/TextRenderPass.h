#pragma once

#include <Engine/GUI/FontManager.h>
#include <Engine/GUI/Renderer/RenderBatch.h>
#include <Engine/GUI/Renderer/RenderPass.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir::GUI
{
    struct SFontData
    {
        glm::vec2 UnitRange;
        float FontSize;
        float Padding;
    };

    class ELIXIR_API TextRenderPass final : public RenderPass
    {
      public:
        static constexpr size_t MAX_VERTICES = 10000;
        static constexpr size_t MAX_INDICES = 15000;

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
        void InitFontData();
        void InitRenderPass(const ShaderLoader* shaderLoader);
        void BindShaderParameters() const;

        void BuildTextGeometry(const SDrawCommand& cmd);
        void BuildTextureGeometry(const SDrawCommand& cmd);

        Ref<SFont> m_Font;

        SFontData m_FontData{};
        Ref<UniformBuffer> m_FontConstantBuffer;

        struct SVertex
        {
            glm::vec2 Position;
            glm::vec2 TexCoord;
        };

        std::vector<SVertex> m_Vertices;
        std::vector<uint32_t> m_Indices;

        Ref<Shader> m_Shader;
        Ref<GraphicsPipeline> m_Pipeline;
        Ref<DynamicVertexBuffer> m_VertexBuffer;
        Ref<DynamicIndexBuffer> m_IndexBuffer;

        Ref<UniformBuffer> m_PerFrameConstantBuffer;
        const GraphicsContext* m_GraphicsContext = nullptr;
    };
}