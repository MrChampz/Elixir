#pragma once

#include "LeafSettings.h"
#include <Engine/Aether/Simulation/RenderFrame.h>
#include <Engine/Aether/Rendering/Renderer.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Graphics/FrameSlotState.h>

namespace EditorVFX
{
    /** Draws procedural leaves using immutable Aether strand data. Owned only by the Editor.
     */
    class LeafRenderer final
    {
      public:
        /**
         * @brief Creates shader and uniform resources.
         * @param context Graphics context that outlives this renderer.
         * @param loader Loader used to read the application shaders.
         */
        LeafRenderer(
            const Elixir::GraphicsContext* context,
            const Elixir::ShaderLoader* loader
        );
        /**
         * @brief Replaces settings on the render-frame thread.
         * @param settings Optional visual settings indexed by emitter.
         */
        void SetSettings(std::vector<std::optional<LeafSettings>> settings)
        {
            m_Settings = std::move(settings);
        }
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
         * @brief Records leaves after the material system draws strand tubes.
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
        std::vector<std::optional<LeafSettings>> m_Settings;
        Elixir::EImageFormat m_ColorFormat = Elixir::EImageFormat::Undefined;
        Elixir::EDepthStencilImageFormat m_DepthFormat =
            Elixir::EDepthStencilImageFormat::Undefined;
    };
}
