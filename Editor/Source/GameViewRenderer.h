#pragma once

#include <Engine.h>

class GameViewRenderer final
{
public:
    GameViewRenderer(
        const Elixir::GraphicsContext* context,
        const Elixir::ShaderLoader* shaderLoader
    );

    bool Resize(const Elixir::Extent2D& extent);
    void Render() const;

    Ref<Elixir::Texture2D> GetRenderTarget() const { return m_RenderTarget; }
    Elixir::Extent2D GetExtent() const { return m_Extent; }

private:
    struct SFrameData
    {
        glm::vec2 Viewport;
        float AspectRatio;
        float Padding = 0.0f;
    };

    Ref<Elixir::Texture2D> CreateRenderTarget() const;

    const Elixir::GraphicsContext* m_GraphicsContext;
    Elixir::Extent2D m_Extent { 1280, 720 };
    Ref<Elixir::Texture2D> m_RenderTarget;
    Ref<Elixir::Shader> m_Shader;
    Ref<Elixir::UniformBuffer> m_FrameConstantBuffer;
    Ref<Elixir::GraphicsPipeline> m_Pipeline;
};
