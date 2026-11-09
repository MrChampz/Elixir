#pragma once

#include "../Vegetation/VegetationEffect.h"
#include <Engine/Aether/Simulation/RenderFrame.h>
#include <Engine/Aether/Rendering/Renderer.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Graphics/FrameSlotState.h>

namespace EditorVFX
{
    /** Draws vegetation tubes using immutable Aether strand data. Owned only by the Editor.
     */
    class VegetationRenderer final
    {
      public:
        /**
         * @brief Creates shader and uniform resources.
         * @param context Graphics context that outlives this renderer.
         * @param loader Loader used to read the application shaders.
         */
        VegetationRenderer(
            const Elixir::GraphicsContext* context,
            const Elixir::ShaderLoader* loader
        );
        /**
         * @brief Retains particle buffers and copies the current camera.
         * @param frame Immutable simulation output.
         * @param camera Camera used for custom draws.
         */
        void Prepare(
            const Elixir::Ref<const Elixir::Aether::Simulation::RenderFrame>& frame,
            const Elixir::Camera& camera
        );
        /**
         * @brief Records vegetation tubes after the material system finishes scene geometry.
         * @param color Application color target.
         * @param depth Matching depth target retained from material rendering.
         */
        void Render(
            const Elixir::Ref<Elixir::Image>& color,
            const Elixir::Ref<Elixir::Image>& depth
        );

      private:
        Elixir::Ref<const Elixir::Aether::Simulation::RenderFrame> m_PendingFrame;
        Elixir::Aether::Rendering::SFrameData m_FrameData{};
        const Elixir::GraphicsContext* m_Context;
        Elixir::Ref<Elixir::Shader> m_Shader;
        Elixir::Ref<Elixir::GraphicsPipeline> m_Pipeline;
        Elixir::FrameSlotState<Elixir::Ref<Elixir::UniformBuffer>> m_FrameBuffers;
        Elixir::EImageFormat m_ColorFormat = Elixir::EImageFormat::Undefined;
        Elixir::EDepthStencilImageFormat m_DepthFormat =
            Elixir::EDepthStencilImageFormat::Undefined;
    };
}
