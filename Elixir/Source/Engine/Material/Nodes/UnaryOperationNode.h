#pragma once

#include <vector>
#include <string_view>

#include <Engine/Material/Material.h>
#include <Engine/Material/MaterialGraph.h>

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