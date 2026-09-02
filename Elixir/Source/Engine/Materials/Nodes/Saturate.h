#pragma once

#include <Engine/Material/Nodes/UnaryOperationNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Clamps every component to the zero-to-one range.
     */
    class Saturate final : public UnaryOperationNode
    {
    public:
        std::string_view GetTypeName() const override { return "Material.Saturate"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto& input = context.Input(0);
            return {
                .Code = "saturate(" + input.Code + ")",
                .ValueType = input.ValueType,
            };
        }
    };
}