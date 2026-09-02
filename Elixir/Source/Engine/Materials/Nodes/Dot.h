#pragma once

#include <Engine/Material/Nodes/BinaryOperationNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Calculates the dot product of two values.
     */
    class Dot final : public BinaryOperationNode
    {
    public:
        std::string_view GetTypeName() const override { return "Material.Dot"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto outputType = GetOutputType(context);
            const auto a = context.Widen(context.Input(0), outputType);
            const auto b = context.Widen(context.Input(1), outputType);
            return {
                .Code = "dot(" + a + ", " + b + ")",
                .ValueType = EMaterialValueType::Float
            };
        }
    };
}