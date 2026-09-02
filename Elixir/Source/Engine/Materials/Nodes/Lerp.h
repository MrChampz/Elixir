#pragma once

#include <Engine/Material/Material.h>
#include <Engine/Material/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Linearly interpolates between two values.
     */
    class Lerp final : public MaterialNode
    {
    public:
        Lerp()
          : MaterialNode({
                { "A", EMaterialValueType::Float4, "0.0" },
                { "B", EMaterialValueType::Float4, "0.0" },
                { "T", EMaterialValueType::Float4, "0.0" }
            }) {}

        std::string_view GetTypeName() const override { return "Material.Lerp"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto type = MaterialEmitContext::Wider(
                context.Input(0).ValueType,
                context.Input(1).ValueType
            );

            return {
                .Code = "lerp(" + context.Widen(context.Input(0), type) + ", " +
                    context.Widen(context.Input(1), type) + ", " +
                    context.Widen(context.Input(2), type) + ")",
                .ValueType = type
            };
        }
    };
}