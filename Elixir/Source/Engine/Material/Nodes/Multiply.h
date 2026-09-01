#pragma once

#include <Engine/Material/Nodes/BinaryOperationNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Multiplies two values component by component.
     */
    class Multiply final : public BinaryOperationNode
    {
    public:
        std::string_view GetTypeName() const override { return "Material.Multiply"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto outputType = GetOutputType(context);
            const auto a = context.Widen(context.Input(0), outputType);
            const auto b = context.Widen(context.Input(1), outputType);
            return {
                .Code = "(" + a + " * " + b + ")",
                .ValueType = outputType
            };
        }
    };
}