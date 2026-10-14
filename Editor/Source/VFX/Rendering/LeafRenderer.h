#pragma once

#include "LeafSettings.h"
#include <Engine/Aether/Simulation/RenderFrame.h>
#include <Engine/Aether/Rendering/Renderer.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Graphics/FrameSlotState.h>

namespace EditorVFX
{
    /** Draws procedural leaves using immutable Aether strand data. Owned only by the Editor. */
    class LeafRenderer final
    {
    public:
        /** Creates shader and uniform resources. The graphics context must outlive the renderer. */
        LeafRenderer(const Elixir::GraphicsContext* context, const Elixir::ShaderLoader* loader);
        /** Replaces per-emitter settings on the render-frame thread. Missing entries draw no leaves. */
        void SetSettings(std::vector<std::optional<LeafSettings>> settings) { m_Settings = std::move(settings); }
        /** Retains simulation buffers and copies camera values for this frame's custom draws. */
        void Prepare(const Elixir::Ref<const Elixir::Aether::Simulation::RenderFrame>& frame, const Elixir::Camera& camera);
        /** Records the prepared opaque leaf draws after tubes, preserving the application's depth target. */
        void Render(const Elixir::Ref<Elixir::Image>& color, const Elixir::Ref<Elixir::Image>& depth);
    private:
        Elixir::Ref<const Elixir::Aether::Simulation::RenderFrame> m_PendingFrame;
        Elixir::Aether::Rendering::SFrameData m_FrameData{};
        const Elixir::GraphicsContext* m_Context;
        Elixir::Ref<Elixir::Shader> m_Shader;
        Elixir::Ref<Elixir::GraphicsPipeline> m_Pipeline;
        Elixir::FrameSlotState<Elixir::Ref<Elixir::UniformBuffer>> m_FrameBuffers;
        std::vector<std::optional<LeafSettings>> m_Settings;
        Elixir::EImageFormat m_ColorFormat = Elixir::EImageFormat::Undefined;
        Elixir::EDepthStencilImageFormat m_DepthFormat = Elixir::EDepthStencilImageFormat::Undefined;
    };
}
