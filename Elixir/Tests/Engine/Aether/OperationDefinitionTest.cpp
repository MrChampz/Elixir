#include <gtest/gtest.h>

#include <Engine/Aether/Core/Operations/OperationRegistry.h>
#include <Engine/Aether/Modules/Module.h>

using namespace Elixir::Aether::Core;
using namespace Elixir::Aether::Modules;

namespace
{
    class OperationDefinitionTest : public testing::TestWithParam<const SOperationDefinition*> {};

    SOperand MakeOperand(ModuleCompileContext& context, EOperandType type)
    {
        switch (type)
        {
            case EOperandType::Target:
            case EOperandType::Scalar: return context.Scalar(2.0f);
            case EOperandType::Vector3: return context.Vector(glm::vec3(2.0f));
            case EOperandType::Vector4: return context.Value(glm::vec4(2.0f));
            case EOperandType::UInt: return context.UInt(UINT32_MAX);
            case EOperandType::Curve: return context.Curve("missing");
        }
        throw std::invalid_argument("Unknown operand type.");
    }
}

TEST_P(OperationDefinitionTest, ValidatesEveryInputAndKeepsEmissionAtomic)
{
    SCompiledOperations operations;
    const std::vector<SParameter> parameters;
    ModuleCompileContext context(operations, parameters, "Emitter", 1.0f);
    const auto* definition = FindOperationDefinition(GetParam()->Type);
    ASSERT_NE(definition, nullptr);
    std::vector<SOperand> operands;
    for (const auto type : definition->Inputs) operands.push_back(MakeOperand(context, type));

    // All current definitions have one to nine inputs; Emit uses an initializer list.
    const auto emit = [&] {
        switch (operands.size())
        {
            case 1: context.Emit(GetParam()->Type, EParticleAttribute::Temp0, {operands[0]}); break;
            case 2: context.Emit(GetParam()->Type, EParticleAttribute::Temp0, {operands[0], operands[1]}); break;
            case 3: context.Emit(GetParam()->Type, EParticleAttribute::Temp0, {operands[0], operands[1], operands[2]}); break;
            case 4: context.Emit(GetParam()->Type, EParticleAttribute::Temp0, {operands[0], operands[1], operands[2], operands[3]}); break;
            case 5: context.Emit(GetParam()->Type, EParticleAttribute::Temp0, {operands[0], operands[1], operands[2], operands[3], operands[4]}); break;
            case 6: context.Emit(GetParam()->Type, EParticleAttribute::Temp0, {operands[0], operands[1], operands[2], operands[3], operands[4], operands[5]}); break;
            case 7: context.Emit(GetParam()->Type, EParticleAttribute::Temp0, {operands[0], operands[1], operands[2], operands[3], operands[4], operands[5], operands[6]}); break;
            case 8: context.Emit(GetParam()->Type, EParticleAttribute::Temp0, {operands[0], operands[1], operands[2], operands[3], operands[4], operands[5], operands[6], operands[7]}); break;
            case 9: context.Emit(GetParam()->Type, EParticleAttribute::Temp0, {operands[0], operands[1], operands[2], operands[3], operands[4], operands[5], operands[6], operands[7], operands[8]}); break;
            default: FAIL() << "Unexpected current operation arity";
        }
    };
    ASSERT_NO_THROW(emit());
    ASSERT_EQ(operations.Ops.size(), 1u);
    EXPECT_EQ(operations.Ops[0].Type, GetParam()->Type);
    EXPECT_EQ(operations.Ops[0].OperandCount, operands.size());
    EXPECT_THROW(context.Emit(GetParam()->Type, EParticleAttribute::Temp0, {}), std::invalid_argument);
    for (size_t index = 0; index < operands.size(); ++index)
    {
        const auto original = operands[index];
        operands[index] = definition->Inputs[index] == EOperandType::UInt
            ? context.Scalar(1.0f) : context.UInt(1u);
        EXPECT_THROW(emit(), std::invalid_argument) << "argument " << index;
        operands[index] = original;
    }
    EXPECT_EQ(operations.Ops.size(), 1u);
    EXPECT_EQ(operations.Operands.size(), operands.size());
}

INSTANTIATE_TEST_SUITE_P(AllOperations, OperationDefinitionTest,
    testing::ValuesIn(GetOperationDefinitions()));

TEST(OperationCatalogTest, RejectsUnknownOperationWithoutWritingTables)
{
    SCompiledOperations operations;
    const std::vector<SParameter> parameters;
    ModuleCompileContext context(operations, parameters, "Emitter", 1.0f);
    const auto unknown = static_cast<EParticleOp>(UINT32_MAX);
    EXPECT_EQ(FindOperationDefinition(unknown), nullptr);
    EXPECT_THROW(context.Emit(unknown, EParticleAttribute::Temp0, {}), std::invalid_argument);
    EXPECT_TRUE(operations.Ops.empty());
    EXPECT_TRUE(operations.Operands.empty());
}
