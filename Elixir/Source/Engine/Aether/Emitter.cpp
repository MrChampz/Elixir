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

    SGPUEmitter Emitter::Build(
        const ParameterStore& params,
        std::vector<SGPUModule>& modules
    ) const
    {
        SGPUEmitter emitter;
        emitter.Name = m_Name;
        emitter.MaxParticles = (uint32_t)m_Particles.size();
        emitter.SpawnRatePerSecond = m_SpawnRate;
        emitter.GravityScale = params.GetFloat("GravityScale", 1.0f);
        emitter.SpawnModuleOffset = (uint32_t)modules.size();

        modules.push_back({
            EModuleType::SpawnRate,
            EParticleAttribute::None,
            UINT32_MAX,
            UINT32_MAX,
            { m_SpawnRate, 0.0f, 0.0f, 0.0f },
            {}
        });

        for (const auto& module : m_SpawnModules)
        {
            if (const auto* typed = dynamic_cast<const SetPositionCircle*>(module.get()))
            {
                modules.push_back({
                    EModuleType::SetPositionCircle,
                    EParticleAttribute::Position,
                    UINT32_MAX,
                    UINT32_MAX,
                    { typed->GetCenter(), typed->GetRadius(), 0.0f },
                    {}
                });
            }
            else if (const auto* typed = dynamic_cast<const SetVelocityCone*>(module.get()))
            {
                modules.push_back({
                    EModuleType::SetVelocityCone,
                    EParticleAttribute::Velocity,
                    UINT32_MAX,
                    UINT32_MAX,
                    { typed->GetMinAngle(), typed->GetMaxAngle(), typed->GetMinSpeed(), typed->GetMaxSpeed() },
                    {}
                });
            }
            else if (const auto* typed = dynamic_cast<const SetLifetime*>(module.get()))
            {
                modules.push_back({
                    EModuleType::SetLifetime,
                    EParticleAttribute::Lifetime,
                    UINT32_MAX,
                    UINT32_MAX,
                    { typed->GetMinSeconds(), typed->GetMaxSeconds(), 0.0f, 0.0f },
                    {}
                });
            }
            else if (const auto* typed = dynamic_cast<const SetSize*>(module.get()))
            {
                modules.push_back({
                    EModuleType::SetSize,
                    EParticleAttribute::Size,
                    UINT32_MAX,
                    UINT32_MAX,
                    { typed->GetMinSize(), typed->GetMaxSize(), 0.0f, 0.0f },
                    {}
                });
            }
            else if (const auto* typed = dynamic_cast<const SetColor*>(module.get()))
            {
                modules.push_back({
                    EModuleType::SetColor,
                    EParticleAttribute::Color,
                    UINT32_MAX,
                    UINT32_MAX,
                    typed->GetColor(),
                    {}
                });
            }
        }

        emitter.SpawnModuleCount = (uint32_t)modules.size() - emitter.SpawnModuleOffset;
        emitter.UpdateModuleOffset = (uint32_t)modules.size();

        for (const auto& module : m_UpdateModules)
        {
            if (const auto* typed = dynamic_cast<const ApplyGravity*>(module.get()))
            {
                modules.push_back({
                    EModuleType::ApplyGravity,
                    EParticleAttribute::Velocity,
                    UINT32_MAX,
                    UINT32_MAX,
                    { typed->GetGravity(), emitter.GravityScale, 0.0f },
                    {}
                });
            }
            else if (const auto* typed = dynamic_cast<const ApplyLinearDrag*>(module.get()))
            {
                modules.push_back({
                    EModuleType::ApplyLinearDrag,
                    EParticleAttribute::Velocity,
                    UINT32_MAX,
                    UINT32_MAX,
                    { typed->GetDragPerSecond(), 0.0f, 0.0f, 0.0f },
                    {}
                });
            }
            else if (const auto* typed = dynamic_cast<const ColorOverLife*>(module.get()))
            {
                modules.push_back({
                    EModuleType::ColorOverLife,
                    EParticleAttribute::Color,
                    UINT32_MAX,
                    UINT32_MAX,
                    typed->GetStartColor(),
                    typed->GetEndColor()
                });
            }
            else if (const auto* typed = dynamic_cast<const SizeOverLife*>(module.get()))
            {
                modules.push_back({
                    EModuleType::SizeOverLife,
                    EParticleAttribute::Size,
                    UINT32_MAX,
                    UINT32_MAX,
                    { typed->GetStartSize(), typed->GetEndSize(), 0.0f, 0.0f },
                    {}
                });
            }
            else if (const auto* typed = dynamic_cast<const KillOutsideBounds*>(module.get()))
            {
                modules.push_back({
                    EModuleType::KillOutsideBounds,
                    EParticleAttribute::Position,
                    UINT32_MAX,
                    UINT32_MAX,
                    { typed->GetMin(), 0.0f, 0.0f },
                    { typed->GetMax(), 0.0f, 0.0f }
                });
            }
        }

        emitter.UpdateModuleCount = (uint32_t)modules.size() - emitter.UpdateModuleOffset;

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