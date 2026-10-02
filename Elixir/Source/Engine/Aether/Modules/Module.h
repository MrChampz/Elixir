#pragma once

#include <Engine/Aether/Core/CurveStore.h>
#include <Engine/Aether/Core/ParameterStore.h>
#include <Engine/Aether/Core/Particle.h>

namespace Elixir::Aether::Modules
{
    /** @brief Identifies the point in the particle lifetime at which a module runs. */
    enum class EModulePhase : uint8_t
    {
        Spawn,
        Update
    };

    /** @brief Stores one GPU operation emitted by an authored particle module. */
    struct SGPUParticleOp
    {
        Core::EParticleOp Type = Core::EParticleOp::SetLiteral;
        Core::EParticleAttribute Target = Core::EParticleAttribute::None;
        uint32_t Parameter0Index = UINT32_MAX;
        uint32_t Parameter1Index = UINT32_MAX;
        glm::vec4 Data0{};
        glm::vec4 Data1{};
        glm::vec4 Data2{};
    };

    /**
     * @brief Gives a module the data it needs to compile GPU operations.
     *
     * The context resolves emitter-local values before system-level values. It
     * appends operations to the compiled system and does not own its inputs.
     */
    class ELIXIR_API ModuleCompileContext final
    {
    public:
        /**
         * @brief Creates a context for one emitter compilation.
         * @param operations Destination for the module's GPU operations.
         * @param parameters Compiled parameters available to the emitter.
         * @param emitterName Name used to resolve emitter-local parameters.
         * @param gravityScale Multiplier configured by the emitter.
         */
        ModuleCompileContext(
            std::vector<SGPUParticleOp>& operations,
            const std::vector<Core::SGPUParameter>& parameters,
            std::string_view emitterName,
            float gravityScale
        );

        /**
         * @brief Finds a parameter available to this emitter.
         *
         * The lookup first checks the emitter-local name, then the system name.
         *
         * @param name Authored parameter name.
         * @return The compiled parameter index, or UINT32_MAX when not found.
         */
        uint32_t FindParameter(std::string_view name) const;

        /**
         * @brief Finds the first compiled parameter for a curve.
         *
         * The lookup first checks the emitter-local curve, then the system curve.
         *
         * @param name Authored curve name.
         * @return The compiled parameter index, or UINT32_MAX when not found.
         */
        uint32_t FindCurve(std::string_view name) const;

        /**
         * @brief Returns the gravity multiplier configured by the emitter.
         * @return The gravity multiplier.
         */
        float GetGravityScale() const { return m_GravityScale; }

        /**
         * @brief Appends one operation to the compiled system.
         * @param operation Operation to execute on the GPU.
         */
        void Emit(SGPUParticleOp operation);

    private:
        std::vector<SGPUParticleOp>& m_Operations;
        const std::vector<Core::SGPUParameter>& m_Parameters;
        std::string_view m_EmitterName;
        float m_GravityScale;
    };

    /** @brief Base class for every authored particle module. */
    class ELIXIR_API Module
    {
    public:
        explicit Module(EModulePhase phase = EModulePhase::Spawn);
        virtual ~Module();

        /**
         * @brief Appends this module's GPU operations to the supplied context.
         * @return True when the module emitted its own operations. False keeps
         * legacy built-in modules on the compatibility compilation path.
         */
        virtual bool Compile(ModuleCompileContext& context) const;

        /** @brief Returns the phase in which this module is compiled. */
        EModulePhase GetPhase() const { return m_Phase; }

    private:
        EModulePhase m_Phase;
    };

    /** @brief Base class for modules that run when a particle is created. */
    class ELIXIR_API SpawnModule : public Module
    {
    protected:
        SpawnModule() : Module(EModulePhase::Spawn) {}
    };

    /** @brief Base class for modules that run while a particle is updated. */
    class ELIXIR_API UpdateModule : public Module
    {
    protected:
        UpdateModule() : Module(EModulePhase::Update) {}
    };
}
