#pragma once

#include <Engine/Aether/Emitter.h>
#include <Engine/Aether/ParameterStore.h>
#include <Engine/Aether/CurveStore.h>
#include <Engine/Aether/ColorCurveStore.h>

namespace Elixir::Aether
{
    struct SCompiledTriggerTarget
    {
        uint32_t TargetEmitterIndex = 0;
        uint32_t BurstCount = 0;
        float DelaySeconds = 0.0f;
    };

    struct SCompiledSystem
    {
        UUID SourceId;
        uint32_t CompilationRevision = 0;

        std::string Name;
        EParticleStateLayout ParticleStateLayout = EParticleStateLayout::CoreV1;

        std::vector<SCompiledEmitter> Emitters;
        std::vector<SCompiledTriggerTarget> TriggerTargets;

        std::vector<SGPUParticleOp> Ops;

        std::vector<SGPUParameter> Parameters;
        std::vector<SGPUCurve> Curves;
        std::vector<SGPUColorCurve> ColorCurves;

        std::vector<std::string> SpriteTextures;

        uint32_t TotalMaxParticles = 0;
    };

    class ELIXIR_API System final
    {
      public:
        explicit System(const std::string& name);

        System(System&&) = default;
        System& operator=(System&&) = default;

        System(const System&) = delete;
        System& operator=(const System&) = delete;

        Emitter& AddEmitter(const std::string& name, uint32_t maxParticles, float spawnRate);

        SCompiledSystem Compile() const;

        ParameterStore& GetParameters() { return m_Parameters; }
        CurveStore& GetCurves() { return m_Curves; }
        ColorCurveStore& GetColorCurves() { return m_ColorCurves; }

      private:
        UUID m_UUID;
        mutable uint32_t m_CompilationRevision = 0;

        std::string m_Name;

        ParameterStore m_Parameters;
        CurveStore m_Curves;
        ColorCurveStore m_ColorCurves;

        std::vector<Scope<Emitter>> m_Emitters;
    };
}
