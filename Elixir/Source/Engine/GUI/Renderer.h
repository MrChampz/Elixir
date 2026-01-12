#pragma once

#include <Engine/GUI/RenderBatch.h>
#include <Engine/Graphics/Pipeline/Pipeline.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir::GUI
{
    struct Vertex
    {
        glm::vec2 Position;
        glm::vec2 TexCoord;
        SColor Color;
    };

    struct PerFrameData
    {
        glm::vec2 RenderExtent;
        glm::vec2 Padding;
    };

    class ELIXIR_API Renderer final
    {
      public:
        ~Renderer() = default;

        void Initialize(
            const GraphicsContext* context,
            const ShaderLoader* shaderLoader,
            const Extent2D& extent
        );
        void Shutdown();

        void Resize(const Extent2D& extent);

        void Render(const RenderBatch& batch);

      private:
        void BuildRectGeometry(const SDrawCommand& cmd);
        void BuildRectOutlineGeometry(const SDrawCommand& cmd);
        void BuildTextGeometry(const SDrawCommand& cmd);
        void BuildTextureGeometry(const SDrawCommand& cmd);

        size_t m_MaxVertices = 10000;
        size_t m_MaxIndices = 15000;

        std::vector<Vertex> m_Vertices;
        std::vector<uint32_t> m_Indices;

        Ref<Shader> m_Shader;
        Ref<GraphicsPipeline> m_Pipeline;
        Ref<DynamicVertexBuffer> m_VertexBuffer;
        Ref<DynamicIndexBuffer> m_IndexBuffer;

        PerFrameData m_PerFrameData{};
        Ref<UniformBuffer> m_PerFrameConstantBuffer;

        Extent2D m_RenderExtent;
        const GraphicsContext* m_GraphicsContext = nullptr;
    };
}