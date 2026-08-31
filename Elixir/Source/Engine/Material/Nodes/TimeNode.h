#pragma once

#include <string_view>
#include <vector>

#include <Engine/Material/Material.h>
#include <Engine/Material/MaterialGraph.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Outputs elapsed time in seconds.
     */
    class TimeNode : public MaterialNode
    {
    public:
        std::string_view GetTypeName() const override { return "material.time"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            return { .Code = "Time", .ValueType = EMaterialValueType::Float };
        }

    private:
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}