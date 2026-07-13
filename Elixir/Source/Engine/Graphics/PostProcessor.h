#pragma once

#include <Engine/Graphics/Texture.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Graphics/Pipeline/Pipeline.h>

namespace Elixir
{
    // Full-screen post pass: grabs the rendered scene, then re-draws it with bloom
    // (soft glow around bright areas) and FXAA edge anti-aliasing.
    class ELIXIR_API PostProcessor
    {
      public:
        PostProcessor(const GraphicsContext* context, const ShaderLoader* shaderLoader);

        void Apply();

      private:
        struct alignas(16) SPostData
        {
            glm::vec2 ScreenSize{ 1.0f, 1.0f };
            float BloomThreshold = 0.6f;
            float BloomIntensity = 0.85f;
            float BloomRadius = 48.0f; // texels
            float _Pad0 = 0.0f;
            float _Pad1 = 0.0f;
            float _Pad2 = 0.0f;
        };

        const GraphicsContext* m_Context;

        Ref<Shader> m_Shader;
        Ref<GraphicsPipeline> m_Pipeline;
        Ref<UniformBuffer> m_Buffer;
        SPostData m_Data;
        Ref<Sampler> m_Sampler;

        Ref<Texture2D> m_SceneColor;
        Extent3D m_Extent{};
    };
}
