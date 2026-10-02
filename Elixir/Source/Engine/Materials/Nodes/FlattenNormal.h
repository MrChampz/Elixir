#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Blends a tangent-space normal toward the flat normal.
     *
     * A flatness of zero returns the flat tangent-space normal. A flatness of one
     * preserves the input normal.
     */
    class FlattenNormal final : public MaterialNode
    {
    public:
        FlattenNormal()
          : MaterialNode({{
                "Normal",
                EMaterialValueType::Float3,
                "float3(0.0, 0.0, 1.0)",
                EMaterialValueType::Float3
            },
            {
                "Flatness",
                EMaterialValueType::Float,
                "1.0",
                EMaterialValueType::Float
            }}) {}

        std::string_view GetTypeName() const override { return "Material.FlattenNormal"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto normal = context.Widen(
                context.Input(0),
                EMaterialValueType::Float3
            );

            const auto flatness = context.Widen(
                context.Input(1),
                EMaterialValueType::Float
            );

            return {
                .Code = "normalize(float3((" + normal + ").xy * " +
                    flatness + ", lerp(1.0, (" + normal + ").z, " +
                    flatness + ")))",
                .ValueType = EMaterialValueType::Float3
            };
        }
    };
}
