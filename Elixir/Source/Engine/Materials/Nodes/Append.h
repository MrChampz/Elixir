#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /** @brief Appends two scalar or vector values into one vector. */
    class Append final : public MaterialNode
    {
    public:
        /** @brief Creates a vector append node. */
        Append()
          : MaterialNode({{
                "A",
                EMaterialValueType::Float4,
                "0.0",
                EMaterialValueType::Float
            }, {
                "B",
                EMaterialValueType::Float4,
                "0.0",
                EMaterialValueType::Float
            }}) {}

        std::string_view GetTypeName() const override { return "Material.Append"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto& a = context.Input(0);
            const auto& b = context.Input(1);
            const int componentCount =
                MaterialEmitContext::Components(a.ValueType) +
                MaterialEmitContext::Components(b.ValueType);

            if (componentCount < 2 || componentCount > 4)
            {
                return {
                    .Code = "float2(0.0, 0.0)",
                    .ValueType = EMaterialValueType::Float2
                };
            }

            const auto valueType = static_cast<EMaterialValueType>(componentCount - 1);
            return {
                .Code = std::string(MaterialEmitContext::TypeName(valueType)) +
                    "(" + a.Code + ", " + b.Code + ")",
                .ValueType = valueType
            };
        }
    };
}
