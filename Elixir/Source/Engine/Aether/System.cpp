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

    Emitter* System::FindEmitter(const std::string_view name) const
    {
        for (const auto& emitter : m_Emitters)
        {
            if (emitter->GetName() == name)
                return emitter.get();
        }

        return nullptr;
    }

    SCompiledSystem System::Compile(MaterialResolver& materialResolver) const
    {
        SCompiledSystem system;
        system.SourceId = m_UUID;
        system.CompilationRevision = ++m_CompilationRevision;

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

        system.ExposedParameters.reserve(system.Parameters.size());

        for (uint32_t i = 0; i < system.Parameters.size(); ++i)
        {
            system.ExposedParameters.push_back({
                .Name = system.Parameters[i].Name,
                .ParameterIndex = i,
            });
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

        uint32_t localParticleOffset = 0;
        system.Emitters.reserve(m_Emitters.size());

        for (const auto& emitter : m_Emitters)
        {
            auto compiled = emitter->Compile(
                m_Parameters,
                system.Parameters,
                system.Ops,
                materialResolver
            );
            compiled.LocalParticleOffset = localParticleOffset;

            localParticleOffset += compiled.MaxParticles;
            system.TotalMaxParticles += compiled.MaxParticles;

            system.Emitters.push_back(compiled);
        }

        std::vector<std::vector<SCompiledTriggerTarget>> targetsBySource(system.Emitters.size());

        for (std::size_t targetIndex = 0; targetIndex < m_Emitters.size(); ++targetIndex)
        {
            const auto& emitter = m_Emitters[targetIndex];

            const auto& name = emitter->GetTriggerEmitterName();
            if (name.empty()) continue;

            const auto found = std::ranges::find_if(m_Emitters, [&name](const auto& e)
            {
                return e->GetName() == name;
            });

            if (found != m_Emitters.end())
            {
                const auto sourceIndex = (uint32_t)std::distance(m_Emitters.begin(), found);

                auto& target = system.Emitters[targetIndex];
                target.TriggerSourceEmitterIndex = (int32_t)sourceIndex;
                target.IsTriggerDriven = true;

                targetsBySource[sourceIndex].push_back({
                    .TargetEmitterIndex = (uint32_t)targetIndex,
                    .BurstCount = target.BurstCount,
                    .DelaySeconds = target.TriggerDelaySeconds,
                });
            }
            else
            {
                EE_CORE_ERROR(
                    "Trigger source emitter '{}' not found for emitter '{}'.",
                    name,
                    emitter->GetName()
                )
            }
        }

        for (uint32_t sourceIndex = 0; sourceIndex < system.Emitters.size(); ++sourceIndex)
        {
            auto& source = system.Emitters[sourceIndex];
            const auto& targets = targetsBySource[sourceIndex];

            source.TriggerTargetOffset = (uint32_t)system.TriggerTargets.size();
            source.TriggerTargetCount = (uint32_t)targets.size();

            system.TriggerTargets.insert(
                system.TriggerTargets.end(),
                targets.begin(),
                targets.end()
            );
        }

        return system;
    }
}
