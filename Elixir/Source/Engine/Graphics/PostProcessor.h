#pragma once

#include <Engine/Graphics/FrameSlotState.h>
#include <Engine/Graphics/Image.h>

namespace Elixir
{
    class GraphicsContext;
    class GraphicsPipeline;
    class Sampler;
    class Shader;
    class ShaderLoader;
    class UniformBuffer;

    /** @brief Configures HDR bloom, exposure, and tone mapping. */
    struct SPostProcessSettings
    {
        /** Exposure adjustment in stops before tone mapping. */
        float Exposure = 0.0f;

        /** HDR luminance at which bloom starts contributing. */
        float BloomThreshold = 1.0f;

        /** Width of the smooth bloom threshold transition. */
        float BloomKnee = 0.5f;

        /** Bloom blur radius in source-image texels. */
        float BloomRadius = 6.0f;

        /** Multiplier applied to the HDR bloom contribution. */
        float BloomIntensity = 0.08f;
    };

    /** @brief Converts an HDR scene image into the application's LDR render target. */
    class ELIXIR_API PostProcessor final
    {
    public:
        /**
         * @brief Creates post-processing resources.
         * @param context Graphics context used to create and submit GPU resources.
         * @param shaderLoader Loader used to obtain post-process shaders.
         * @param extent Dimensions of the HDR scene target in texels.
         * @pre context and shaderLoader remain valid for this object's lifetime.
         */
        PostProcessor(
            const GraphicsContext* context,
            const ShaderLoader* shaderLoader,
            const Extent3D& extent
        );

        /**
         * @brief Resizes post-process targets before recording the next frame.
         * @param extent Dimensions of the HDR scene target in texels.
         */
        void Resize(const Extent3D& extent);

        /**
         * @brief Applies HDR bloom, exposure, and tone mapping to a scene.
         * @param sceneTarget HDR scene image produced by world renderers.
         * @param renderTarget LDR render target that receives the display-ready image.
         * @pre sceneTarget and renderTarget have equal extents.
         */
        void Apply(const Ref<Image>& sceneTarget, const Ref<Image>& renderTarget);

        /** @brief Returns the settings used by future post-process passes. */
        SPostProcessSettings& GetSettings() { return m_Settings; }

        /** @brief Returns the settings used by future post-process passes. */
        const SPostProcessSettings& GetSettings() const { return m_Settings; }

    private:
        const GraphicsContext* m_Context = nullptr;
        Ref<Image> m_BloomTarget;
        Ref<Shader> m_BloomShader;
        Ref<Shader> m_ToneMapShader;
        Ref<GraphicsPipeline> m_BloomPipeline;
        Ref<GraphicsPipeline> m_ToneMapPipeline;
        Ref<Sampler> m_Sampler;
        FrameSlotState<Ref<UniformBuffer>> m_FrameBuffers;
        SPostProcessSettings m_Settings;
    };
}
