#pragma once

#include <vector>
#include <string_view>

#include <Engine/Material/Material.h>
#include <Engine/Material/MaterialGraph.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Combines two values, widening operands to a shared width.
     */
    class BinaryOperationNode : public MaterialNode
    {
    public:
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }

    protected:
        BinaryOperationNode()
          : m_Inputs{
            { "A", EMaterialValueType::Float4, "0.0" },
            { "B", EMaterialValueType::Float4, "0.0" }
            } {}

        static EMaterialValueType GetOutputType(const MaterialEmitContext& context)
        {
            return MaterialEmitContext::Wider(
                context.Input(0).ValueType,
                context.Input(1).ValueType
            );
        }

    private:
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}