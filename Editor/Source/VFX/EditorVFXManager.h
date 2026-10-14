#pragma once

#include <Engine/Aether/Manager.h>
#include "Rendering/LeafRenderer.h"
#include "Rendering/BalloonRenderer.h"

/** @brief Extends Aether with simulation reconstruction for the Editor preview timeline. */
class EditorVFXManager final : public Elixir::Aether::Manager
{
public:
    /** @brief Creates an independent preview manager using the Aether services. */
    EditorVFXManager(const Elixir::GraphicsContext* context, const Elixir::ShaderLoader* loader,
        Elixir::Materials::MaterialRegistry& registry, Elixir::Materials::MaterialSystem& materials);
    /** Replaces custom renderer settings after a successful effect compilation. Call on the render thread. */
    void SetLeafRenderers(std::vector<std::optional<EditorVFX::LeafSettings>> settings);
    /** Replaces optional balloon renderer settings on the render-frame thread. */
    void SetBalloonRenderers(std::vector<std::optional<EditorVFX::BalloonSettings>> settings);
    /** Records custom draws after the material system rendered the tubes. Call on the render thread. */
    void RenderCustomRenderers(const Elixir::Ref<Elixir::Image>& color, const Elixir::Ref<Elixir::Image>& depth);

    /**
     * @brief Simulates a fresh preview from time zero and renders the requested time.
     * @param targetSeconds Preview time from zero through five seconds.
     * @pre All registered instances are fresh and BeginFrame() was called.
     */
    void Replay(const Elixir::Camera& camera, const Elixir::Ref<Elixir::Image>& sceneTarget,
        float targetSeconds);
protected:
    /** Draws Editor renderers using the immutable frame produced by Aether. */
    void OnRenderFrame(const Elixir::Ref<const Elixir::Aether::Simulation::RenderFrame>& frame, const Elixir::Camera& camera,
        const Elixir::Ref<Elixir::Image>& sceneTarget) override;
private:
    EditorVFX::LeafRenderer m_Leaves;
    EditorVFX::BalloonRenderer m_Balloons;
};
