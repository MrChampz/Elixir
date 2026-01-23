#pragma once

#include <Engine/GUI/FontManager.h>
#include <Engine/GUI/RenderBatch.h>
#include <Engine/Graphics/Pipeline/Pipeline.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Graphics/CommandBuffer.h>

namespace Elixir::GUI
{
    struct SVertex
    {
        glm::vec2 Position;
        glm::vec2 TexCoord;
    };

    struct SQuadData
    {
        glm::vec2 Position;
        glm::vec2 Size;

        float CornerRadius = 0.0f;
        float Padding;

        SColor Color;
    };

    struct SPerFrameData
    {
        glm::vec2 RenderExtent;
        glm::vec2 Padding;
    };

    struct STextData
    {
        glm::vec2 UnitRange;
        float FontSize;
        float Padding;
    };

    struct SQuad
    {
        SRect Geometry;
        SRect TexCoords;
        SColor Color;
    };

    struct SBatchContext
    {
        std::vector<SVertex> Vertices;
        std::vector<uint32_t> Indices;

        Ref<Shader> Shader;
        Ref<GraphicsPipeline> Pipeline;
        Ref<DynamicVertexBuffer> VertexBuffer;
        Ref<DynamicIndexBuffer> IndexBuffer;

        void AddQuad(const SQuad& quad)
        {
            const auto baseIndex = Vertices.size();

            // Create 4 vertices for the quad
            const auto topLeft = quad.Geometry.Position;
            const auto bottomRight = quad.Geometry.Position + quad.Geometry.Size;
            const auto topRight = glm::vec2{ bottomRight.x, topLeft.y };
            const auto bottomLeft = glm::vec2{ topLeft.x, bottomRight.y };

            const auto topLeftUV = glm::vec2{ quad.TexCoords.Position.x, quad.TexCoords.Size.y };
            const auto bottomRightUV = glm::vec2{ quad.TexCoords.Size.x, quad.TexCoords.Position.y };
            const auto topRightUV = glm::vec2{ quad.TexCoords.Size.x, quad.TexCoords.Size.y };
            const auto bottomLeftUV = glm::vec2{ quad.TexCoords.Position.x, quad.TexCoords.Position.y };

            Vertices.push_back({ topLeft,     topLeftUV     });
            Vertices.push_back({ topRight,    topRightUV    });
            Vertices.push_back({ bottomRight, bottomRightUV });
            Vertices.push_back({ bottomLeft,  bottomLeftUV  });

            // Two triangles (6 indices)
            Indices.push_back(baseIndex + 0);
            Indices.push_back(baseIndex + 2);
            Indices.push_back(baseIndex + 1);
            Indices.push_back(baseIndex + 0);
            Indices.push_back(baseIndex + 3);
            Indices.push_back(baseIndex + 2);
        }

        void Clear()
        {
            Vertices.clear();
            Indices.clear();
        }

        void UpdateBuffers() const
        {
            VertexBuffer->UpdateData(Vertices.data(),  Vertices.size() * sizeof(SVertex));
            IndexBuffer->UpdateData(Indices.data(), Indices.size() * sizeof(uint32_t));
        }

        void Draw(const Ref<CommandBuffer>& cmd) const
        {
            Pipeline->Bind(cmd);
            VertexBuffer->Bind(cmd);
            IndexBuffer->Bind(cmd);
            cmd->DrawIndexed(Indices.size());
        }

        bool HasData() const { return !Vertices.empty(); }
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
        void BuildTextGeometry(const SDrawCommand& cmd, SBatchContext& batchContext);
        void BuildTextureGeometry(const SDrawCommand& cmd);

        size_t m_MaxVertices = 10000;
        size_t m_MaxIndices = 15000;

        SBatchContext m_TextBatchContext{};
        SBatchContext m_GUIBatchContext{};

        SPerFrameData m_PerFrameData{};
        Ref<UniformBuffer> m_PerFrameConstantBuffer;

        STextData m_TextData{};
        Ref<UniformBuffer> m_TextConstantBuffer;

        Ref<SFont> m_Font;

        Extent2D m_RenderExtent{};
        const GraphicsContext* m_GraphicsContext = nullptr;




        std::vector<SVertex> Vertices;
        std::vector<uint32_t> Indices;
        std::vector<SQuadData> m_Quads;

        Ref<Shader> Shader;
        Ref<GraphicsPipeline> Pipeline;
        Ref<VertexBuffer> m_VertexBuffer;
        Ref<DynamicVertexBuffer> m_QuadBuffer;
        Ref<IndexBuffer> m_IndexBuffer;
    };
}