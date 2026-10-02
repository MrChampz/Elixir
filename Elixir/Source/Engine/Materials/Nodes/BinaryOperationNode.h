#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Combines two values, widening operands to a shared width.
     */
    class BinaryOperationNode : public MaterialNode
    {
    protected:
        BinaryOperationNode()
          : MaterialNode({
                { "A", EMaterialValueType::Float4, "0.0" },
                { "B", EMaterialValueType::Float4, "0.0" }
            }) {}

        static EMaterialValueType GetOutputType(const MaterialEmitContext& context)
        {
            return MaterialEmitContext::Wider(
                context.Input(0).ValueType,
                context.Input(1).ValueType
            );
        }
    };
}