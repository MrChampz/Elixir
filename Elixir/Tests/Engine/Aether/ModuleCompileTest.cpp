#include <gtest/gtest.h>
#include <bit>
#include <stdexcept>

#include <Engine/Aether/System.h>
#include <Engine/Aether/Core/Operand.h>
#include <Engine/Aether/Core/Operation.h>
#include <Engine/Aether/Core/OperationInputs.h>
#include <Engine/Aether/Modules/ApplyGravity.h>
#include <Engine/Aether/Modules/ApplyVortex.h>
#include <Engine/Aether/Modules/ScaleOverLife.h>
#include <Engine/Aether/Modules/SetLifetime.h>
#include <Engine/Aether/Modules/SetVelocityCone.h>
#include <Engine/Aether/Modules/SetPositionVortexRibbonPath.h>
#include <Engine/Aether/Modules/SetRibbonId.h>
#include <Engine/Aether/Modules/SetRibbonIdFromSpawnOrder.h>

#include "TestInstanceRegistry.h"

using namespace Elixir;
using namespace Elixir::Aether;
using namespace Elixir::Aether::Core;
using namespace Elixir::Aether::Modules;

static_assert(std::is_abstract_v<Module>);
static_assert(std::is_abstract_v<SpawnModule>);
static_assert(std::is_abstract_v<UpdateModule>);

namespace
{
    /** @brief Emits two markers to verify dispatch of custom particle modules. */
    class MarkerModule final : public Module
    {
    public:
        /** @brief Creates a marker module for the supplied phase. */
        MarkerModule(EModulePhase phase, float marker) : Module(phase), m_Marker(marker) {}

        /** @brief Appends two marker operations to the compilation context. */
        void Compile(ModuleCompileContext& context) const override
        {
            context.Emit(EParticleOp::SetLiteral, EParticleAttribute::Color, { context.Value(glm::vec4(m_Marker)) });
            context.Emit(EParticleOp::SetLiteral, EParticleAttribute::Color, { context.Value(glm::vec4(m_Marker + 1.0f)) });
        }

    private:
        float m_Marker;
    };
}

TEST(ModuleCompileTest, DispatchesCustomModulesInPhaseOrderAndPreservesOffsets)
{
    const auto system = CreateRef<System>("Module dispatch");
    auto& emitter = system->AddEmitter("First", 8, 0.0f);
    emitter.AddModule(CreateScope<MarkerModule>(EModulePhase::Update, 30.0f));
    emitter.AddModule(CreateScope<MarkerModule>(EModulePhase::Spawn, 10.0f));
    emitter.AddModule(CreateScope<MarkerModule>(EModulePhase::Spawn, 20.0f));
    auto& second = system->AddEmitter("Second", 8, 0.0f);
    second.AddModule(CreateScope<MarkerModule>(EModulePhase::Update, 40.0f));

    TestInstanceRegistry runtime;
    const auto instance = runtime.CreateRegisteredInstance(system);
    ASSERT_TRUE(instance);
    Elixir::Aether::Rendering::FrameSubmission submission;
    ASSERT_TRUE(submission.Submit(*instance));
    const auto& compiled = submission.GetRenderProxies().front()->GetCompiledSystem();

    ASSERT_EQ(compiled.Emitters.size(), 2u);
    ASSERT_EQ(compiled.Operations.Ops.size(), 8u);
    EXPECT_EQ(compiled.Emitters[0].SpawnOpOffset, 0u);
    EXPECT_EQ(compiled.Emitters[0].SpawnOpCount, 4u);
    EXPECT_EQ(compiled.Emitters[0].UpdateOpOffset, 4u);
    EXPECT_EQ(compiled.Emitters[0].UpdateOpCount, 2u);
    EXPECT_EQ(compiled.Emitters[1].SpawnOpOffset, 6u);
    EXPECT_EQ(compiled.Emitters[1].SpawnOpCount, 0u);
    EXPECT_EQ(compiled.Emitters[1].UpdateOpOffset, 6u);
    EXPECT_EQ(compiled.Emitters[1].UpdateOpCount, 2u);
    const std::array expected{ 10.0f, 11.0f, 20.0f, 21.0f, 30.0f, 31.0f, 40.0f, 41.0f };
    for (size_t i = 0; i < expected.size(); ++i)
        EXPECT_FLOAT_EQ(std::bit_cast<float>(compiled.Operations.Constants[compiled.Operations.Operands[compiled.Operations.Ops[i].OperandOffset] & OperandIndexMask]), expected[i]);
}

namespace
{
    uint32_t Reference(const SCompiledOperations& program, size_t operation, uint32_t argument)
    {
        return program.Operands.at(program.Ops.at(operation).OperandOffset + argument);
    }

    float Literal(const SCompiledOperations& program, size_t operation, uint32_t argument, uint32_t component = 0)
    {
        return std::bit_cast<float>(program.Constants.at((Reference(program, operation, argument) & OperandIndexMask) + component));
    }

    uint32_t Encoded(EOperandSource source, uint32_t index)
    {
        return (uint32_t(source) << 29) | index;
    }
}

TEST(ModuleCompileTest, ResolvesLocalParametersThenSystemParametersAndMissingValues)
{
    const std::vector<SParameter> parameters{
        { .Name = "Low", .Value = glm::vec4(1.0f) }, { .Name = "Smoke.Low", .Value = glm::vec4(2.0f) }, { .Name = "High", .Value = glm::vec4(3.0f) },
    };
    SCompiledOperations program;
    ModuleCompileContext context(program, parameters, "Smoke", 2.5f);
    SetLifetime(1.0f, 4.0f).BindParameters("Low", "High").Compile(context);
    ApplyGravity({ 0.0f, -4.0f, 0.0f }).BindParameter("Missing").Compile(context);
    ApplyVortex({ 1.0f, 2.0f, 3.0f }, 4.0f, 5.0f)
        .BindParameters("Low", "High", "Missing", "Missing").Compile(context);

    ASSERT_EQ(program.Ops.size(), 3u);
    EXPECT_EQ(Reference(program, 0, Inputs::RandomRange_Minimum), Encoded(EOperandSource::Parameter, 1));
    EXPECT_EQ(Reference(program, 0, Inputs::RandomRange_Maximum), Encoded(EOperandSource::Parameter, 2));
    EXPECT_FLOAT_EQ(Literal(program, 1, Inputs::AddWithDelta_Value, 1), -10.0f);
    EXPECT_EQ(Reference(program, 2, Inputs::ApplyVortex_Center), Encoded(EOperandSource::Parameter, 1));
    EXPECT_EQ(Reference(program, 2, Inputs::ApplyVortex_Tangential), Encoded(EOperandSource::Parameter, 2));
    EXPECT_FLOAT_EQ(Literal(program, 2, Inputs::ApplyVortex_Radial), 5.0f);
}

TEST(ModuleCompileTest, CompilesScaleCurveWithRangeOffsetAndClamp)
{
    const std::vector<SParameter> parameters{ { .Name = "Curve:0", .Value = glm::vec4(0) }, { .Name = "Smoke.Curve:0", .Value = glm::vec4(1) } };
    SCompiledOperations program;
    ModuleCompileContext context(program, parameters, "Smoke", 1.0f);
    ScaleOverLife(1.0f, 3.0f).BindCurve("Curve", EDynamicInput::EmitterTime).Compile(context);

    ASSERT_EQ(program.Ops.size(), 5u);
    EXPECT_EQ(Reference(program, 0, Inputs::SampleCurve_Curve), Encoded(EOperandSource::Curve, 1));
    EXPECT_EQ(Reference(program, 0, Inputs::SampleCurve_Input), Encoded(EOperandSource::DynamicInput, uint32_t(EDynamicInput::EmitterTime)));
    EXPECT_EQ(program.Ops[1].Type, EParticleOp::Mul);
    EXPECT_FLOAT_EQ(Literal(program, 1, Inputs::Mul_Value), 2.0f);
    EXPECT_EQ(program.Ops[2].Type, EParticleOp::Add);
    EXPECT_FLOAT_EQ(Literal(program, 2, Inputs::Add_Value), 1.0f);
    for (uint32_t i = 0; i < 4; ++i)
    {
        EXPECT_FLOAT_EQ(Literal(program, 3, Inputs::Clamp_Minimum, i), 0.0f);
        EXPECT_FLOAT_EQ(Literal(program, 3, Inputs::Clamp_Maximum, i), 4.0f);
    }
    EXPECT_EQ(program.Ops[4].Target, EParticleAttribute::Scale);
    EXPECT_EQ(Reference(program, 4, Inputs::CopyFromAttribute_Value), Encoded(EOperandSource::ParticleAttribute, uint32_t(EParticleAttribute::Temp0)));
}

TEST(ModuleCompileTest, CompilesConeBindingsAndLiteralFallbacks)
{
    const std::vector<SParameter> parameters{
        { .Name = "Angle", .Value = glm::vec4(0.5f) }, { .Name = "Smoke.Angle", .Value = glm::vec4(1) }, { .Name = "Low", .Value = glm::vec4(2) }, { .Name = "Smoke.High", .Value = glm::vec4(3) },
    };
    SCompiledOperations program;
    ModuleCompileContext context(program, parameters, "Smoke", 1.0f);
    SetVelocityCone({ 0, 1, 0 }, 0.25f, 4.0f, 5.0f).BindParameters("Angle", "Low", "High").Compile(context);
    SetVelocityCone({ 0, 1, 0 }, 0.25f, 4.0f, 5.0f).BindParameters("Missing", "Low", "Missing").Compile(context);
    SetVelocityCone({ 0, 1, 0 }, 0.25f, 4.0f, 5.0f).Compile(context);

    ASSERT_EQ(program.Ops.size(), 3u);
    EXPECT_EQ(Reference(program, 0, Inputs::SampleCone_Angle), Encoded(EOperandSource::Parameter, 1));
    EXPECT_EQ(Reference(program, 0, Inputs::SampleCone_MinSpeed), Encoded(EOperandSource::Parameter, 2));
    EXPECT_EQ(Reference(program, 0, Inputs::SampleCone_MaxSpeed), Encoded(EOperandSource::Parameter, 3));
    EXPECT_FLOAT_EQ(Literal(program, 1, Inputs::SampleCone_Angle), 0.25f);
    EXPECT_EQ(Reference(program, 1, Inputs::SampleCone_MinSpeed), Encoded(EOperandSource::Parameter, 2));
    EXPECT_FLOAT_EQ(Literal(program, 1, Inputs::SampleCone_MaxSpeed), 5.0f);
    EXPECT_FLOAT_EQ(Literal(program, 2, Inputs::SampleCone_MinSpeed), 4.0f);
    for (size_t i = 0; i < 3; ++i)
    {
        EXPECT_EQ(program.Ops[i].Type, EParticleOp::SampleCone);
        EXPECT_EQ(program.Ops[i].OperandCount, 4u);
        EXPECT_FLOAT_EQ(Literal(program, i, Inputs::SampleCone_Direction, 1), 1.0f);
    }
    EXPECT_EQ(Reference(program, 0, Inputs::SampleCone_Direction), Reference(program, 2, Inputs::SampleCone_Direction));
}

TEST(ModuleCompileTest, PacksAndInternsTypedConstantsWithoutFloat4Padding)
{
    SCompiledOperations program;
    const std::vector<SParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    const auto scalar = context.Scalar(1);
    EXPECT_EQ(context.Scalar(1).Reference, scalar.Reference);
    context.Vector({1, 2, 3});
    context.Value({1, 2, 3, 4});
    EXPECT_EQ(program.Constants.size(), 8u);
    EXPECT_NE(context.Scalar(0).Reference, context.Scalar(-0.0f).Reference);
    const auto integer = context.UInt(UINT32_MAX);
    EXPECT_EQ(program.Constants[integer.Reference & OperandIndexMask], UINT32_MAX);
    EXPECT_EQ(context.UInt(17).Reference, Encoded(EOperandSource::ImmediateUInt, 17));
}

TEST(ModuleCompileTest, ValidatesContractsAndNamedParameterTypes)
{
    const std::vector<SParameter> parameters{
        { .Name = "Scalar", .Type = EParameterType::Scalar, .Value = glm::vec4(1) }, { .Name = "Vector", .Type = EParameterType::Vector, .Value = glm::vec4(1) },
    };
    SCompiledOperations program;
    ModuleCompileContext context(program, parameters, "", 1);
    EXPECT_THROW(context.Scalar(0, "Vector"), std::invalid_argument);
    EXPECT_THROW(context.Vector(glm::vec3(0), "Scalar"), std::invalid_argument);
    EXPECT_THROW(context.Emit(EParticleOp::SampleCone, EParticleAttribute::Velocity, {context.Scalar(1)}), std::invalid_argument);
    EXPECT_THROW(context.Emit(EParticleOp::SetLiteral, EParticleAttribute::Position, {context.Scalar(1)}), std::invalid_argument);
    EXPECT_TRUE(program.Ops.empty());
    EXPECT_TRUE(program.Operands.empty());
    EXPECT_EQ(context.Curve("Missing").Reference, Encoded(EOperandSource::Curve, OperandIndexMask));
}

TEST(ModuleCompileTest, IgnoresNullModulesBeforeCompilingValidModules)
{
    const auto system = CreateRef<System>("Null modules");
    auto& emitter = system->AddEmitter("Smoke", 8, 0.0f);
    emitter.AddModule(nullptr);
    emitter.AddModule(CreateScope<SetLifetime>(1.0f, 2.0f));
    emitter.AddModule(nullptr);

    TestInstanceRegistry runtime;
    const auto instance = runtime.CreateRegisteredInstance(system);
    ASSERT_TRUE(instance);
    Elixir::Aether::Rendering::FrameSubmission submission;
    ASSERT_TRUE(submission.Submit(*instance));
    const auto& compiled = submission.GetRenderProxies().front()->GetCompiledSystem();
    ASSERT_EQ(compiled.Emitters.size(), 1u);
    ASSERT_EQ(compiled.Operations.Ops.size(), 1u);
    EXPECT_EQ(compiled.Emitters[0].SpawnOpCount, 1u);
    EXPECT_EQ(compiled.Operations.Ops[0].Type, EParticleOp::RandomRange);
}

TEST(ModuleCompileTest, SupportsNineArgumentsWithoutPayloadStructs)
{
    SCompiledOperations program;
    const std::vector<SParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    SetPositionVortexRibbonPath({1, 2, 3}, 4, 5, 6, 7, 8, 9, 10, 11).Compile(context);
    ASSERT_EQ(program.Ops.size(), 1u);
    EXPECT_EQ(program.Ops[0].OperandCount, 9u);
    EXPECT_EQ(program.Operands.size(), 9u);
    EXPECT_EQ(program.Constants.size(), 11u);
    program.CompactConstants();
    EXPECT_EQ(program.Constants.size(), 11u);
    EXPECT_FLOAT_EQ(Literal(program, 0, Inputs::SetPositionVortexRibbonPath_Center, 2), 3);
    EXPECT_FLOAT_EQ(Literal(program, 0, Inputs::SetPositionVortexRibbonPath_DepthAmplitude), 11);
}

TEST(ModuleCompileTest, KeepsLargeRibbonIntegersExactUntilParticleStorage)
{
    SCompiledOperations program;
    const std::vector<SParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    SetRibbonId(UINT32_MAX).Compile(context);
    SetRibbonIdFromSpawnOrder(17, UINT32_MAX).Compile(context);
    const auto identifier = Reference(program, 0, Inputs::SetLiteral_Value);
    EXPECT_EQ(identifier >> 29, uint32_t(EOperandSource::Constant));
    EXPECT_EQ(program.Constants.at(identifier & OperandIndexMask), UINT32_MAX);
    EXPECT_EQ(Reference(program, 1, Inputs::SetRibbonIdFromSpawnOrder_FirstRibbonId), identifier);
    EXPECT_EQ(Reference(program, 1, Inputs::SetRibbonIdFromSpawnOrder_RibbonCount), Encoded(EOperandSource::ImmediateUInt, 17));
}

TEST(ModuleCompileTest, RejectsMalformedReferencesBeforeEmittingInstructions)
{
    SCompiledOperations program;
    const std::vector<SParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    EXPECT_THROW(context.Emit(EParticleOp::SetLiteral, EParticleAttribute::Scale,
        {{EOperandType::Scalar, Encoded(EOperandSource::Constant, 0)}}), std::invalid_argument);
    EXPECT_THROW(context.Emit(EParticleOp::SetLiteral, EParticleAttribute::Scale,
        {context.Dynamic(static_cast<EDynamicInput>(99))}), std::invalid_argument);
    EXPECT_THROW(context.Emit(EParticleOp::SetLiteral, EParticleAttribute::Scale,
        {context.Attribute(EParticleAttribute::Color, EOperandType::Scalar)}), std::invalid_argument);
    EXPECT_THROW(context.Emit(EParticleOp::SetLiteral, EParticleAttribute::Temp0,
        {context.Curve("Missing")}), std::invalid_argument);
    EXPECT_TRUE(program.Ops.empty());
    EXPECT_TRUE(program.Operands.empty());
}

TEST(ModuleCompileTest, UsesTwentyFourBytesForOneScalarInstruction)
{
    SCompiledOperations program;
    const std::vector<SParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    context.Emit(EParticleOp::SetLiteral, EParticleAttribute::Scale, {context.Scalar(2)});
    program.CompactConstants();
    EXPECT_EQ(program.Ops.size() * sizeof(SOperation) +
        (program.Operands.size() + program.Constants.size()) * sizeof(uint32_t), 24u);
}

TEST(ModuleCompileTest, PreservesScalarBitsAndSignedZeroInConstants)
{
    SCompiledOperations program;
    const std::vector<SParameter> parameters{{ .Name = "Bound", .Type = EParameterType::Scalar, .Value = glm::vec4(3) }};
    ModuleCompileContext context(program, parameters, "", 1);
    const uint32_t bits = 0xe0000001u;
    context.Emit(EParticleOp::RandomRange, EParticleAttribute::Scale,
        {context.Scalar(std::bit_cast<float>(bits)), context.Scalar(0, "Bound")});
    context.Emit(EParticleOp::RandomRange, EParticleAttribute::Scale,
        {context.Scalar(0.0f), context.Scalar(-0.0f)});
    program.CompactConstants();
    EXPECT_EQ(program.Ops[0].OperandCount, 2u);
    EXPECT_EQ(program.Ops[1].OperandCount, 2u);
    EXPECT_EQ(Reference(program, 0, 0) >> 29, uint32_t(EOperandSource::Constant));
    EXPECT_EQ(program.Constants.at(Reference(program, 0, 0) & OperandIndexMask), bits);
    EXPECT_EQ(Reference(program, 0, 1), Encoded(EOperandSource::Parameter, 0));
    EXPECT_EQ(program.Constants.at(Reference(program, 1, 0) & OperandIndexMask), 0u);
    EXPECT_EQ(program.Constants.at(Reference(program, 1, 1) & OperandIndexMask), 0x80000000u);
    EXPECT_NE(Reference(program, 1, 0), Reference(program, 1, 1));
    EXPECT_EQ(program.Constants.size(), 3u);
}

TEST(ModuleCompileTest, CompactsRemainingConstantsAndPreservesSharingAndIntegerBits)
{
    SCompiledOperations program;
    const std::vector<SParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    context.Scalar(99);
    context.Emit(EParticleOp::SetLiteral, EParticleAttribute::Scale, {context.Scalar(2)});
    context.Emit(EParticleOp::SetLiteral, EParticleAttribute::Position, {context.Vector({1, 2, 3})});
    context.Emit(EParticleOp::SetLiteral, EParticleAttribute::Velocity, {context.Vector({1, 2, 3})});
    SetRibbonId(UINT32_MAX).Compile(context);
    program.CompactConstants();
    ASSERT_EQ(program.Constants.size(), 5u);
    EXPECT_FLOAT_EQ(Literal(program, 0, 0), 2);
    EXPECT_EQ(Reference(program, 1, 0), Reference(program, 2, 0));
    EXPECT_FLOAT_EQ(Literal(program, 1, 0, 2), 3);
    EXPECT_EQ(program.Constants.at(Reference(program, 3, 0) & OperandIndexMask), UINT32_MAX);
    const auto constants = program.Constants;
    const auto operands = program.Operands;
    program.CompactConstants();
    EXPECT_EQ(program.Constants, constants);
    EXPECT_EQ(program.Operands, operands);
}

TEST(ParameterStoreTest, ReplacingAParameterTypeKeepsItsNameUnique)
{
    ParameterStore store;
    store.SetFloat("Value", 1);
    store.SetFloat4("Value", glm::vec4(2));
    auto parameters = store.Compile("Emitter.");
    ASSERT_EQ(parameters.size(), 1u);
    EXPECT_EQ(parameters[0].Name, "Emitter.Value");
    EXPECT_EQ(parameters[0].Type, EParameterType::Vector);
    EXPECT_EQ(parameters[0].Value, glm::vec4(2));
    store.SetFloat("Value", 3);
    parameters = store.Compile();
    ASSERT_EQ(parameters.size(), 1u);
    EXPECT_EQ(parameters[0].Type, EParameterType::Scalar);
    EXPECT_EQ(parameters[0].Value, glm::vec4(3, 0, 0, 0));
}
