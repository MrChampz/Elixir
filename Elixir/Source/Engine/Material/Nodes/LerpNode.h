#pragma once

#include <Engine/Material/MaterialGraph.h>

namespace Elixir::MaterialNodes
{
    /** @brief Linearly interpolates between two values. */
    class LerpNode final : public IMaterialNode
    {
    public:
        LerpNode() : m_Inputs{{ "A", EMaterialValueType::Float4, "0.0" }, { "B", EMaterialValueType::Float4, "0.0" }, { "T", EMaterialValueType::Float4, "0.0" }} {}
        std::string_view GetTypeName() const override { return "material.lerp"; }
        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }
        SMaterialExpression Emit(const SMaterialEmitContext& context) const override
        {
            const auto type = SMaterialEmitContext::Wider(context.Input(0).ValueType, context.Input(1).ValueType);
            return { "lerp(" + context.Widen(context.Input(0), type) + ", " + context.Widen(context.Input(1), type) + ", " + context.Widen(context.Input(2), type) + ")", type };
        }

    private:
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}
