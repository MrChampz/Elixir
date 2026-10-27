#include <gtest/gtest.h>
#include <bit>
#include <cstring>
#include <limits>

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

    uint32_t Encoded(EOperandSource source, EOperandFormat format, uint32_t index)
    {
        return (uint32_t(source) << OperandSourceShift) | (uint32_t(format) << OperandFormatShift) | index;
    }
}

TEST(ModuleCompileTest, ResolvesLocalParametersThenSystemParametersAndMissingValues)
{
    const auto parameters = CompileParameters(std::vector<SParameter>{
        { .Name = "Low", .Value = glm::vec4(1.0f) }, { .Name = "Smoke.Low", .Value = glm::vec4(2.0f) }, { .Name = "High", .Value = glm::vec4(3.0f) },
    });
    SCompiledModuleData program;
    ModuleCompileContext context(program, parameters, "Smoke", 2.5f);
    SetLifetime(1.0f, 4.0f).BindParameters("Low", "High").Compile(context);
    ApplyGravity({ 0.0f, -4.0f, 0.0f }).BindParameter("Missing").Compile(context);
    ApplyVortex({ 1.0f, 2.0f, 3.0f }, 4.0f, 5.0f)
        .BindParameters("Low", "High", "Missing", "Missing").Compile(context);

    ASSERT_EQ(program.Operations.size(), 3u);
    EXPECT_EQ(Reference(program, 0, Operations::RandomRange::Inputs::Minimum), Encoded(EOperandSource::Parameter, EOperandFormat::Float4, 4));
    EXPECT_EQ(Reference(program, 0, Operations::RandomRange::Inputs::Maximum), Encoded(EOperandSource::Parameter, EOperandFormat::Float4, 8));
    EXPECT_FLOAT_EQ(Literal(program, 1, Operations::AddWithDelta::Inputs::Value, 1), -10.0f);
    EXPECT_EQ(Reference(program, 2, Operations::ApplyVortex::Inputs::Center), Encoded(EOperandSource::Parameter, EOperandFormat::Float4, 4));
    EXPECT_EQ(Reference(program, 2, Operations::ApplyVortex::Inputs::Tangential), Encoded(EOperandSource::Parameter, EOperandFormat::Float4, 8));
    EXPECT_FLOAT_EQ(Literal(program, 2, Operations::ApplyVortex::Inputs::Radial), 5.0f);
}

TEST(ModuleCompileTest, CompilesScaleCurveWithRangeOffsetAndClamp)
{
    const auto parameters = CompileParameters(std::vector<SParameter>{ { .Name = "Curve:0", .Value = glm::vec4(0) }, { .Name = "Smoke.Curve:0", .Value = glm::vec4(1) } });
    SCompiledModuleData program;
    ModuleCompileContext context(program, parameters, "Smoke", 1.0f);
    ScaleOverLife(1.0f, 3.0f).BindCurve("Curve", EDynamicInput::EmitterTime).Compile(context);

    ASSERT_EQ(program.Operations.size(), 5u);
    EXPECT_EQ(Reference(program, 0, Operations::SampleCurve::Inputs::Curve), Encoded(EOperandSource::Parameter, EOperandFormat::Curve, 4));
    EXPECT_EQ(Reference(program, 0, Operations::SampleCurve::Inputs::Input), Encoded(EOperandSource::DynamicInput, EOperandFormat::Float, uint32_t(EDynamicInput::EmitterTime)));
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
    EXPECT_EQ(Reference(program, 4, Operations::CopyFromAttribute::Inputs::Value), Encoded(EOperandSource::ParticleAttribute, GetAttributeOperandFormat(EParticleAttribute(uint32_t(EParticleAttribute::Temp0))), uint32_t(EParticleAttribute::Temp0)));
}

TEST(ModuleCompileTest, CompilesConeBindingsAndLiteralFallbacks)
{
    const auto parameters = CompileParameters(std::vector<SParameter>{
        { .Name = "Angle", .Value = glm::vec4(0.5f) }, { .Name = "Smoke.Angle", .Value = glm::vec4(1) }, { .Name = "Low", .Value = glm::vec4(2) }, { .Name = "Smoke.High", .Value = glm::vec4(3) },
    });
    SCompiledModuleData program;
    ModuleCompileContext context(program, parameters, "Smoke", 1.0f);
    SetVelocityCone({ 0, 1, 0 }, 0.25f, 4.0f, 5.0f).BindParameters("Angle", "Low", "High").Compile(context);
    SetVelocityCone({ 0, 1, 0 }, 0.25f, 4.0f, 5.0f).BindParameters("Missing", "Low", "Missing").Compile(context);
    SetVelocityCone({ 0, 1, 0 }, 0.25f, 4.0f, 5.0f).Compile(context);

    ASSERT_EQ(program.Operations.size(), 3u);
    EXPECT_EQ(Reference(program, 0, Operations::SampleCone::Inputs::Angle), Encoded(EOperandSource::Parameter, EOperandFormat::Float4, 4));
    EXPECT_EQ(Reference(program, 0, Operations::SampleCone::Inputs::MinSpeed), Encoded(EOperandSource::Parameter, EOperandFormat::Float4, 8));
    EXPECT_EQ(Reference(program, 0, Operations::SampleCone::Inputs::MaxSpeed), Encoded(EOperandSource::Parameter, EOperandFormat::Float4, 12));
    EXPECT_FLOAT_EQ(Literal(program, 1, Operations::SampleCone::Inputs::Angle), 0.25f);
    EXPECT_EQ(Reference(program, 1, Operations::SampleCone::Inputs::MinSpeed), Encoded(EOperandSource::Parameter, EOperandFormat::Float4, 8));
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
    const std::vector<Elixir::Aether::Core::SCompiledParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    const auto scalar = context.Scalar(1);
    EXPECT_EQ(context.Scalar(1).Reference, scalar.Reference);
    context.Vector({1, 2, 3});
    context.Value({1, 2, 3, 4});
    EXPECT_EQ(program.Constants.size(), 8u);
    EXPECT_NE(context.Scalar(0).Reference, context.Scalar(-0.0f).Reference);
    const auto integer = context.UInt(UINT32_MAX);
    EXPECT_EQ(program.Constants[integer.Reference & OperandIndexMask], UINT32_MAX);
    EXPECT_EQ(context.UInt(17).Reference, Encoded(EOperandSource::Immediate, EOperandFormat::UInt, 17));
}

TEST(ModuleCompileTest, ValidatesContractsAndNamedParameterTypes)
{
    const auto parameters = CompileParameters(std::vector<SParameter>{
        { .Name = "Scalar", .Type = EParameterType::Float, .Value = glm::vec4(1) }, { .Name = "Vector", .Type = EParameterType::Float4, .Value = glm::vec4(1) },
    });
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
    EXPECT_EQ(context.Curve("Missing").Reference, Encoded(EOperandSource::Parameter, EOperandFormat::Curve, OperandIndexMask));
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
    const std::vector<Elixir::Aether::Core::SCompiledParameter> parameters;
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
    const std::vector<Elixir::Aether::Core::SCompiledParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    SetRibbonId(UINT32_MAX).Compile(context);
    SetRibbonIdFromSpawnOrder(17, UINT32_MAX).Compile(context);
    const auto identifier = Reference(program, 0, Operations::SetLiteral::Inputs::Value);
    EXPECT_EQ(identifier >> OperandSourceShift, uint32_t(EOperandSource::Constant));
    EXPECT_EQ(program.Constants.at(identifier & OperandIndexMask), UINT32_MAX);
    EXPECT_EQ(Reference(program, 1, Operations::SetRibbonIdFromSpawnOrder::Inputs::FirstRibbonId), identifier);
    EXPECT_EQ(Reference(program, 1, Operations::SetRibbonIdFromSpawnOrder::Inputs::RibbonCount), Encoded(EOperandSource::Immediate, EOperandFormat::UInt, 17));
}

TEST(ModuleCompileTest, RequiresUnsignedRibbonIdsForDestinationsAndAttributeReads)
{
    SCompiledModuleData program;
    const std::vector<Elixir::Aether::Core::SCompiledParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);

    context.Emit(EOperationType::SetLiteral, EParticleAttribute::RibbonId, {context.Scalar(1)});
    context.Emit(EOperationType::CopyFromAttribute, EParticleAttribute::Scale,
        {context.Attribute(EParticleAttribute::RibbonId, EOperandInput::Float)});
    context.Emit(EOperationType::RandomRange, EParticleAttribute::RibbonId,
        {context.UInt(1), context.UInt(2)});
    EXPECT_TRUE(program.Operations.empty());
    EXPECT_TRUE(program.Operands.empty());

    EXPECT_FALSE(context.IsValid());
    ModuleCompileContext validContext(program, parameters, "", 1);
    validContext.Emit(EOperationType::SetLiteral, EParticleAttribute::RibbonId,
        {validContext.UInt(UINT32_MAX)});
    validContext.Emit(EOperationType::CopyFromAttribute, EParticleAttribute::RibbonId,
        {validContext.Attribute(EParticleAttribute::RibbonId, EOperandInput::UInt)});
    ASSERT_EQ(program.Operations.size(), 2u);
    EXPECT_EQ(Reference(program, 1, Operations::CopyFromAttribute::Inputs::Value),
        Encoded(EOperandSource::ParticleAttribute, GetAttributeOperandFormat(EParticleAttribute(uint32_t(EParticleAttribute::RibbonId))), uint32_t(EParticleAttribute::RibbonId)));
}

TEST(ModuleCompileTest, RejectsOtherDestinationsForRibbonIdsFromSpawnOrder)
{
    SCompiledModuleData program;
    const std::vector<Elixir::Aether::Core::SCompiledParameter> parameters;
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
    const auto parameters = CompileParameters(std::vector<SParameter>{
        { .Name = "Scalar", .Type = EParameterType::Float },
        { .Name = "Vector", .Type = EParameterType::Float4 },
    });
    const SOperandValidationContext context{constants, parameters};
    const auto valid = [&](EOperandInput type, EOperandSource source, EOperandFormat format, uint32_t index)
    {
        return IsValidOperand({type, Encoded(source, format, index)}, context);
    };

    EXPECT_TRUE(valid(EOperandInput::Float, EOperandSource::Constant, EOperandFormat::Float, 3));
    EXPECT_FALSE(valid(EOperandInput::Float, EOperandSource::Constant, EOperandFormat::Float, 4));
    EXPECT_TRUE(valid(EOperandInput::Float3, EOperandSource::Constant, EOperandFormat::Float3, 1));
    EXPECT_FALSE(valid(EOperandInput::Float3, EOperandSource::Constant, EOperandFormat::Float3, 2));
    EXPECT_TRUE(valid(EOperandInput::Float4, EOperandSource::Constant, EOperandFormat::Float4, 0));
    EXPECT_FALSE(valid(EOperandInput::Float4, EOperandSource::Constant, EOperandFormat::Float4, 1));

    const auto parameter = [&](EOperandInput type, EOperandFormat format, uint32_t index)
    {
        return IsValidOperand({type, Encoded(EOperandSource::Parameter, format, index)}, context);
    };
    EXPECT_TRUE(parameter(EOperandInput::Float, EOperandFormat::Float, 0));
    EXPECT_FALSE(parameter(EOperandInput::Float3, EOperandFormat::Float, 0));
    EXPECT_TRUE(parameter(EOperandInput::Float3, EOperandFormat::Float4, 1));
    EXPECT_FALSE(parameter(EOperandInput::Float, EOperandFormat::Float4, 1));
    EXPECT_FALSE(parameter(EOperandInput::Float4, EOperandFormat::Float4, 2));

    EXPECT_TRUE(valid(EOperandInput::Curve, EOperandSource::Parameter, EOperandFormat::Curve, OperandIndexMask));
    EXPECT_FALSE(valid(EOperandInput::Curve, EOperandSource::Parameter, EOperandFormat::Curve, 2));
}

TEST(ModuleCompileTest, RejectsMalformedReferencesBeforeEmittingInstructions)
{
    SCompiledModuleData program;
    const std::vector<Elixir::Aether::Core::SCompiledParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Scale,
        {{EOperandInput::Float, Encoded(EOperandSource::Constant, EOperandFormat::Float, 0)}});
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Scale,
        {context.Dynamic(static_cast<EDynamicInput>(99))});
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Scale,
        {context.Attribute(EParticleAttribute::Color, EOperandInput::Float)});
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
    const std::vector<Elixir::Aether::Core::SCompiledParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Scale, {context.Scalar(2)});
    program.CompactConstants();
    EXPECT_EQ(program.Operations.size() * sizeof(SGPUOperation) +
        (program.Operands.size() + program.Constants.size()) * sizeof(uint32_t), 24u);
}

TEST(ModuleCompileTest, PreservesScalarBitsAndSignedZeroInConstants)
{
    SCompiledModuleData program;
    const auto parameters = CompileParameters(std::vector<SParameter>{{ .Name = "Bound", .Type = EParameterType::Float, .Value = glm::vec4(3) }});
    ModuleCompileContext context(program, parameters, "", 1);
    const uint32_t bits = 0xe0000001u;
    context.Emit(EOperationType::RandomRange, EParticleAttribute::Scale,
        {context.Scalar(std::bit_cast<float>(bits)), context.Scalar(0, "Bound")});
    context.Emit(EOperationType::RandomRange, EParticleAttribute::Scale,
        {context.Scalar(0.0f), context.Scalar(-0.0f)});
    program.CompactConstants();
    EXPECT_EQ(program.Operations[0].OperandCount, 2u);
    EXPECT_EQ(program.Operations[1].OperandCount, 2u);
    EXPECT_EQ(Reference(program, 0, 0) >> OperandSourceShift, uint32_t(EOperandSource::Constant));
    EXPECT_EQ(program.Constants.at(Reference(program, 0, 0) & OperandIndexMask), bits);
    EXPECT_EQ(Reference(program, 0, 1), (Encoded(EOperandSource::Parameter, EOperandFormat::Float, 0)));
    EXPECT_EQ(program.Constants.at(Reference(program, 1, 0) & OperandIndexMask), 0u);
    EXPECT_EQ(program.Constants.at(Reference(program, 1, 1) & OperandIndexMask), 0x80000000u);
    EXPECT_NE(Reference(program, 1, 0), Reference(program, 1, 1));
    EXPECT_EQ(program.Constants.size(), 3u);
}

TEST(ModuleCompileTest, CompactsRemainingConstantsAndPreservesSharingAndIntegerBits)
{
    SCompiledModuleData program;
    const std::vector<Elixir::Aether::Core::SCompiledParameter> parameters;
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
    EXPECT_EQ(parameters[0].Type, EParameterType::Float4);
    EXPECT_EQ(parameters[0].Value, glm::vec4(2));
    store.SetFloat("Value", 3);
    parameters = store.Compile();
    ASSERT_EQ(parameters.size(), 1u);
    EXPECT_EQ(parameters[0].Type, EParameterType::Float);
    EXPECT_EQ(parameters[0].Value, glm::vec4(3, 0, 0, 0));
}

TEST(CompiledParameterTest, PacksNativeWidthsAndPreservesUnsignedAndSignedZeroBits)
{
    const std::vector<SParameter> authored{
        { .Name = "Scalar", .Type = EParameterType::Float, .Value = {2, 0, 0, 0} },
        { .Name = "Vector2", .Type = EParameterType::Float2, .Value = {3, 4, 0, 0} },
        { .Name = "Vector3", .Type = EParameterType::Float3, .Value = {5, -0.0f, 7, 0} },
        { .Name = "Vector4", .Type = EParameterType::Float4, .Value = {8, 9, 10, 11} },
        { .Name = "UInt", .Type = EParameterType::UInt, .Value = EncodeUIntParameter(UINT32_MAX) },
        { .Name = "Curve:0", .Value = {12, 13, 14, 15} }
    };
    const auto compiled = CompileParameters(authored);
    const auto words = PackParameterValues(compiled);
    ASSERT_EQ(compiled.size(), 6u);
    const std::array<uint32_t, 6> offsets{0, 1, 3, 6, 10, 11};
    for (size_t i = 0; i < compiled.size(); ++i)
    {
        EXPECT_EQ(compiled[i].WordOffset, offsets[i]);
        EXPECT_EQ(compiled[i].Name, authored[i].Name);
        EXPECT_EQ(compiled[i].Type, authored[i].Type);
    }
    ASSERT_EQ(words.size() * sizeof(uint32_t), 60u);
    EXPECT_EQ(words[4], 0x80000000u);
    EXPECT_EQ(words[10], UINT32_MAX);
    EXPECT_FLOAT_EQ(std::bit_cast<float>(words.back()), 15);
}

TEST(CompiledParameterTest, RejectsMalformedOffsetsAndTypesBeforeCopyingPayloads)
{
    auto compiled = CompileParameters(std::array{SParameter{.Type = EParameterType::Float}});
    for (const auto offset : {1u, UINT32_MAX})
    {
        compiled[0].WordOffset = offset;
        EXPECT_TRUE(PackParameterValues(compiled).empty());
    }
    compiled[0].WordOffset = 0;
    compiled[0].Type = EParameterType(255);
    EXPECT_TRUE(PackParameterValues(compiled).empty());
    EXPECT_TRUE(CompileParameters(std::array{SParameter{.Type = EParameterType(255)}}).empty());
    EXPECT_TRUE(CompileParameters(std::span<const SParameter>{}).empty());
    EXPECT_TRUE(PackParameterValues(std::span<const Elixir::Aether::Core::SCompiledParameter>{}).empty());
}

TEST(ModuleCompileTest, BindsNativeParameterFormatsAndKeepsTheirOffsetsDuringCompaction)
{
    const auto parameters = CompileParameters(std::vector<SParameter>{
        {.Name = "Scalar", .Type = EParameterType::Float},
        {.Name = "Vector2", .Type = EParameterType::Float2},
        {.Name = "Vector3", .Type = EParameterType::Float3},
        {.Name = "Vector4", .Type = EParameterType::Float4},
        {.Name = "UInt", .Type = EParameterType::UInt},
        {.Name = "Curve:0"}, {.Name = "Curve:1"}
    });
    SCompiledModuleData program;
    ModuleCompileContext context(program, parameters, "", 1);
    context.Scalar(99);
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Size, {context.Scalar(0, "Scalar")});
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Position, {context.Vector({}, "Vector2")});
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Velocity, {context.Vector({}, "Vector3")});
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Color, {context.Value({}, "Vector4")});
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::RibbonId, {context.UInt(0, "UInt")});
    context.Emit(EOperationType::SampleCurve, EParticleAttribute::Scale, {context.Curve("Curve"), context.Scalar(0.5f)});
    ASSERT_EQ(program.Operations.size(), 6u);
    const std::array formats{EOperandFormat::Float, EOperandFormat::Float2, EOperandFormat::Float3,
        EOperandFormat::Float4, EOperandFormat::UInt};
    const std::array<uint32_t, 5> offsets{0, 1, 3, 6, 10};
    program.CompactConstants();
    for (size_t i = 0; i < formats.size(); ++i)
        EXPECT_EQ(Reference(program, i, 0), Encoded(EOperandSource::Parameter, formats[i], offsets[i]));
    EXPECT_EQ(Reference(program, 5, 0), Encoded(EOperandSource::Parameter, EOperandFormat::Curve, 11));
    ASSERT_EQ(program.Constants.size(), 1u);
    EXPECT_FLOAT_EQ(Literal(program, 5, 1), 0.5f);
    const auto operations = program.Operations.size();
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Scale, {context.Scalar(0, "UInt")});
    EXPECT_EQ(program.Operations.size(), operations);
}

TEST(OperandValidationTest, RejectsInteriorWordsAndIncorrectNativeParameterFormats)
{
    const auto parameters = CompileParameters(std::vector<SParameter>{
        {.Type = EParameterType::Float}, {.Type = EParameterType::Float3}, {.Type = EParameterType::UInt}
    });
    const SOperandValidationContext context{{}, parameters};
    const auto valid = [&](EOperandInput type, EOperandFormat format, uint32_t offset)
    {
        return IsValidOperand({type, Encoded(EOperandSource::Parameter, format, offset)}, context);
    };
    EXPECT_TRUE(valid(EOperandInput::Float3, EOperandFormat::Float3, 1));
    EXPECT_FALSE(valid(EOperandInput::Float3, EOperandFormat::Float3, 2));
    EXPECT_FALSE(valid(EOperandInput::Float3, EOperandFormat::Float4, 1));
    EXPECT_TRUE(valid(EOperandInput::UInt, EOperandFormat::UInt, 4));
    EXPECT_FALSE(valid(EOperandInput::Float, EOperandFormat::Float, 4));
    EXPECT_FALSE(IsValidOperand({EOperandInput::UInt, Encoded(EOperandSource::Immediate, EOperandFormat::Float, 4)}, context));

    auto invalidParameters = parameters;
    invalidParameters[1].Type = EParameterType::Float4;
    EXPECT_FALSE(IsValidOperand({EOperandInput::Float3, Encoded(EOperandSource::Parameter, EOperandFormat::Float3, 1)},
        {{}, invalidParameters}));
    invalidParameters = parameters;
    invalidParameters[1].WordOffset = 2;
    EXPECT_FALSE(IsValidOperand({EOperandInput::Float3, Encoded(EOperandSource::Parameter, EOperandFormat::Float3, 2)},
        {{}, invalidParameters}));
}

TEST(OperandReferenceTest, KeepsOriginFormatAndPayloadInIndependentFields)
{
    const auto reference = EncodeOperandReference(EOperandSource::Parameter, EOperandFormat::Float3, OperandIndexMask);
    EXPECT_EQ(reference, 0x2bffffffu);
    EXPECT_EQ(GetOperandSource(reference), EOperandSource::Parameter);
    EXPECT_EQ(GetOperandFormat(reference), EOperandFormat::Float3);
    EXPECT_EQ(reference & OperandIndexMask, OperandIndexMask);
    EXPECT_EQ(GetOperandSource(EncodeOperandReference(EOperandSource::Constant, EOperandFormat::Float3, 0)),
        EOperandSource::Constant);
    EXPECT_EQ(GetOperandFormat(EncodeOperandReference(EOperandSource::Parameter, EOperandFormat::Float, 0)),
        EOperandFormat::Float);
    EXPECT_EQ(EncodeOperandReference(EOperandSource::Constant, EOperandFormat::Float, OperandIndexMask + 1), UINT32_MAX);
    EXPECT_EQ(EncodeOperandReference(EOperandSource(8), EOperandFormat::Float, 0), UINT32_MAX);
    EXPECT_EQ(EncodeOperandReference(EOperandSource::Constant, EOperandFormat(8), 0), UINT32_MAX);
}

TEST(OperandValidationTest, RejectsFormatsThatDoNotMatchTheirOrigin)
{
    const std::array<uint32_t, 4> constants{};
    const auto parameters = CompileParameters(std::vector<SParameter>{{.Type = EParameterType::Unspecified}});
    const SOperandValidationContext context{constants, parameters};
    const auto valid = [&](EOperandInput type, EOperandSource source, EOperandFormat format, uint32_t index)
    {
        return IsValidOperand({type, Encoded(source, format, index)}, context);
    };
    EXPECT_TRUE(valid(EOperandInput::Curve, EOperandSource::Parameter, EOperandFormat::Curve, 0));
    EXPECT_FALSE(valid(EOperandInput::Curve, EOperandSource::Constant, EOperandFormat::Curve, 0));
    EXPECT_FALSE(valid(EOperandInput::Float, EOperandSource::Parameter, EOperandFormat::Curve, OperandIndexMask));
    EXPECT_FALSE(valid(EOperandInput::UInt, EOperandSource::Immediate, EOperandFormat::Float, 0));
    EXPECT_TRUE(valid(EOperandInput::UInt, EOperandSource::Immediate, EOperandFormat::UInt, OperandIndexMask));
    EXPECT_FALSE(valid(EOperandInput::Float, EOperandSource::DynamicInput, EOperandFormat::Float4, uint32_t(EDynamicInput::EmitterTime)));
    EXPECT_FALSE(valid(EOperandInput::Float, EOperandSource(7), EOperandFormat::Float, 0));
    EXPECT_FALSE(valid(EOperandInput::Float, EOperandSource::Constant, EOperandFormat(7), 0));
    EXPECT_TRUE(valid(EOperandInput::Float3, EOperandSource::Constant, EOperandFormat::Float2, 2));
    EXPECT_FALSE(valid(EOperandInput::Float3, EOperandSource::Constant, EOperandFormat::Float2, 3));
}

TEST(ModuleCompileTest, MovesUnsignedValuesBeyondTheImmediatePayloadIntoConstants)
{
    SCompiledModuleData program;
    const std::vector<Elixir::Aether::Core::SCompiledParameter> parameters;
    ModuleCompileContext context(program, parameters, "", 1);
    const auto immediate = context.UInt(OperandIndexMask);
    const auto constant = context.UInt(OperandIndexMask + 1);
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::RibbonId, {constant});
    program.CompactConstants();
    EXPECT_EQ(immediate.Reference, Encoded(EOperandSource::Immediate, EOperandFormat::UInt, OperandIndexMask));
    EXPECT_EQ(GetOperandSource(Reference(program, 0, 0)), EOperandSource::Constant);
    EXPECT_EQ(GetOperandFormat(Reference(program, 0, 0)), EOperandFormat::UInt);
    ASSERT_EQ(program.Constants.size(), 1u);
    EXPECT_EQ(program.Constants[0], 0x04000000u);
}

TEST(OperandValidationTest, UsesStoredFormatsForAttributesAndTemporaryRegisters)
{
    const SOperandValidationContext context{};
    for (const auto type : {EOperandInput::Float, EOperandInput::Float3, EOperandInput::Float4})
    {
        const auto reference = Encoded(EOperandSource::ParticleAttribute, EOperandFormat::Float4, uint32_t(EParticleAttribute::Temp0));
        EXPECT_TRUE(IsValidOperand({type, reference}, context));
        EXPECT_FALSE(IsValidOperand({type, Encoded(EOperandSource::ParticleAttribute, EOperandFormat::Float,
            uint32_t(EParticleAttribute::Temp0))}, context));
    }
    EXPECT_TRUE(IsValidOperand({EOperandInput::UInt, Encoded(EOperandSource::ParticleAttribute, EOperandFormat::UInt,
        uint32_t(EParticleAttribute::RibbonId))}, context));
    EXPECT_FALSE(IsValidOperand({EOperandInput::UInt, Encoded(EOperandSource::ParticleAttribute, EOperandFormat::Float4,
        uint32_t(EParticleAttribute::RibbonId))}, context));
}

TEST(ModuleCompileTest, OperationFailureRemainsInvalidAfterAValidOperation)
{
    SCompiledModuleData program;
    const std::vector<Elixir::Aether::Core::SCompiledParameter> parameters;
    ModuleCompileContext context(program, parameters, "Emitter", 1);
    ASSERT_TRUE(context.IsValid());
    const auto value = context.Scalar(1);
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Lifetime, {});
    EXPECT_FALSE(context.IsValid());
    context.Emit(EOperationType::SetLiteral, EParticleAttribute::Lifetime, {value});
    EXPECT_FALSE(context.IsValid());
    EXPECT_TRUE(program.Operations.empty());
    EXPECT_TRUE(program.Operands.empty());
}

TEST(ModuleCompileTest, IgnoringAnInvalidOperandStillFailsCompilation)
{
    SCompiledModuleData program;
    ParameterStore store;
    store.SetFloat("Gravity", 1);
    const auto parameters = CompileParameters(store.Compile());
    ModuleCompileContext context(program, parameters, "Emitter", 1);
    context.Vector({0, -9, 0}, "Gravity");
    EXPECT_FALSE(context.IsValid());
    EXPECT_TRUE(program.Operations.empty());
}

TEST(ModuleCompileTest, ConvertsScalarParametersForUnsignedBindings)
{
    for (const float value : {0.0f, 17.0f, 65537.0f, 16777216.0f, 4294967040.0f})
    {
        SCOPED_TRACE(value);
        const auto parameters = CompileParameters(std::vector<SParameter>{
            {.Name = "Seed", .Type = EParameterType::Float, .Value = {value, 0, 0, 0}}
        });
        SCompiledModuleData program;
        ModuleCompileContext context(program, parameters, "", 1);
        const auto operand = context.UInt(0, "Seed");
        EXPECT_TRUE(context.IsValid());
        EXPECT_TRUE(IsValidOperand(operand, {{}, parameters}));
        EXPECT_EQ(operand.Type, EOperandInput::UInt);
        EXPECT_EQ(GetOperandSource(operand.Reference), EOperandSource::Parameter);
        EXPECT_EQ(GetOperandFormat(operand.Reference), EOperandFormat::Float);
        EXPECT_EQ(operand.Reference & OperandIndexMask, parameters[0].WordOffset);
    }
}

TEST(ModuleCompileTest, RejectsInvalidScalarDefaultsForUnsignedBindings)
{
    for (const float value : {-1.0f, 1.5f, 4294967296.0f,
        std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        SCOPED_TRACE(value);
        const auto parameters = CompileParameters(std::vector<SParameter>{
            {.Name = "Seed", .Type = EParameterType::Float, .Value = {value, 0, 0, 0}}
        });
        SCompiledModuleData program;
        ModuleCompileContext context(program, parameters, "", 1);
        const auto operand = context.UInt(0, "Seed");
        EXPECT_FALSE(context.IsValid());
        EXPECT_FALSE(IsValidOperand(operand, {{}, parameters}));
        const SGPUOperand forged{
            EOperandInput::UInt,
            Encoded(EOperandSource::Parameter, EOperandFormat::Float, 0)
        };
        EXPECT_FALSE(IsValidOperand(forged, {{}, parameters}));
    }
}

TEST(ModuleCompileTest, RejectsVectorParametersForUnsignedBindings)
{
    for (const auto type : {EParameterType::Float2, EParameterType::Float3,
        EParameterType::Float4, EParameterType::Unspecified})
    {
        const auto parameters = CompileParameters(std::vector<SParameter>{
            {.Name = "Seed", .Type = type}
        });
        SCompiledModuleData program;
        ModuleCompileContext context(program, parameters, "", 1);
        const auto operand = context.UInt(0, "Seed");
        EXPECT_FALSE(context.IsValid());
        EXPECT_FALSE(IsValidOperand(operand, {{}, parameters}));
    }
}
