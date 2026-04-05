#include "epch.h"
#include "Emitter.h"

#include "System.h"

namespace Elixir::Aether
{
    Emitter::Emitter(
        const std::string& name,
        const uint32_t maxParticles,
        const float spawnRate
    ) : m_Name(name),
        m_Particles(maxParticles),
        m_SpawnRate(spawnRate),
        m_RNG(std::random_device{}()) {}

    void Emitter::Update(const Timestep& timestep, const ParameterStore& params)
    {
        m_SpawnAccumulator += m_SpawnRate * timestep.GetSeconds();

        SSpawnContext spawnContext{ timestep.GetSeconds(), m_RNG };
        while (m_SpawnAccumulator >= 1.0f)
        {
            SpawnParticle(spawnContext, params);
            m_SpawnAccumulator -= 1.0f;
        }

        SUpdateContext updateContext{ timestep.GetSeconds() };
        for (auto& particle : m_Particles)
        {
            if (!particle.Alive) continue;

            particle.Age += timestep.GetSeconds();
            if (particle.Age >= particle.Lifetime)
            {
                particle.Alive = false;
                continue;
            }

            for (const auto& module : m_UpdateModules)
            {
                module->Apply(particle, updateContext, params);
                if (!particle.Alive) break;
            }
        }
    }

    void Emitter::GatherRenderParticles(std::vector<SRenderParticle>& output) const
    {
        for (const auto& particle : m_Particles)
        {
            if (!particle.Alive) continue;
            output.push_back({ particle.Position, particle.Color, particle.Size });
        }
    }

    void Emitter::SpawnParticle(SSpawnContext& context, const ParameterStore& params)
    {
        const auto available = std::ranges::find_if(m_Particles, [](const auto& particle)
        {
            return !particle.Alive;
        });

        if (available == m_Particles.end()) return;

        SParticle particle{};
        particle.Color = {1.0f, 1.0f, 1.0f, 1.0f};
        particle.Lifetime = 1.0f;
        particle.Size = 4.0f;
        particle.Alive = true;

        for (const auto& module : m_SpawnModules)
        {
            module->Apply(particle, context, params);
        }

        *available = particle;
    }
}