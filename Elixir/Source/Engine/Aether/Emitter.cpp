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

    SGPUEmitter Emitter::Build(const ParameterStore& params) const
    {
        SGPUEmitter emitter;
        emitter.Name = m_Name;
        emitter.MaxParticles = (uint32_t)m_Particles.size();
        emitter.SpawnRatePerSecond = m_SpawnRate;
        emitter.GravityScale = params.GetFloat("GravityScale", 1.0f);

        for (const auto& module : m_SpawnModules)
        {
            if (const auto* typed = dynamic_cast<const SetPositionCircle*>(module.get()))
            {
                emitter.SpawnCenter = typed->GetCenter();
                emitter.SpawnRadius = typed->GetRadius();
            }
            else if (const auto* typed = dynamic_cast<const SetVelocityCone*>(module.get()))
            {
                emitter.AngleMinRadians = typed->GetMinAngle();
                emitter.AngleMaxRadians = typed->GetMaxAngle();
                emitter.SpeedMin = typed->GetMinSpeed();
                emitter.SpeedMax = typed->GetMaxSpeed();
            }
            else if (const auto* typed = dynamic_cast<const SetLifetime*>(module.get()))
            {
                emitter.LifetimeMin = typed->GetMinSeconds();
                emitter.LifetimeMax = typed->GetMaxSeconds();
            }
            else if (const auto* typed = dynamic_cast<const SetSize*>(module.get()))
            {
                emitter.SizeStart = typed->GetMinSize();
                emitter.SizeEnd = typed->GetMaxSize();
            }
            else if (const auto* typed = dynamic_cast<const SetColor*>(module.get()))
            {
                emitter.ColorStart = typed->GetColor();
            }
        }

        for (const auto& module : m_UpdateModules)
        {
            if (const auto* typed = dynamic_cast<const ApplyGravity*>(module.get()))
            {
                emitter.Gravity = typed->GetGravity();
            }
            else if (const auto* typed = dynamic_cast<const ApplyLinearDrag*>(module.get()))
            {
                emitter.Drag = typed->GetDragPerSecond();
            }
            else if (const auto* typed = dynamic_cast<const ColorOverLife*>(module.get()))
            {
                emitter.ColorStart = typed->GetStartColor();
                emitter.ColorEnd = typed->GetEndColor();
            }
            else if (const auto* typed = dynamic_cast<const SizeOverLife*>(module.get()))
            {
                emitter.SizeStart = typed->GetStartSize();
                emitter.SizeEnd = typed->GetEndSize();
            }
            else if (const auto* typed = dynamic_cast<const KillOutsideBounds*>(module.get()))
            {
                emitter.MinBounds = typed->GetMin();
                emitter.MaxBounds = typed->GetMax();
            }
        }

        return emitter;
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