#pragma once

#include <Engine/Material/Material.h>
#include <Engine/Material/MaterialNode.h>

namespace Elixir::Materials::Nodes
{
    /**
     * @brief Outputs elapsed time in seconds.
     */
    class Time final : public MaterialNode
    {
    public:
        std::string_view GetTypeName() const override { return "Material.Time"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }

        SMaterialExpression Emit(const MaterialEmitContext& context) const override
        {
            return { .Code = "Time", .ValueType = EMaterialValueType::Float };
        }

    private:
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}