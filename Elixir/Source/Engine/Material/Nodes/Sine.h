#pragma once

#include <Engine/Material/Nodes/UnaryOperationNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Applies sine to every component.
     */
    class Sine final : public UnaryOperationNode
    {
    public:
        std::string_view GetTypeName() const override { return "Material.Sine"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto& input = context.Input(0);
            return {
                .Code = "sin(" + input.Code + ")",
                .ValueType = input.ValueType,
            };
        }
    };
}