#pragma once

#include <Engine/Aether/Emitter.h>
#include <Engine/Aether/ParameterStore.h>

#include <random>

namespace Elixir::Aether
{
    struct SRenderParticle
    {
        glm::vec2 Position;
        glm::vec4 Color;
        float Size = 1.0f;
    };

    struct SSpawnContext
    {
        float deltaTime = 0.0f;
        std::mt19937 RNG;
    };

    struct SUpdateContext
    {
        float deltaTime = 0.0f;
    };

    class ELIXIR_API System final
    {
      public:
        explicit System(const std::string& name);

        void Update(const Timestep& timestep);

        Emitter& AddEmitter(const std::string& name, uint32_t maxParticles, float spawnRate);

        ParameterStore& GetParameters() { return m_Parameters; }
        const std::vector<SRenderParticle>& GetRenderParticles() const { return m_RenderParticles; }

      private:
        std::string m_Name;
        ParameterStore m_Parameters;
        std::vector<Emitter> m_Emitters;
        std::vector<SRenderParticle> m_RenderParticles;
    };
}
