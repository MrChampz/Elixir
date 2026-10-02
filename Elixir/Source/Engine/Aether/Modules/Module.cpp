#include "epch.h"
#include "Module.h"

namespace Elixir::Aether::Modules
{
    /* ModuleCompileContext */

    ModuleCompileContext::ModuleCompileContext(
        std::vector<SGPUParticleOp>& operations,
        const std::vector<Core::SGPUParameter>& parameters,
        const std::string_view emitterName,
        const float gravityScale
    ) : m_Operations(operations),
        m_Parameters(parameters),
        m_EmitterName(emitterName),
        m_GravityScale(gravityScale) {}

    uint32_t ModuleCompileContext::FindParameter(const std::string_view name) const
    {
        return Core::FindScopedParameterIndex(
            m_Parameters,
            std::string(m_EmitterName),
            std::string(name)
        );
    }

    uint32_t ModuleCompileContext::FindCurve(const std::string_view name) const
    {
        return Core::FindCurveParameterIndex(
            m_Parameters,
            std::string(m_EmitterName),
            std::string(name)
        );
    }

    void ModuleCompileContext::Emit(SGPUParticleOp operation)
    {
        m_Operations.push_back(std::move(operation));
    }

    /* Module */

    Module::Module(const EModulePhase phase) : m_Phase(phase) {}

    Module::~Module() = default;
}
