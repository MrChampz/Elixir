#pragma once

#include <Engine/Aether/Emitter.h>
#include <Engine/Aether/Core/ParameterStore.h>
#include <Engine/Aether/Core/CurveStore.h>
#include <Engine/Aether/Core/ColorCurveStore.h>
#include <Engine/Aether/Core/ParticleStateLayout.h>

namespace Elixir
{
    class MaterialResolver;
}

namespace Elixir::Aether
{
    using namespace Core;
    using namespace Modules;
    /**
     * @brief Maps an exposed runtime parameter to the compiled parameter table.
     *
     * SystemInstance uses this mapping to validate named parameter overrides before
     * publishing an immutable render proxy.
     */
    struct SExposedParameter
    {
        std::string Name;
        uint32_t ParameterIndex = 0;
    };

    /**
     * @brief Describes one emitter activated by a trigger event.
     *
     * SCompiledSystem stores trigger targets in a flat table. Each source emitter
     * references a contiguous range of entries in that table.
     */
    struct SCompiledTriggerTarget
    {
        uint32_t TargetEmitterIndex = 0;
        uint32_t BurstCount = 0;
        float DelaySeconds = 0.0f;
    };

    /**
     * @brief Store immutable GPU-ready data for one compiled Aether system.
     *
     * System::Compile() creates this structure from mutable effect authoring data.
     * A SystemInstance selects one compiled system, and the particle renderer uses
     * its data to allocate GPU ranges, simulate particles, and build render items.
     *
     * All emitter and trigger ranges refer to tables owned by this structure.
     * The structure is treated as immutable after compilation.
     */
    struct SCompiledSystem
    {
        UUID SourceId;
        uint32_t CompilationRevision = 0;

        EParticleStateLayout ParticleStateLayout = EParticleStateLayout::CoreV1;

        std::vector<SCompiledEmitter> Emitters;
        std::vector<SCompiledTriggerTarget> TriggerTargets;

        std::vector<SGPUParticleOp> Ops;

        std::vector<SGPUParameter> Parameters;
        std::vector<SExposedParameter> ExposedParameters;
        std::vector<SGPUCurve> Curves;
        std::vector<SGPUColorCurve> ColorCurves;

        uint32_t TotalMaxParticles = 0;
    };

    /**
     * @brief Defines an authored Aether particle effect.
     *
     * System owns the emitters, parameters, and curves that describe one effect.
     * It is mutable authoring data. Compile() converts this data into an immutable
     * SCompiledSystem for runtime use.
     *
     * A system does not simulate or render particles. Create a SystemInstance to
     * use compiled system data at runtime.
     *
     * @note A system is movable but not copyable. Its UUID identifies the authored
     * source across compiled revisions.
     */
    class ELIXIR_API System final
    {
      public:
        /**
         * @brief Creates an empty particle effect definition.
         *
         * @param name Display name for the effect.
         */
        explicit System(const std::string& name);

        System(System&&) = default;
        System& operator=(System&&) = default;

        System(const System&) = delete;
        System& operator=(const System&) = delete;

        /**
         * @brief Adds an emitter to the effect.
         *
         * The system owns the returned emitter.
         *
         * @param name Display name for the emitter.
         * @param maxParticles Maximum number of particles owned by the emitter.
         * @param spawnRate Default particle spawn rate in particles per second.
         * @return The newly created emitter.
         */
        Emitter& AddEmitter(const std::string& name, uint32_t maxParticles, float spawnRate);

        /**
         * @brief Finds an emitter by name.
         *
         * @param name Name of the emitter to find.
         * @return The matching emitter, or null when no emitter has this name.
         */
        Emitter* FindEmitter(std::string_view name) const;

        /**
         * @brief Compiles the authored effect into immutable runtime data.
         *
         * The method compiles system and emitter parameters, bake curves into
         * parameter chunks, compile emitter modules into GPU operations, resolves
         * material render proxies, and builds trigger targets.
         *
         * @param materialResolver Resolves emitter material instances for rendering.
         * @return Immutable data for a SystemInstance and the particle renderer.
         *
         * @warning Any later change to this System requires a new compilation.
         */
        SCompiledSystem Compile(MaterialResolver& materialResolver) const;

        /**
         * @brief Returns the UUID of this effect system.
         * @return The system UUID.
         */
        const UUID& GetId() const { return m_UUID; }

        /**
         * @brief Returns the display name of this effect.
         * @return The authored effect name.
         */
        const std::string& GetName() const { return m_Name; }

        /**
         * @brief Returns the system's emitters.
         * @return Read-only list of emitters owned by this system.
         */
        const std::vector<Scope<Emitter>>& GetEmitters() const { return m_Emitters; }

        /**
         * @brief Returns the system-level parameter store.
         * @return Mutable parameters shared by emitters in this system.
         */
        ParameterStore& GetParameters() { return m_Parameters; }

        /**
         * @brief Returns the system-level parameter store.
         * @return Read-only parameters shared by emitters in this system.
         */
        const ParameterStore& GetParameters() const { return m_Parameters; }

        /**
         * @brief Returns the system-level scalar curve store.
         * @return Mutable scalar curves for this system.
         */
        CurveStore& GetCurves() { return m_Curves; }

        /**
         * @brief Returns the system-level color curve store.
         * @return Mutable color curves for this system.
         */
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
