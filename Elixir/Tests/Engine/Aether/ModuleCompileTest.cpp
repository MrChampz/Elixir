#include <gtest/gtest.h>
#include <bit>
#include <cstring>

#include <Engine/Aether/System.h>
#include <Engine/Aether/Core/Operand.h>
#include <Engine/Aether/Core/Operation.h>
#include <Engine/Aether/Core/Operations/SetLiteral.h>
#include <Engine/Aether/Core/Operations/RandomRange.h>
#include <Engine/Aether/Core/Operations/SampleCone.h>
#include <Engine/Aether/Core/Operations/AddWithDelta.h>
#include <Engine/Aether/Core/Operations/SetPositionVortexRibbonPath.h>
#include <Engine/Aether/Core/Operations/SetRibbonIdFromSpawnOrder.h>
#include <Engine/Aether/Core/Operations/SampleCurve.h>
#include <Engine/Aether/Core/Operations/Add.h>
#include <Engine/Aether/Core/Operations/Mul.h>
#include <Engine/Aether/Core/Operations/Clamp.h>
#include <Engine/Aether/Core/Operations/CopyFromAttribute.h>
#include <Engine/Aether/Core/Operations/ApplyVortex.h>
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
            context.Emit(EOperationType::SetLiteral, EParticleAttribute::Color, { context.Value(glm::vec4(m_Marker)) });
            context.Emit(EOperationType::SetLiteral, EParticleAttribute::Color, { context.Value(glm::vec4(m_Marker + 1.0f)) });
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
    ASSERT_EQ(compiled.ModuleData.Operations.size(), 8u);
    EXPECT_EQ(compiled.Emitters[0].SpawnOperationOffset, 0u);
    EXPECT_EQ(compiled.Emitters[0].SpawnOperationCount, 4u);
    EXPECT_EQ(compiled.Emitters[0].UpdateOperationOffset, 4u);
    EXPECT_EQ(compiled.Emitters[0].UpdateOperationCount, 2u);
    EXPECT_EQ(compiled.Emitters[1].SpawnOperationOffset, 6u);
    EXPECT_EQ(compiled.Emitters[1].SpawnOperationCount, 0u);
    EXPECT_EQ(compiled.Emitters[1].UpdateOperationOffset, 6u);
    EXPECT_EQ(compiled.Emitters[1].UpdateOperationCount, 2u);
    const std::array expected{ 10.0f, 11.0f, 20.0f, 21.0f, 30.0f, 31.0f, 40.0f, 41.0f };
    for (size_t i = 0; i < expected.size(); ++i)
        EXPECT_FLOAT_EQ(std::bit_cast<float>(compiled.ModuleData.Constants[compiled.ModuleData.Operands[compiled.ModuleData.Operations[i].OperandOffset] & OperandIndexMask]), expected[i]);
}

namespace
{
    uint32_t Reference(const SCompiledModuleData& program, size_t operation, uint32_t argument)
    {
        return program.Operands.at(program.Operations.at(operation).OperandOffset + argument);
    }

    float Literal(const SCompiledModuleData& program, size_t operation, uint32_t argument, uint32_t component = 0)
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
    SCompiledModuleData program;
    ModuleCompileContext context(program, parameters, "Smoke", 2.5f);
    SetLifetime(1.0f, 4.0f).BindParameters("Low", "High").Compile(context);
    ApplyGravity({ 0.0f, -4.0f, 0.0f }).BindParameter("Missing").Compile(context);
    ApplyVortex({ 1.0f, 2.0f, 3.0f }, 4.0f, 5.0f)
        .BindParameters("Low", "High", "Missing", "Missing").Compile(context);

    ASSERT_EQ(program.Operations.size(), 3u);
    EXPECT_EQ(Reference(program, 0, Operations::RandomRange::Inputs::Minimum), Encoded(EOperandSource::Parameter, 1));
    EXPECT_EQ(Reference(program, 0, Operations::RandomRange::Inputs::Maximum), Encoded(EOperandSource::Parameter, 2));
    EXPECT_FLOAT_EQ(Literal(program, 1, Operations::AddWithDelta::Inputs::Value, 1), -10.0f);
    EXPECT_EQ(Reference(program, 2, Operations::ApplyVortex::Inputs::Center), Encoded(EOperandSource::Parameter, 1));
    EXPECT_EQ(Reference(program, 2, Operations::ApplyVortex::Inputs::Tangential), Encoded(EOperandSource::Parameter, 2));
    EXPECT_FLOAT_EQ(Literal(program, 2, Operations::ApplyVortex::Inputs::Radial), 5.0f);
}

TEST(ModuleCompileTest, CompilesScaleCurveWithRangeOffsetAndClamp)
{
    const std::vector<SParameter> parameters{ { .Name = "Curve:0", .Value = glm::vec4(0) }, { .Name = "Smoke.Curve:0", .Value = glm::vec4(1) } };
    SCompiledModuleData program;
    ModuleCompileContext context(program, parameters, "Smoke", 1.0f);
    ScaleOverLife(1.0f, 3.0f).BindCurve("Curve", EDynamicInput::EmitterTime).Compile(context);

    ASSERT_EQ(program.Operations.size(), 5u);
    EXPECT_EQ(Reference(program, 0, Operations::SampleCurve::Inputs::Curve), Encoded(EOperandSource::Curve, 1));
    EXPECT_EQ(Reference(program, 0, Operations::SampleCurve::Inputs::Input), Encoded(EOperandSource::DynamicInput, uint32_t(EDynamicInput::EmitterTime)));
    EXPECT_EQ(program.Operations[1].Type, EOperationType::Mul);
    EXPECT_FLOAT_EQ(Literal(program, 1, Operations::Mul::Inputs::Value), 2.0f);
    EXPECT_EQ(program.Operations[2].Type, EOperationType::Add);
    EXPECT_FLOAT_EQ(Literal(program, 2, Operations::Add::Inputs::Value), 1.0f);
    for (uint32_t i = 0; i < 4; ++i)
    {
        EXPECT_FLOAT_EQ(Literal(program, 3, Operations::Clamp::Inputs::Minimum, i), 0.0f);
        EXPECT_FLOAT_EQ(Literal(program, 3, Operations::Clamp::Inputs::Maximum, i), 4.0f);
    }
    EXPECT_EQ(program.Operations[4].Target, EParticleAttribute::Scale);
    EXPECT_EQ(Reference(program, 4, Operations::CopyFromAttribute::Inputs::Value), Encoded(EOperandSource::ParticleAttribute, uint32_t(EParticleAttribute::Temp0)));
}

TEST(ModuleCompileTest, CompilesConeBindingsAndLiteralFallbacks)
{
    const std::vector<SParameter> parameters{
        { .Name = "Angle", .Value = glm::vec4(0.5f) }, { .Name = "Smoke.Angle", .Value = glm::vec4(1) }, { .Name = "Low", .Value = glm::vec4(2) }, { .Name = "Smoke.High", .Value = glm::vec4(3) },
    };
    SCompiledModuleData program;
    ModuleCompileContext context(program, parameters, "Smoke", 1.0f);
    SetVelocityCone({ 0, 1, 0 }, 0.25f, 4.0f, 5.0f).BindParameters("Angle", "Low", "High").Compile(context);
    SetVelocityCone({ 0, 1, 0 }, 0.25f, 4.0f, 5.0f).BindParameters("Missing", "Low", "Missing").Compile(context);
    SetVelocityCone({ 0, 1, 0 }, 0.25f, 4.0f, 5.0f).Compile(context);

    ASSERT_EQ(program.Operations.size(), 3u);
    EXPECT_EQ(Reference(program, 0, Operations::SampleCone::Inputs::Angle), Encoded(EOperandSource::Parameter, 1));
    EXPECT_EQ(Reference(program, 0, Operations::SampleCone::Inputs::MinSpeed), Encoded(EOperandSource::Parameter, 2));
    EXPECT_EQ(Reference(program, 0, Operations::SampleCone::Inputs::MaxSpeed), Encoded(EOperandSource::Parameter, 3));
    EXPECT_FLOAT_EQ(Literal(program, 1, Operations::SampleCone::Inputs::Angle), 0.25f);
    EXPECT_EQ(Reference(program, 1, Operations::SampleCone::Inputs::MinSpeed), Encoded(EOperandSource::Parameter, 2));
    EXPECT_FLOAT_EQ(Literal(program, 1, Operations::SampleCone::Inputs::MaxSpeed), 5.0f);
    EXPECT_FLOAT_EQ(Literal(program, 2, Operations::SampleCone::Inputs::MinSpeed), 4.0f);
    for (size_t i = 0; i < 3; ++i)
    {
        EXPECT_EQ(program.Operations[i].Type, EOperationType::SampleCone);
        EXPECT_EQ(program.Operations[i].OperandCount, 4u);
        EXPECT_FLOAT_EQ(Literal(program, i, Operations::SampleCone::Inputs::Direction, 1), 1.0f);
    }
    EXPECT_EQ(Reference(program, 0, Operations::SampleCone::Inputs::Direction), Reference(program, 2, Operations::SampleCone::Inputs::Direction));
}

TEST(ModuleCompileTest, PacksAndInternsTypedConstantsWithoutFloat4Padding)
{
    SCompiledModuleData program;
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
    SCompiledModuleData program;
    ModuleCompileContext context(program, parameters, "", 1);
    const auto invalidScalar = context.Scalar(0, "Vector");
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Temp0, {invalidScalar});
    const auto invalidVector = context.Vector(glm::vec3(0), "Scalar");
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Temp0, {invalidVector});
    EXPECT_TRUE(program.Constants.empty());
    context.Emit(EOperationType::SampleCone, EParticleAttribute::Velocity, {context.Scalar(1)});
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Position, {context.Scalar(1)});
    EXPECT_TRUE(program.Operations.empty());
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
    ASSERT_EQ(compiled.ModuleData.Operations.size(), 1u);
    EXPECT_EQ(compiled.Emitters[0].SpawnOperationCount, 1u);
    EXPECT_EQ(compiled.ModuleData.Operations[0].Type, EOperationType::RandomRange);
}

TEST(ModuleCompileTest, SupportsNineArgumentsWithoutPayloadStructs)
{
    SCompiledModuleData program;
    const std::vector<SParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    SetPositionVortexRibbonPath({1, 2, 3}, 4, 5, 6, 7, 8, 9, 10, 11).Compile(context);
    ASSERT_EQ(program.Operations.size(), 1u);
    EXPECT_EQ(program.Operations[0].OperandCount, 9u);
    EXPECT_EQ(program.Operands.size(), 9u);
    EXPECT_EQ(program.Constants.size(), 11u);
    program.CompactConstants();
    EXPECT_EQ(program.Constants.size(), 11u);
    EXPECT_FLOAT_EQ(Literal(program, 0, Operations::SetPositionVortexRibbonPath::Inputs::Center, 2), 3);
    EXPECT_FLOAT_EQ(Literal(program, 0, Operations::SetPositionVortexRibbonPath::Inputs::DepthAmplitude), 11);
}

TEST(ModuleCompileTest, KeepsLargeRibbonIntegersExactUntilParticleStorage)
{
    SCompiledModuleData program;
    const std::vector<SParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    SetRibbonId(UINT32_MAX).Compile(context);
    SetRibbonIdFromSpawnOrder(17, UINT32_MAX).Compile(context);
    const auto identifier = Reference(program, 0, Operations::SetLiteral::Inputs::Value);
    EXPECT_EQ(identifier >> 29, uint32_t(EOperandSource::Constant));
    EXPECT_EQ(program.Constants.at(identifier & OperandIndexMask), UINT32_MAX);
    EXPECT_EQ(Reference(program, 1, Operations::SetRibbonIdFromSpawnOrder::Inputs::FirstRibbonId), identifier);
    EXPECT_EQ(Reference(program, 1, Operations::SetRibbonIdFromSpawnOrder::Inputs::RibbonCount), Encoded(EOperandSource::ImmediateUInt, 17));
}

TEST(ModuleCompileTest, RequiresUnsignedRibbonIdsForDestinationsAndAttributeReads)
{
    SCompiledModuleData program;
    const std::vector<SParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);

    context.Emit(EOperationType::SetLiteral, EParticleAttribute::RibbonId, {context.Scalar(1)});
    context.Emit(EOperationType::CopyFromAttribute, EParticleAttribute::Scale,
        {context.Attribute(EParticleAttribute::RibbonId, EOperandType::Scalar)});
    context.Emit(EOperationType::RandomRange, EParticleAttribute::RibbonId,
        {context.UInt(1), context.UInt(2)});
    EXPECT_TRUE(program.Operations.empty());
    EXPECT_TRUE(program.Operands.empty());

    context.Emit(EOperationType::SetLiteral, EParticleAttribute::RibbonId, {context.UInt(UINT32_MAX)});
    context.Emit(EOperationType::CopyFromAttribute, EParticleAttribute::RibbonId,
        {context.Attribute(EParticleAttribute::RibbonId, EOperandType::UInt)});
    ASSERT_EQ(program.Operations.size(), 2u);
    EXPECT_EQ(Reference(program, 1, Operations::CopyFromAttribute::Inputs::Value),
        Encoded(EOperandSource::ParticleAttribute, uint32_t(EParticleAttribute::RibbonId)));
}

TEST(ModuleCompileTest, RejectsOtherDestinationsForRibbonIdsFromSpawnOrder)
{
    SCompiledModuleData program;
    const std::vector<SParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    const auto count = context.UInt(2);
    const auto firstId = context.UInt(1);
    context.Emit(EOperationType::SetRibbonIdFromSpawnOrder, EParticleAttribute::RibbonId, {count, firstId});
    ASSERT_EQ(program.Operations.size(), 1u);
    const auto references = program.Operands;
    const auto constants = program.Constants;

    for (uint32_t value = 0; value <= uint32_t(EParticleAttribute::Temp3); ++value)
    {
        const auto target = EParticleAttribute(value);
        if (target == EParticleAttribute::RibbonId)
            continue;

        context.Emit(EOperationType::SetRibbonIdFromSpawnOrder, target, {count, firstId});
        ASSERT_EQ(program.Operations.size(), 1u) << "target " << value;
        EXPECT_EQ(program.Operations[0].Target, EParticleAttribute::RibbonId);
        EXPECT_EQ(program.Operands, references);
        EXPECT_EQ(program.Constants, constants);
    }
}

TEST(OperandValidationTest, ChecksTableBoundariesWithoutCompilingAnOperation)
{
    const std::array<uint32_t, 4> constants{};
    const std::vector<SParameter> parameters{
        { .Name = "Scalar", .Type = EParameterType::Scalar },
        { .Name = "Vector", .Type = EParameterType::Vector },
    };
    const SOperandValidationContext context{constants, parameters};
    const auto valid = [&](EOperandType type, EOperandSource source, uint32_t index)
    {
        return IsValidOperand({type, Encoded(source, index)}, context);
    };

    EXPECT_TRUE(valid(EOperandType::Scalar, EOperandSource::Constant, 3));
    EXPECT_FALSE(valid(EOperandType::Scalar, EOperandSource::Constant, 4));
    EXPECT_TRUE(valid(EOperandType::Vector3, EOperandSource::ConstantVector3, 1));
    EXPECT_FALSE(valid(EOperandType::Vector3, EOperandSource::ConstantVector3, 2));
    EXPECT_TRUE(valid(EOperandType::Vector4, EOperandSource::ConstantVector4, 0));
    EXPECT_FALSE(valid(EOperandType::Vector4, EOperandSource::ConstantVector4, 1));

    EXPECT_TRUE(valid(EOperandType::Scalar, EOperandSource::Parameter, 0));
    EXPECT_FALSE(valid(EOperandType::Vector3, EOperandSource::Parameter, 0));
    EXPECT_TRUE(valid(EOperandType::Vector3, EOperandSource::Parameter, 1));
    EXPECT_FALSE(valid(EOperandType::Scalar, EOperandSource::Parameter, 1));
    EXPECT_FALSE(valid(EOperandType::Scalar, EOperandSource::Parameter, 2));

    EXPECT_TRUE(valid(EOperandType::Curve, EOperandSource::Curve, OperandIndexMask));
    EXPECT_FALSE(valid(EOperandType::Curve, EOperandSource::Curve, 2));
}

TEST(ModuleCompileTest, RejectsMalformedReferencesBeforeEmittingInstructions)
{
    SCompiledModuleData program;
    const std::vector<SParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Scale,
        {{EOperandType::Scalar, Encoded(EOperandSource::Constant, 0)}});
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Scale,
        {context.Dynamic(static_cast<EDynamicInput>(99))});
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Scale,
        {context.Attribute(EParticleAttribute::Color, EOperandType::Scalar)});
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Temp0,
        {context.Curve("Missing")});
    EXPECT_TRUE(program.Operations.empty());
    EXPECT_TRUE(program.Operands.empty());
}

TEST(GPUOperationTest, MatchesTheShaderHeaderLayout)
{
    ASSERT_EQ(sizeof(EOperationType), sizeof(uint32_t));
    ASSERT_EQ(sizeof(SGPUOperation), 16u);
    EXPECT_EQ(offsetof(SGPUOperation, Type), 0u);
    EXPECT_EQ(offsetof(SGPUOperation, Target), 4u);
    EXPECT_EQ(offsetof(SGPUOperation, OperandOffset), 8u);
    EXPECT_EQ(offsetof(SGPUOperation, OperandCount), 12u);

    const SGPUOperation operation{
        EOperationType::RandomRange,
        EParticleAttribute::Size,
        7,
        2
    };
    std::array<uint32_t, 4> words{};
    std::memcpy(words.data(), &operation, sizeof(operation));
    const std::array<uint32_t, 4> expected{
        uint32_t(EOperationType::RandomRange),
        uint32_t(EParticleAttribute::Size),
        7,
        2
    };
    EXPECT_EQ(words, expected);
}

TEST(ModuleCompileTest, UsesTwentyFourBytesForOneScalarInstruction)
{
    SCompiledModuleData program;
    const std::vector<SParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Scale, {context.Scalar(2)});
    program.CompactConstants();
    EXPECT_EQ(program.Operations.size() * sizeof(SGPUOperation) +
        (program.Operands.size() + program.Constants.size()) * sizeof(uint32_t), 24u);
}

TEST(ModuleCompileTest, PreservesScalarBitsAndSignedZeroInConstants)
{
    SCompiledModuleData program;
    const std::vector<SParameter> parameters{{ .Name = "Bound", .Type = EParameterType::Scalar, .Value = glm::vec4(3) }};
    ModuleCompileContext context(program, parameters, "", 1);
    const uint32_t bits = 0xe0000001u;
    context.Emit(EOperationType::RandomRange, EParticleAttribute::Scale,
        {context.Scalar(std::bit_cast<float>(bits)), context.Scalar(0, "Bound")});
    context.Emit(EOperationType::RandomRange, EParticleAttribute::Scale,
        {context.Scalar(0.0f), context.Scalar(-0.0f)});
    program.CompactConstants();
    EXPECT_EQ(program.Operations[0].OperandCount, 2u);
    EXPECT_EQ(program.Operations[1].OperandCount, 2u);
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
    SCompiledModuleData program;
    const std::vector<SParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    context.Scalar(99);
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Scale, {context.Scalar(2)});
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Position, {context.Vector({1, 2, 3})});
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Velocity, {context.Vector({1, 2, 3})});
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
