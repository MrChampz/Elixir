#pragma once

#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /** @brief Scales the XY components of a tangent-space normal. */
    class ScaleNormal final : public MaterialNode
    {
    public:
        ScaleNormal()
          : MaterialNode({{
                "Normal",
                EMaterialValueType::Float3,
                "float3(0.0, 0.0, 1.0)",
                EMaterialValueType::Float3
            },
            {
                "Scale",
                EMaterialValueType::Float,
                "1.0",
                EMaterialValueType::Float
            }}) {}

        std::string_view GetTypeName() const override { return "Material.ScaleNormal"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto normal = context.Widen(
                context.Input(0),
                EMaterialValueType::Float3
            );
            const auto scale = context.Widen(
                context.Input(1),
                EMaterialValueType::Float
            );

            return {
                .Code = "normalize(float3((" + normal + ").xy * " + scale +
                    ", (" + normal + ").z))",
                .ValueType = EMaterialValueType::Float3
            };
        }
    };
}
