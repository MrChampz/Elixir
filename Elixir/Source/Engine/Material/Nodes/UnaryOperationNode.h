#pragma once

#include <Engine/Material/MaterialGraph.h>

namespace Elixir::MaterialNodes
{
    /** @brief Defines a supported single-input material operation. */
    enum class EUnaryMaterialOperation : uint8_t
    {
        /** @brief Applies sine to every component. */
        Sine,
        /** @brief Subtracts every component from one. */
        OneMinus,
        /** @brief Clamps every component to the zero-to-one range. */
        Saturate,
    };

    /** @brief Applies a single-input operation while preserving input width. */
    class UnaryOperationNode final : public IMaterialNode
    {
    public:
        /** @brief Creates a unary operation node. */
        explicit UnaryOperationNode(const EUnaryMaterialOperation operation)
            : m_Operation(operation), m_Inputs{{ "Value", EMaterialValueType::Float4, "0.0" }} {}

        std::string_view GetTypeName() const override
        {
            switch (m_Operation)
            {
                case EUnaryMaterialOperation::Sine: return "material.sine";
                case EUnaryMaterialOperation::OneMinus: return "material.one_minus";
                case EUnaryMaterialOperation::Saturate: return "material.saturate";
            }
            return "material.unary";
        }

        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }
        SMaterialExpression Emit(const SMaterialEmitContext& context) const override
        {
            const auto& input = context.Input(0);
            switch (m_Operation)
            {
                case EUnaryMaterialOperation::Sine: return { "sin(" + input.Code + ")", input.ValueType };
                case EUnaryMaterialOperation::OneMinus: return { "(1.0 - " + input.Code + ")", input.ValueType };
                case EUnaryMaterialOperation::Saturate: return { "saturate(" + input.Code + ")", input.ValueType };
            }
            return { "0.0", EMaterialValueType::Float };
        }

    private:
        EUnaryMaterialOperation m_Operation;
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}
