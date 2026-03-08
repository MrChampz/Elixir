#pragma once

#include <Engine/GUI/Renderer/RenderBatch.h>
#include <Engine/GUI/Renderer/RenderPass.h>
#include <Engine/Graphics/TextureSet.h>
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

        ~QuadRenderPass() override;

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

            /**
             * When texture is used, this represents the borders of 9-patch texture.
             * Border mapping = (left, top, right, bottom).
             *
             * When not used, this represents the corner radius of the quad.
             * Corner Radius mapping = (top-left, top-right, bottom-right, bottom-left).
             */
            glm::vec4 Border = glm::vec4(0.0f);

            /**
             * Shadow offset (x, y), blur (z) and intensity (w).
             */
            glm::vec4 InsetShadow = glm::vec4(0.0f);

            /**
             * Shadow offset (x, y), blur (z) and intensity (w).
             */
            glm::vec4 DropShadow = glm::vec4(0.0f);

            SColor Color;

            SColor OutlineColor;
            float OutlineThickness = 0.0f;

            uint32_t TextureIndex = 0;

            SRect ScissorRect;
        };

        std::vector<SQuad> m_Quads;

        Ref<Shader> m_Shader;
        Ref<GraphicsPipeline> m_Pipeline;
        Ref<DynamicVertexBuffer> m_QuadBuffer;
        Ref<TextureSet> m_TextureSet;

        Ref<Texture2D> m_WhiteTexture;
        SResourceHandle m_WhiteTextureHandle;

        Ref<UniformBuffer> m_PerFrameConstantBuffer;
        const GraphicsContext* m_GraphicsContext;
    };
}