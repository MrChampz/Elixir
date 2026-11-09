#pragma once

#include <Engine/Aether/Manager.h>
#include "Rendering/LeafRenderer.h"
#include "Rendering/BalloonRenderer.h"

/** @brief Extends Aether with simulation reconstruction for the Editor preview timeline. */
class EditorVFXManager final : public Elixir::Aether::Manager
{
public:
    /**
     * @brief Creates independent preview services and attachment renderers.
     * @param context Graphics context that outlives the manager.
     * @param loader Shader loader used to initialize renderers.
     * @param registry Material registry shared with the application.
     * @param materials Material system that receives particle geometry.
     */
    EditorVFXManager(
        const Elixir::GraphicsContext* context,
        const Elixir::ShaderLoader* loader,
        Elixir::Materials::MaterialRegistry& registry,
        Elixir::Materials::MaterialSystem& materials
    );
    /**
     * @brief Replaces leaf settings on the render-frame thread.
     * @param settings Optional renderer settings indexed by emitter.
     */
    void SetLeafRenderers(std::vector<std::optional<EditorVFX::LeafSettings>> settings);
    /**
     * @brief Replaces balloon settings on the render-frame thread.
     * @param settings Optional renderer settings indexed by emitter.
     */
    void SetBalloonRenderers(std::vector<std::optional<EditorVFX::BalloonSettings>> settings);
    /**
     * @brief Records application geometry after the material system draws tubes.
     * @param color Preview color target.
     * @param depth Matching preview depth target.
     */
    void RenderCustomRenderers(
        const Elixir::Ref<Elixir::Image>& color,
        const Elixir::Ref<Elixir::Image>& depth
    );

    /**
     * @brief Simulates a fresh preview from time zero and renders the requested time.
     * @param camera Preview camera.
     * @param sceneTarget Preview color target.
     * @param targetSeconds Preview time from zero through five seconds.
     * @pre All registered instances are fresh and BeginFrame() was called.
     */
    void Replay(const Elixir::Camera& camera, const Elixir::Ref<Elixir::Image>& sceneTarget,
        float targetSeconds);
protected:
    /**
     * @brief Prepares attachments from the immutable simulation output.
     * @param frame Simulation output retaining particle buffers.
     * @param camera Preview camera.
     * @param sceneTarget Preview color target.
     */
    void OnRenderFrame(
        const Elixir::Ref<const Elixir::Aether::Simulation::RenderFrame>& frame,
        const Elixir::Camera& camera,
        const Elixir::Ref<Elixir::Image>& sceneTarget
    ) override;
private:
    EditorVFX::LeafRenderer m_Leaves;
    EditorVFX::BalloonRenderer m_Balloons;
};
