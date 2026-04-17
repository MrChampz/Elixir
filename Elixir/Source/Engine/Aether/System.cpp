#include "epch.h"
#include "System.h"

namespace Elixir::Aether
{
    uint32_t FindParameterIndex(
        const std::vector<SGPUParameter>& parameters,
        const std::string& name
    )
    {
        for (uint32_t i = 0; i < parameters.size(); ++i)
        {
            if (parameters[i].Name == name)
                return i;
        }
        return UINT32_MAX;
    }

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

        system.Parameters = m_Parameters.Compile();

        uint32_t particleOffset = 0;
        system.Emitters.reserve(m_Emitters.size());

        for (const auto& emitter : m_Emitters)
        {
            auto desc = emitter.Build(m_Parameters, system.Modules);
            desc.ParticleOffset = particleOffset;

            particleOffset += desc.MaxParticles;
            system.TotalMaxParticles += desc.MaxParticles;

            system.Emitters.push_back(desc);
        }

        const uint32_t canopyColorStart = FindParameterIndex(system.Parameters, "CanopyColorStart");
        const uint32_t canopyColorEnd = FindParameterIndex(system.Parameters, "CanopyColorEnd");
        const uint32_t sparksColorStart = FindParameterIndex(system.Parameters, "SparkColorStart");
        const uint32_t sparksColorEnd = FindParameterIndex(system.Parameters, "SparkColorEnd");

        for (auto& module : system.Modules)
        {
            if (module.Type != EModuleType::ColorOverLife)
                continue;

            if (module.Data0.x < 0.8f)
            {
                module.Parameter0Index = canopyColorStart;
                module.Parameter1Index = canopyColorEnd;
            }
            else
            {
                module.Parameter0Index = sparksColorStart;
                module.Parameter1Index = sparksColorEnd;
            }
        }

        return system;
    }
}