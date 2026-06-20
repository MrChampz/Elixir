#pragma once

#include <Engine/Aether/Emitter.h>
#include <Engine/Aether/ParameterStore.h>
#include <Engine/Aether/CurveStore.h>
#include <Engine/Aether/ColorCurveStore.h>

#include <random>

namespace Elixir::Aether
{
    struct SRenderParticle
    {
        glm::vec2 Position;
        glm::vec4 Color;
        float Size = 1.0f;
    };

    struct SSpawnContext
    {
        float deltaTime = 0.0f;
        std::mt19937 RNG;
    };

    struct SUpdateContext
    {
        float deltaTime = 0.0f;
    };

    struct SGPUSystem
    {
        std::string Name;
        std::vector<SGPUEmitter> Emitters;
        std::vector<SGPUParticleOp> Ops;
        std::vector<SGPUParameter> Parameters;
        std::vector<SGPUCurve> Curves;
        std::vector<SGPUColorCurve> ColorCurves;
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

        void Update(const Timestep& timestep);

        Emitter& AddEmitter(const std::string& name, uint32_t maxParticles, float spawnRate);

        SGPUSystem Build() const;

        ParameterStore& GetParameters() { return m_Parameters; }
        CurveStore& GetCurves() { return m_Curves; }
        ColorCurveStore& GetColorCurves() { return m_ColorCurves; }
        const std::vector<SRenderParticle>& GetRenderParticles() const { return m_RenderParticles; }

      private:
        std::string m_Name;
        ParameterStore m_Parameters;
        CurveStore m_Curves;
        ColorCurveStore m_ColorCurves;
        std::vector<Scope<Emitter>> m_Emitters;
        std::vector<SRenderParticle> m_RenderParticles;
    };
}
