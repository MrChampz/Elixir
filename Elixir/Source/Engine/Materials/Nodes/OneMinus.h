#pragma once

#include <Engine/Material/Nodes/UnaryOperationNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Subtracts every component from one.
     */
    class OneMinus final : public UnaryOperationNode
    {
    public:
        std::string_view GetTypeName() const override { return "Material.OneMinus"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto& input = context.Input(0);
            return {
                .Code = "(1.0 - " + input.Code + ")",
                .ValueType = input.ValueType,
            };
        }
    };
}