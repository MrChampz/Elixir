#pragma once

#include <Engine/Aether/Emitter.h>
#include <Engine/Aether/ParameterStore.h>
#include <Engine/Aether/CurveStore.h>
#include <Engine/Aether/ColorCurveStore.h>

namespace Elixir::Aether
{
    struct SCompiledSystem
    {
        std::string Name;
        EParticleStateLayout ParticleStateLayout = EParticleStateLayout::CoreV1;

        std::vector<SCompiledEmitter> Emitters;
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
        std::string m_Name;

        ParameterStore m_Parameters;
        CurveStore m_Curves;
        ColorCurveStore m_ColorCurves;

        std::vector<Scope<Emitter>> m_Emitters;
    };
}
