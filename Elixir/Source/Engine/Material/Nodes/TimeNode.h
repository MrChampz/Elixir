#pragma once

#include <Engine/Material/MaterialGraph.h>

namespace Elixir::MaterialNodes
{
    /** @brief Outputs elapsed time in seconds. */
    class TimeNode final : public IMaterialNode
    {
    public:
        std::string_view GetTypeName() const override { return "material.time"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }
        SMaterialExpression Emit(const SMaterialEmitContext&) const override { return { "Time", EMaterialValueType::Float }; }

    private:
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}
