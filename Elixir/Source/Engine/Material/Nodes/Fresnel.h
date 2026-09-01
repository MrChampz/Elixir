#pragma once

#include <vector>
#include <string_view>

#include <Engine/Material/Material.h>
#include <Engine/Material/MaterialGraph.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Calculates a Schlick Fresnel factor.
     */
    class Fresnel final : public MaterialNode
    {
    public:
        std::string_view GetTypeName() const override { return "Material.Fresnel"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            return {
                .Code = "pow(saturate(1.0 - dot(N, V)), 5.0)",
                .ValueType = EMaterialValueType::Float
            };
        }

    private:
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}