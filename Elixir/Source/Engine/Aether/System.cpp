#include "epch.h"
#include "System.h"

#include <Engine/Aether/SystemInstance.h>

namespace Elixir::Aether
{
    System::System(const std::string& name) : m_Name(name) {}

    Ref<SystemInstance> System::CreateInstance()
    {
        return CreateRef<SystemInstance>(shared_from_this());
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

    Emitter* System::FindEmitter(const std::string_view name) const
    {
        for (const auto& emitter : m_Emitters)
        {
            if (emitter->GetName() == name)
                return emitter.get();
        }

        return nullptr;
    }

    std::vector<SParameter> System::BuildExposedParameters() const
    {
        auto parameters = m_Parameters.Compile();

        for (const auto& emitter : m_Emitters)
        {
            const auto prefix = emitter->GetName() + ".";
            const auto params = emitter->GetParameters().Compile(prefix);
            parameters.insert(parameters.end(), params.begin(), params.end());
        }

        return parameters;
    }

    std::optional<SParameter> System::GetParameterDefault(std::string_view name) const
    {
        const auto parameters = BuildExposedParameters();
        const auto found = std::ranges::find_if(parameters, [&name](const auto& param)
        {
            return param.Name == name;
        });

        if (found == parameters.end())
            return std::nullopt;

        return *found;
    }

    SCompiledSystem System::Compile() const
    {
        SCompiledSystem system;
        system.SourceId = m_UUID;
        system.CompilationRevision = ++m_CompilationRevision;

        auto parameters = BuildExposedParameters();

        const auto systemCurves = m_Curves.Compile();
        system.Curves.insert(system.Curves.end(), systemCurves.begin(), systemCurves.end());

        const auto systemColorCurves = m_ColorCurves.Compile();
        system.ColorCurves.insert(system.ColorCurves.end(), systemColorCurves.begin(), systemColorCurves.end());

        for (const auto& emitter : m_Emitters)
        {
            const auto prefix = emitter->GetName() + ".";

            const auto emitterCurves = emitter->GetCurves().Compile(prefix);
            system.Curves.insert(system.Curves.end(), emitterCurves.begin(), emitterCurves.end());

            const auto emitterColorCurves = emitter->GetColorCurves().Compile(prefix);
            system.ColorCurves.insert(system.ColorCurves.end(), emitterColorCurves.begin(), emitterColorCurves.end());
        }

        system.ExposedParameters.reserve(parameters.size());

        for (uint32_t i = 0; i < parameters.size(); ++i)
        {
            system.ExposedParameters.push_back({
                .Name = parameters[i].Name,
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

            parameters.push_back({ .Name = curve.Name + ":0", .Value = CurveChunk(samples, 0) });
            parameters.push_back({ .Name = curve.Name + ":1", .Value = CurveChunk(samples, 1) });
        }

        for (const auto& curve : system.ColorCurves)
        {
            const auto baked = BakeColorCurve(curve.Samples);
            for (std::size_t i = 0; i < 8; ++i)
                parameters.push_back({
                    .Name = curve.Name + ":" + std::to_string(i),
                    .Value = baked[i]
                });
        }

        system.Parameters = CompileParameters(parameters);
        if (system.Parameters.size() != parameters.size()) return system;

        system.DefaultParameterWords = PackParameterValues(system.Parameters);

        if (system.DefaultParameterWords.size() > OperandIndexMask)
        {
            EE_CORE_ERROR("Aether parameter table exceeds the operand word range.");
            return system;
        }

        uint32_t localParticleOffset = 0;
        system.Emitters.reserve(m_Emitters.size());

        for (const auto& emitter : m_Emitters)
        {
            auto compiled = emitter->Compile(
                m_Parameters,
                system.Parameters,
                system.ModuleData
            );
            compiled.LocalParticleOffset = localParticleOffset;

            localParticleOffset += compiled.MaxParticles;
            system.TotalMaxParticles += compiled.MaxParticles;

            system.Emitters.push_back(compiled);
        }

        system.ModuleData.CompactConstants();

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
