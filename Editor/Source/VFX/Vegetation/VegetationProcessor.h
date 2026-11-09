#pragma once

#include "VegetationSettings.h"
#include <Engine/Aether/Simulation/ParticleProcessor.h>
#include <atomic>

namespace EditorVFX
{
    /** Owns the Editor's development playback control independently of Aether physics. */
    struct VegetationPlayback
    {
        std::atomic_bool Paused{false};
    };

    /** Describes the Editor-owned growth, branching, and attachment compute stage. */
    class VegetationProcessor final : public Elixir::Aether::Simulation::ParticleProcessor
    {
      public:
        /**
         * @brief Creates an immutable vegetation processor description.
         * @param settings Development and constraint settings.
         * @param playback Editor playback control shared with the resulting runtime.
         */
        VegetationProcessor(
            VegetationSettings settings,
            Elixir::Ref<VegetationPlayback> playback
        );
        /**
         * @brief Checks the particle budget required by the vegetation sample.
         * @param emitter Compiled emitter to validate.
         */
        bool IsCompatible(const Elixir::Aether::SCompiledEmitter& emitter) const override;
        /**
         * @brief Creates the Editor's compute resources for one Aether Simulator.
         * @param context Graphics context that outlives the returned runtime.
         * @param loader Loader available during initialization.
         */
        Elixir::Scope<Elixir::Aether::Simulation::ParticleProcessorRuntime> CreateRuntime(
            const Elixir::GraphicsContext* context,
            const Elixir::ShaderLoader& loader
        ) const override;

      private:
        VegetationSettings m_Settings;
        Elixir::Ref<VegetationPlayback> m_Playback;
    };
}
