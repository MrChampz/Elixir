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
            emitter->Update(timestep, m_Parameters);
            emitter->GatherRenderParticles(m_RenderParticles);
        }
    }

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

        for (const auto& emitter : m_Emitters)
        {
            const auto prefix = emitter->GetName() + ".";

            const auto emitterParams = emitter->GetParameters().Compile(prefix);
            system.Parameters.insert(system.Parameters.end(), emitterParams.begin(), emitterParams.end());

            const auto emitterCurves = emitter->GetCurves().Compile(prefix);
            system.Curves.insert(system.Curves.end(), emitterCurves.begin(), emitterCurves.end());
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

        // TEMPORARY
        const uint32_t canopyColorStart = FindParameterIndex(system.Parameters, "CanopyColorStart");
        const uint32_t canopyColorEnd = FindParameterIndex(system.Parameters, "CanopyColorEnd");
        const uint32_t sparksColorStart = FindParameterIndex(system.Parameters, "SparkColorStart");
        const uint32_t sparksColorEnd = FindParameterIndex(system.Parameters, "SparkColorEnd");

        for (auto& op : system.Ops)
        {
            if (op.Type != EParticleOp::LerpOverLife ||
                op.Target != EParticleAttribute::Color ||
                op.Parameter0Index != UINT32_MAX ||
                op.Parameter1Index != UINT32_MAX)
                continue;

            if (op.Data0.x < 0.8f)
            {
                op.Parameter0Index = canopyColorStart;
                op.Parameter1Index = canopyColorEnd;
            }
            else
            {
                op.Parameter0Index = sparksColorStart;
                op.Parameter1Index = sparksColorEnd;
            }
        }

        return system;
    }
}