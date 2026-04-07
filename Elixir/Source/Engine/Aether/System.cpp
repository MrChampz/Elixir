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

        if (!m_Emitters.empty())
        {
            system.Emitter = m_Emitters.front().Build(m_Parameters);
        }

        return system;
    }
}