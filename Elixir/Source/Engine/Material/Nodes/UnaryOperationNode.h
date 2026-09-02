#pragma once

#include <Engine/Material/Material.h>
#include <Engine/Material/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Applies a single-input operation while preserving input width.
     */
    class UnaryOperationNode : public MaterialNode
    {
    public:
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }

    protected:
        UnaryOperationNode()
          : m_Inputs{{
              "Value",
              EMaterialValueType::Float4,
              "0.0"
          }} {}

    private:
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}