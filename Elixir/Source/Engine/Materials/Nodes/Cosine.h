#pragma once

#include <Engine/Materials/Nodes/UnaryOperationNode.h>

namespace Elixir::Materials::Nodes
{
    /** @brief Applies cosine to every component. */
    class Cosine final : public UnaryOperationNode
    {
    public:
        std::string_view GetTypeName() const override { return "Material.Cosine"; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            const auto& input = context.Input(0);
            return {
                .Code = "cos(" + input.Code + ")",
                .ValueType = input.ValueType,
            };
        }
    };
}
