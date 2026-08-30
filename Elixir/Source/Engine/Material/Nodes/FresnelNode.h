#pragma once

#include <Engine/Material/MaterialGraph.h>

namespace Elixir::MaterialNodes
{
    /** @brief Calculates a Schlick Fresnel factor. */
    class FresnelNode final : public IMaterialNode
    {
    public:
        std::string_view GetTypeName() const override { return "material.fresnel"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }
        SMaterialExpression Emit(const SMaterialEmitContext&) const override
        {
            return { "pow(saturate(1.0 - dot(N, V)), 5.0)", EMaterialValueType::Float };
        }

    private:
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}
