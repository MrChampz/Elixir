#pragma once

#include <Engine/Core/UUID.h>
#include <Engine/Material/MaterialInstance.h>
#include <Engine/Material/MaterialResolver.h>
#include <Engine/Aether/Modules/Modules.h>
#include <Engine/Aether/Core/ParameterStore.h>
#include <Engine/Aether/Core/CurveStore.h>
#include <Engine/Aether/Core/ColorCurveStore.h>
#include <Engine/Aether/Effect/MaterialDescription.h>
#include <Engine/Aether/Effect/MaterialFactory.h>

namespace Elixir::Aether
{
    using namespace Core;
    using namespace Modules;
    /**
     * @brief Stores immutable GPU-ready data for one compiled emitter.
     *
     * System::Compile() creates this structure from an authored Emitter. The
     * particle renderer uses its ranges, material proxy, and render settings to
     * simulate and draw the emitter.
     *
     * Offsets refer to data owned by the containing SCompiledSystem unless stated
     * otherwise. They are not physical GPU addresses.
     *
     * @note Equality and hashing use ID only. Each compiled emitter must keep the
     * UUID of its authored source emitter.
     */
    struct SCompiledEmitter
    {
        UUID Id;
        EParticleRenderMode RenderMode = EParticleRenderMode::Sprite;
        EParticleSimulationSpace SimulationSpace = EParticleSimulationSpace::World;

        // Immutable GPU material state published by System::Compile.
        Ref<const MaterialRenderProxy> Material;

        float SpawnRatePerSecond = 1.0f;
        uint32_t BurstCount = 0u;
        float BurstIntervalSeconds = 0.0f;
        int32_t TriggerSourceEmitterIndex = -1;
        float TriggerDelaySeconds = 0.0f;
        uint32_t TriggerTargetOffset = 0;
        uint32_t TriggerTargetCount = 0;
        bool IsTriggerDriven = false;

        float GravityScale = 1.0f;

        // Relative to the beginning of this compiled system. It is not a
        // physical address in a shared GPU particle pool.
        uint32_t LocalParticleOffset = 0;

        uint32_t MaxParticles = 0u;
        uint32_t SpawnOpOffset = 0u;
        uint32_t SpawnOpCount = 0u;
        uint32_t UpdateOpOffset = 0u;
        uint32_t UpdateOpCount = 0u;

        bool operator==(const SCompiledEmitter& other) const noexcept
        {
            return Id == other.Id;
        }

        UUID GetHashParams() const
        {
            return Id;
        }
    };
}

GENERATE_HASH_FUNCTION(Elixir::Aether::SCompiledEmitter)

namespace Elixir::Aether
{
    /**
     * @brief Defines one particle source within an authored Aether system.
     *
     * An emitter owns spawn and update modules, local parameters and curves, and
     * material selection data. Compile() converts this authoring data into an
     * SCompiledEmitter and appends its GPU operations to the parent system.
     *
     * An emitter does not own runtime particle state. The renderer allocates that
     * state after its parent system is compiled and instantiated.
     *
     * @note An emitter is movable but not copyable. Its UUID identifies the
     * emitter across compiled system revisions.
     */
    class ELIXIR_API Emitter final
    {
        friend class System;

      public:
        /**
         * @brief Creates an emitter with a default spawn rate.
         *
         * @param name Display name for the emitter.
         * @param maxParticles Maximum number of particles the emitter can own.
         * @param spawnRate Default spawn rate in particles per second.
         */
        Emitter(const std::string& name, uint32_t maxParticles, float spawnRate);

        Emitter(Emitter&&) = default;
        Emitter& operator=(Emitter&&) = default;

        Emitter(const Emitter&) = delete;
        Emitter& operator=(const Emitter&) = delete;

        /**
         * @brief Adds a module that runs when particles spawn.
         *
         * The emitter owns the returned module.
         *
         * @tparam Module A type derived from ParticleSpawnModule.
         * @tparam Args Constructor argument types for Module.
         * @param args Arguments forwarded to the module constructor.
         * @return The newly created spawn module.
         */
        template <typename Module, typename... Args>
        Module& AddSpawnModule(Args&&... args)
        {
            auto module = CreateScope<Module>(std::forward<Args>(args)...);
            auto& ref = *module;
            m_SpawnModules.push_back(std::move(module));
            return ref;
        }

        /**
         * @brief Adds a module that runs while particles update.
         *
         * The emitter owns the returned module.
         *
         * @tparam Module A type derived from ParticleUpdateModule.
         * @tparam Args Constructor argument types for Module.
         * @param args Arguments forwarded to the module constructor.
         * @return The newly created update module.
         */
        template <typename Module, typename... Args>
        Module& AddUpdateModule(Args&&... args)
        {
            auto module = CreateScope<Module>(std::forward<Args>(args)...);
            auto& ref = *module;
            m_UpdateModules.push_back(std::move(module));
            return ref;
        }

        /**
         * @brief Returns the stable identity of this emitter.
         * @return The UUID of this emitter.
         */
        const UUID& GetId() const { return m_Id; }

        /**
         * @brief Returns the display name of this emitter.
         * @return The authored emitter name.
         */
        const std::string& GetName() const { return m_Name; }

        /**
         * @brief Returns the geometry mode used to render this emitter.
         * @return The selected particle render mode.
         */
        EParticleRenderMode GetRenderMode() const { return m_RenderMode; }

        /**
         * @brief Sets the geometry mode used to render this emitter.
         * @param mode Particle render mode to use during compilation.
         * @note The emitter's material must support this mode.
         */
        void SetRenderMode(const EParticleRenderMode mode) { m_RenderMode = mode; }

        /**
         * @brief Returns the simulation space used by this emitter.
         * @return The selected simulation space.
         */
        EParticleSimulationSpace GetSimulationSpace() const { return m_SimulationSpace; }

        /**
         * @brief Sets the simulation space used by this emitter.
         * @param space World or local particle simulation space.
         */
        void SetSimulationSpace(const EParticleSimulationSpace space) { m_SimulationSpace = space; }

        /**
         * @brief Returns the maximum particle capacity.
         * @return Maximum number of particles owned by this emitter.
         */
        uint32_t GetMaxParticles() const { return m_MaxParticles; }

        /**
         * @brief Returns the material data parsed from an effect asset.
         * @return The authored material description, when one exists.
         * @note This is effect-format data, not a Material instance.
         */
        const std::optional<Effect::SMaterialDescription>& GetMaterialDescription() const
        {
            return m_MaterialDescription;
        }

        /**
         * @brief Stores material data parsed from an effect asset.
         * @param description Effect-format material data for this emitter.
         * @note Effect::MaterialResolver converts this data into a material instance.
         */
        void SetMaterialDescription(Effect::SMaterialDescription description)
        {
            m_MaterialDescription = std::move(description);
        }

        /**
         * @brief Returns the selected material instance.
         * @return The material instance, or null when none has been assigned.
         */
        const Ref<MaterialInstance>& GetMaterial() const { return m_Material; }

        /**
         * @brief Creates and assigns a default instance of material.
         * @param material Material used to create the assigned instance.
         * @note A null material logs an error and preserves the current selection.
         */
        void SetMaterial(const Ref<Material>& material);

        /**
         * @brief Assigns a material instance to this emitter.
         * @param material Material instance to assign.
         * @note Pass a null reference to clear the current selection.
         */
        void SetMaterial(Ref<MaterialInstance> material) { m_Material = std::move(material); }

        /**
         * @brief Configure periodic burst emission.
         *
         * @param count Number of particles requested by each burst.
         * @param intervalSeconds Time between consecutive bursts.
         */
        void SetBurst(uint32_t count, float intervalSeconds);

        /**
         * @brief Makes this emitter react to another emitter's trigger events.
         *
         * The source name is resolved when the parent system is compiled.
         *
         * @param emitterName Name of the source emitter.
         * @param delaySeconds Delay before the triggered burst is requested.
         */
        void SetTriggerEmitter(std::string emitterName, float delaySeconds);

        /**
         * @brief Returns the number of particles requested by each burst.
         * @return Configured burst particle count.
         */
        uint32_t GetBurstCount() const { return m_BurstCount; }

        /**
         * @brief Returns the time between periodic bursts.
         * @return Burst interval in seconds.
         */
        float GetBurstIntervalSeconds() const { return m_BurstIntervalSeconds; }

        /**
         * @brief Returns the configured trigger source name.
         * @return Source emitter name, or an empty string when no trigger is set.
         */
        const std::string& GetTriggerEmitterName() const { return m_TriggerEmitterName; }

        /**
         * @brief Returns the delay applied after a trigger event.
         * @return Trigger delay in seconds.
         */
        float GetTriggerDelaySeconds() const { return m_TriggerDelaySeconds; }

        /**
         * @brief Returns this emitter's parameter store.
         * @return Mutable emitter-local parameters.
         */
        ParameterStore& GetParameters() { return m_Parameters; }

        /**
         * @brief Returns this emitter's parameter store.
         * @return Read-only emitter-local parameters.
         */
        const ParameterStore& GetParameters() const { return m_Parameters; }

        /**
         * @brief Returns this emitter's scalar curve store.
         * @return Mutable emitter-local scalar curves.
         */
        CurveStore& GetCurves() { return m_Curves; }

        /**
         * @brief Returns this emitter's scalar curve store.
         * @return Read-only emitter-local scalar curves.
         */
        const CurveStore& GetCurves() const { return m_Curves; }

        /**
         * @brief Returns this emitter's color curve store.
         * @return Mutable emitter-local color curves.
         */
        ColorCurveStore& GetColorCurves() { return m_ColorCurves; }

        /**
         * @brief Returns this emitter's color curve store.
         * @return Read-only emitter-local color curves.
         */
        const ColorCurveStore& GetColorCurves() const { return m_ColorCurves; }

        /**
         * @brief Returns the parameter name that overrides the spawn rate.
         * @return Parameter name, or an empty string when no override is set.
         */
        const std::string& GetSpawnRateParamName() const { return m_SpawnRateParamName; }

        /**
         * @brief Sets the parameter name that overrides the spawn rate.
         * @param paramName System-level or emitter-local parameter name.
         */
        void SetSpawnRateParamName(const std::string& paramName) { m_SpawnRateParamName = paramName; }

      private:
        // Compiles this emitter into internal GPU-ready runtime data.
        SCompiledEmitter Compile(
            const ParameterStore& paramStore,
            const std::vector<SGPUParameter>& params,
            std::vector<SGPUParticleOp>& ops,
            MaterialResolver& materialResolver
        ) const;

        UUID m_Id;
        std::string m_Name;
        EParticleRenderMode m_RenderMode = EParticleRenderMode::Sprite;
        EParticleSimulationSpace m_SimulationSpace = EParticleSimulationSpace::World;
        std::optional<Effect::SMaterialDescription> m_MaterialDescription;
        Ref<MaterialInstance> m_Material;
        uint32_t m_MaxParticles;

        std::vector<Scope<ParticleSpawnModule>> m_SpawnModules;
        std::vector<Scope<ParticleUpdateModule>> m_UpdateModules;

        std::string m_SpawnRateParamName;
        float m_SpawnRate = 0.0f; // particles per second
        uint32_t m_BurstCount = 0u;
        float m_BurstIntervalSeconds = 0.0f;
        std::string m_TriggerEmitterName;
        float m_TriggerDelaySeconds = 0.0f;

        ParameterStore m_Parameters;
        CurveStore m_Curves;
        ColorCurveStore m_ColorCurves;
    };
}
