#pragma once

#include <Engine/Material/MaterialGraph.h>

namespace Elixir::MaterialNodes
{
    /** @brief Defines a supported two-input material operation. */
    enum class EBinaryMaterialOperation : uint8_t
    {
        /** @brief Adds two values component by component. */
        Add,
        /** @brief Subtracts the second value from the first. */
        Subtract,
        /** @brief Multiplies two values component by component. */
        Multiply,
        /** @brief Divides the first value by the second. */
        Divide,
        /** @brief Raises the first value to the second. */
        Power,
        /** @brief Calculates the dot product of two values. */
        Dot,
    };

    /** @brief Combines two values, widening operands to a shared width. */
    class BinaryOperationNode final : public IMaterialNode
    {
    public:
        /** @brief Creates a binary operation node. */
        explicit BinaryOperationNode(const EBinaryMaterialOperation operation)
            : m_Operation(operation),
              m_Inputs{{ "A", EMaterialValueType::Float4, "0.0" }, { "B", EMaterialValueType::Float4, "0.0" }} {}

        std::string_view GetTypeName() const override
        {
            switch (m_Operation)
            {
                case EBinaryMaterialOperation::Add: return "material.add";
                case EBinaryMaterialOperation::Subtract: return "material.subtract";
                case EBinaryMaterialOperation::Multiply: return "material.multiply";
                case EBinaryMaterialOperation::Divide: return "material.divide";
                case EBinaryMaterialOperation::Power: return "material.power";
                case EBinaryMaterialOperation::Dot: return "material.dot";
            }
            return "material.binary";
        }

        const std::vector<SMaterialNodeInput>& GetInputs() const override { return m_Inputs; }
        SMaterialExpression Emit(const SMaterialEmitContext& context) const override
        {
            const auto outputType = SMaterialEmitContext::Wider(context.Input(0).ValueType, context.Input(1).ValueType);
            const auto a = context.Widen(context.Input(0), outputType);
            const auto b = context.Widen(context.Input(1), outputType);

            switch (m_Operation)
            {
                case EBinaryMaterialOperation::Add: return { "(" + a + " + " + b + ")", outputType };
                case EBinaryMaterialOperation::Subtract: return { "(" + a + " - " + b + ")", outputType };
                case EBinaryMaterialOperation::Multiply: return { "(" + a + " * " + b + ")", outputType };
                case EBinaryMaterialOperation::Divide: return { "(" + a + " / " + b + ")", outputType };
                case EBinaryMaterialOperation::Power: return { "pow(" + a + ", " + b + ")", outputType };
                case EBinaryMaterialOperation::Dot: return { "dot(" + a + ", " + b + ")", EMaterialValueType::Float };
            }
            return { "0.0", EMaterialValueType::Float };
        }

    private:
        EBinaryMaterialOperation m_Operation;
        std::vector<SMaterialNodeInput> m_Inputs;
    };
}
