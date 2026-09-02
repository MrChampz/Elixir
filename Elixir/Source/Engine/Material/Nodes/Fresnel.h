#pragma once

#include <Engine/Material/Material.h>
#include <Engine/Material/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Calculates a Schlick Fresnel factor.
     */
    class Fresnel final : public MaterialNode
    {
    public:
        std::string_view GetTypeName() const override { return "Material.Fresnel"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            return {
                .Code = "pow(saturate(1.0 - dot(N, V)), 5.0)",
                .ValueType = EMaterialValueType::Float
            };
        }
    };
}