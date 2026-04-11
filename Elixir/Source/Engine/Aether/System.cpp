#include "epch.h"
#include "System.h"

namespace Elixir::Aether
{
    System::System(const std::string& name) : m_Name(name) {}

    void System::Update(const Timestep& timestep)
    {
        m_RenderParticles.clear();

        for (auto& emitter : m_Emitters)
        {
            emitter.Update(timestep, m_Parameters);
            emitter.GatherRenderParticles(m_RenderParticles);
        }
    }

    Emitter& System::AddEmitter(
        const std::string& name,
        uint32_t maxParticles,
        float spawnRate
    )
    {
        m_Emitters.emplace_back(name, maxParticles, spawnRate);
        return m_Emitters.back();
    }

    SGPUSystem System::Build() const
    {
        SGPUSystem system;
        system.Name = m_Name;

        uint32_t particleOffset = 0;
        system.Emitters.reserve(m_Emitters.size());

        for (const auto& emitter : m_Emitters)
        {
            auto desc = emitter.Build(m_Parameters);
            desc.ParticleOffset = particleOffset;

            particleOffset += desc.MaxParticles;
            system.TotalMaxParticles += desc.MaxParticles;

            system.Emitters.push_back(desc);
        }

        return system;
    }
}