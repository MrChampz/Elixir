#include <gtest/gtest.h>

#include <Engine/Aether/System.h>
#include <Engine/Aether/Modules/ApplyGravity.h>
#include <Engine/Aether/Modules/ApplyVortex.h>
#include <Engine/Aether/Modules/ScaleOverLife.h>
#include <Engine/Aether/Modules/SetLifetime.h>
#include <Engine/Aether/Modules/SetVelocityCone.h>

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
            context.Emit({ .Data0 = glm::vec4(m_Marker) });
            context.Emit({ .Data0 = glm::vec4(m_Marker + 1.0f) });
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
    ASSERT_EQ(compiled.Ops.size(), 8u);
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
        EXPECT_FLOAT_EQ(compiled.Ops[i].Data0.x, expected[i]);
}

TEST(ModuleCompileTest, ResolvesLocalParametersThenSystemParametersAndMissingValues)
{
    const std::vector<SGPUParameter> parameters{
        { "Low", glm::vec4(1.0f) },
        { "Smoke.Low", glm::vec4(2.0f) },
        { "High", glm::vec4(3.0f) },
    };
    std::vector<SGPUParticleOp> operations;
    ModuleCompileContext context(operations, parameters, "Smoke", 2.5f);
    SetLifetime(1.0f, 4.0f).BindParameters("Low", "High").Compile(context);
    ApplyGravity({ 0.0f, -4.0f, 0.0f }).BindParameter("Missing").Compile(context);
    ApplyVortex({ 1.0f, 2.0f, 3.0f }, 4.0f, 5.0f)
        .BindParameters("Low", "High", "Missing", "Missing").Compile(context);

    ASSERT_EQ(operations.size(), 3u);
    EXPECT_EQ(operations[0].Type, EParticleOp::RandomRange);
    EXPECT_EQ(operations[0].Target, EParticleAttribute::Lifetime);
    EXPECT_EQ(operations[0].Parameter0Index, 1u);
    EXPECT_EQ(operations[0].Parameter1Index, 2u);
    EXPECT_FLOAT_EQ(operations[0].Data0.x, 1.0f);
    EXPECT_FLOAT_EQ(operations[0].Data1.x, 4.0f);
    EXPECT_EQ(operations[1].Parameter0Index, UINT32_MAX);
    EXPECT_FLOAT_EQ(operations[1].Data0.y, -10.0f);
    EXPECT_EQ(operations[2].Parameter0Index, 1u);
    EXPECT_EQ(operations[2].Parameter1Index, UINT32_MAX);
    EXPECT_FLOAT_EQ(operations[2].Data2.z, 2.0f);
    EXPECT_FLOAT_EQ(operations[2].Data2.w, -1.0f);
}

TEST(ModuleCompileTest, CompilesScaleCurveWithRangeOffsetAndClamp)
{
    const std::vector<SGPUParameter> parameters{
        { "Curve:0", glm::vec4(0.0f) },
        { "Smoke.Curve:0", glm::vec4(1.0f) },
    };
    std::vector<SGPUParticleOp> operations;
    ModuleCompileContext context(operations, parameters, "Smoke", 1.0f);
    ScaleOverLife(1.0f, 3.0f).BindCurve("Curve", EDynamicInput::EmitterTime).Compile(context);

    ASSERT_EQ(operations.size(), 5u);
    EXPECT_EQ(operations[0].Type, EParticleOp::SampleCurve);
    EXPECT_EQ(operations[0].Parameter0Index, 1u);
    EXPECT_FLOAT_EQ(operations[0].Data0.x, static_cast<float>(EDynamicInput::EmitterTime));
    EXPECT_EQ(operations[1].Type, EParticleOp::Mul);
    EXPECT_FLOAT_EQ(operations[1].Data0.x, 2.0f);
    EXPECT_EQ(operations[2].Type, EParticleOp::Add);
    EXPECT_FLOAT_EQ(operations[2].Data0.x, 1.0f);
    EXPECT_EQ(operations[3].Type, EParticleOp::Clamp);
    EXPECT_EQ(operations[3].Data0, glm::vec4(0.0f));
    EXPECT_EQ(operations[3].Data1, glm::vec4(4.0f));
    EXPECT_EQ(operations[4].Type, EParticleOp::CopyFromAttribute);
    EXPECT_EQ(operations[4].Target, EParticleAttribute::Scale);
}

TEST(ModuleCompileTest, CompilesConeBindingsAndLiteralFallbacks)
{
    const std::vector<SGPUParameter> parameters{
        { "Angle", glm::vec4(0.5f) },
        { "Smoke.Angle", glm::vec4(1.0f) },
        { "Low", glm::vec4(2.0f) },
        { "Smoke.High", glm::vec4(3.0f) },
    };
    std::vector<SGPUParticleOp> operations;
    ModuleCompileContext context(operations, parameters, "Smoke", 1.0f);
    SetVelocityCone({ 0, 1, 0 }, 0.25f, 4.0f, 5.0f)
        .BindParameters("Angle", "Low", "High").Compile(context);
    SetVelocityCone({ 0, 1, 0 }, 0.25f, 4.0f, 5.0f)
        .BindParameters("Missing", "Low", "Missing").Compile(context);
    SetVelocityCone({ 0, 1, 0 }, 0.25f, 4.0f, 5.0f).Compile(context);

    ASSERT_EQ(operations.size(), 3u);
    EXPECT_EQ(operations[0].Parameter0Index, 2u);
    EXPECT_EQ(operations[0].Parameter1Index, 3u);
    EXPECT_FLOAT_EQ(operations[0].Data1.z, 1.0f);
    EXPECT_EQ(operations[1].Parameter0Index, 2u);
    EXPECT_EQ(operations[1].Parameter1Index, UINT32_MAX);
    EXPECT_FLOAT_EQ(operations[1].Data1.z, -1.0f);
    EXPECT_EQ(operations[2].Parameter0Index, UINT32_MAX);
    EXPECT_EQ(operations[2].Parameter1Index, UINT32_MAX);
    EXPECT_FLOAT_EQ(operations[2].Data1.z, -1.0f);
    for (const auto& operation : operations)
    {
        EXPECT_EQ(operation.Type, EParticleOp::SampleCone);
        EXPECT_EQ(operation.Target, EParticleAttribute::Velocity);
        EXPECT_EQ(operation.Data0, glm::vec4(0, 1, 0, 0.25f));
        EXPECT_FLOAT_EQ(operation.Data1.x, 4.0f);
        EXPECT_FLOAT_EQ(operation.Data1.y, 5.0f);
    }
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
    ASSERT_EQ(compiled.Ops.size(), 1u);
    EXPECT_EQ(compiled.Emitters[0].SpawnOpCount, 1u);
    EXPECT_EQ(compiled.Ops[0].Type, EParticleOp::RandomRange);
}
