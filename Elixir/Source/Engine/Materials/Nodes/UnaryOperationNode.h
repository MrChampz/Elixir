#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Applies a single-input operation while preserving input width.
     */
    class UnaryOperationNode : public MaterialNode
    {
    protected:
        UnaryOperationNode()
          : m_Inputs{{
              "Value",
              EMaterialValueType::Float4,
              "0.0"
          }} {}
    };
}