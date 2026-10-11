#pragma once

#include <Engine/Aether/Manager.h>

/** @brief Extends Aether with simulation reconstruction for the Editor preview timeline. */
class EditorVFXManager final : public Elixir::Aether::Manager
{
public:
    /** @brief Creates an independent preview manager using the Aether services. */
    using Manager::Manager;

    /**
     * @brief Simulates a fresh preview from time zero and renders the requested time.
     * @param targetSeconds Preview time from zero through five seconds.
     * @pre All registered instances are fresh and BeginFrame() was called.
     */
    void Replay(const Elixir::Camera& camera, const Elixir::Ref<Elixir::Image>& sceneTarget,
        float targetSeconds);
};
