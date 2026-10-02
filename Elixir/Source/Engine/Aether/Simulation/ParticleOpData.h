#pragma once

#include <Engine/Aether/Modules/Module.h>

// Internal GPU upload protocol.
namespace Elixir::Aether::Simulation::Detail
{
    using Modules::SGPUParticleOp;
    using Core::EParticleOp;

    /** @brief Stores one particle operation in the shader upload layout. */
    struct SParticleOpData
    {
        /** @brief Operation type, target, and two absolute parameter indices; -1 marks literals. */
        glm::vec4 Header{};
        /** @brief First operation-specific payload. */
        glm::vec4 Data0{};
        /** @brief Second operation-specific payload, including the cone angle index. */
        glm::vec4 Data1{};
        /** @brief Third operation-specific payload, including vortex parameter indices. */
        glm::vec4 Data2{};
    };

    /** @brief Converts system-relative parameter indices to GPU buffer indices. */
    inline SParticleOpData ToOpData(const SGPUParticleOp& op, uint32_t parameterBaseOffset)
    {
        const auto ResolveParameterIndex = [parameterBaseOffset](const uint32_t parameterIndex)
        {
            return parameterIndex == UINT32_MAX
                ? -1.0f
                : (float)(parameterBaseOffset + parameterIndex);
        };

        SParticleOpData desc{};

        desc.Header = {
            (float)(uint32_t)op.Type,
            (float)op.Target,
            ResolveParameterIndex(op.Parameter0Index),
            ResolveParameterIndex(op.Parameter1Index)
        };

        desc.Data0 = op.Data0;
        desc.Data1 = op.Data1;
        desc.Data2 = op.Data2;

        const auto ResolveEmbeddedParameterIndex = [parameterBaseOffset](const float parameterIndex)
        {
            const auto index = (int32_t)parameterIndex;
            return index < 0
                ? -1.0f
                : (float)(parameterBaseOffset + (uint32_t)index);
        };

        if (op.Type == EParticleOp::SampleCone)
        {
            desc.Data1.z = ResolveEmbeddedParameterIndex(op.Data1.z);
        }
        else if (op.Type == EParticleOp::ApplyVortex)
        {
            desc.Data2.z = ResolveEmbeddedParameterIndex(op.Data2.z);
            desc.Data2.w = ResolveEmbeddedParameterIndex(op.Data2.w);
        }

        return desc;
    }
}
