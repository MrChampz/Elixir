#include "epch.h"
#include "System.h"

namespace Elixir::Aether
{
    System::System(const std::string& name) : m_Name(name) {}

    Emitter& System::AddEmitter(
        const std::string& name,
        uint32_t maxParticles,
        float spawnRate
    )
    {
        m_Emitters.push_back(CreateScope<Emitter>(name, maxParticles, spawnRate));
        return *m_Emitters.back();
    }

    SGPUSystem System::Build() const
    {
        SGPUSystem system;
        system.Name = m_Name;

        system.Parameters = m_Parameters.Compile();

        const auto systemCurves = m_Curves.Compile();
        system.Curves.insert(system.Curves.end(), systemCurves.begin(), systemCurves.end());

        const auto systemColorCurves = m_ColorCurves.Compile();
        system.ColorCurves.insert(system.ColorCurves.end(), systemColorCurves.begin(), systemColorCurves.end());

        for (const auto& emitter : m_Emitters)
        {
            const auto prefix = emitter->GetName() + ".";

            const auto emitterParams = emitter->GetParameters().Compile(prefix);
            system.Parameters.insert(system.Parameters.end(), emitterParams.begin(), emitterParams.end());

            const auto emitterCurves = emitter->GetCurves().Compile(prefix);
            system.Curves.insert(system.Curves.end(), emitterCurves.begin(), emitterCurves.end());

            const auto emitterColorCurves = emitter->GetColorCurves().Compile(prefix);
            system.ColorCurves.insert(system.ColorCurves.end(), emitterColorCurves.begin(), emitterColorCurves.end());
        }

        for (const auto& curve : system.Curves)
        {
            std::vector<float> samples = curve.Samples;

            if (samples.empty())
                samples = { 1.0f };

            if (samples.size() < 8)
                samples.resize(8, samples.back());

            system.Parameters.push_back({ curve.Name + ":0", CurveChunk(samples, 0) });
            system.Parameters.push_back({ curve.Name + ":1", CurveChunk(samples, 1) });
        }

        for (const auto& curve : system.ColorCurves)
        {
            const auto baked = BakeColorCurve(curve.Samples);
            for (std::size_t i = 0; i < 8; ++i)
                system.Parameters.push_back({ curve.Name + ":" + std::to_string(i), baked[i] });
        }

        uint32_t particleOffset = 0;
        system.Emitters.reserve(m_Emitters.size());

        for (const auto& emitter : m_Emitters)
        {
            auto desc = emitter->Build(m_Parameters, system.Parameters, system.Ops);
            desc.ParticleOffset = particleOffset;

            particleOffset += desc.MaxParticles;
            system.TotalMaxParticles += desc.MaxParticles;

            system.Emitters.push_back(desc);
        }

        return system;
    }
}