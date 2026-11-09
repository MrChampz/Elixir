#include <gtest/gtest.h>

#include <Engine/Aether/System.h>
#include <Engine/Aether/Rendering/StrandRenderConfig.h>
#include <Engine/Aether/Modules/ApplyGravity.h>
#include <Engine/Aether/Modules/DistanceConstraint.h>
#include <Engine/Aether/Modules/ApplyLinearDrag.h>
#include <Engine/Aether/Modules/CurlNoiseForce.h>
#include <Engine/Materials/Compilation/Compiler.h>
#include <Engine/Materials/MaterialInstance.h>

#include "TestInstanceRegistry.h"

using namespace Elixir;
using namespace Elixir::Aether;
using namespace Elixir::Aether::Core;
using namespace Elixir::Materials;

template <typename T>
concept HasPublicCompile = requires(const T& system)
{
    system.Compile();
};

static_assert(!HasPublicCompile<System>);

namespace
{
    SCompiledSystem Compile(const Ref<System>& system)
    {
        TestInstanceRegistry runtime;
        const auto instance = system->CreateInstance();
        EXPECT_TRUE(instance);

        if (!instance || !runtime.Registry.Register(instance))
            return {};

        Elixir::Aether::Rendering::FrameSubmission submission;
        EXPECT_TRUE(submission.Submit(*instance));

        if (submission.IsEmpty())
            return {};

        return submission.GetRenderProxies().front()->GetCompiledSystem();
    }
}

TEST(SystemTest, CompilePreservesEmitterSimulationSpace)
{
    const auto system = CreateRef<System>("Simulation space contract");
    system->AddEmitter("World", 8, 0.0f); // world emitter
    auto& localEmitter = system->AddEmitter("Local", 8, 0.0f);
    localEmitter.SetSimulationSpace(EParticleSimulationSpace::Local);

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.Emitters.size(), 2);
    EXPECT_EQ(compiled.Emitters[0].SimulationSpace, EParticleSimulationSpace::World);
    EXPECT_EQ(compiled.Emitters[1].SimulationSpace, EParticleSimulationSpace::Local);
    EXPECT_EQ(compiled.Emitters[0].ConstraintCount, 0u);
}

TEST(SystemTest, CompileAssignsContiguousLocalEmitterParticleOffsets)
{
    const auto system = CreateRef<System>("Particle offset contract");
    system->AddEmitter("First", 3u, 0.0f);
    system->AddEmitter("Second", 7u, 0.0f);
    system->AddEmitter("Third", 11u, 0.0f);

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.Emitters.size(), 3u);
    EXPECT_EQ(compiled.ParticleStateLayout, EParticleStateLayout::CoreV1);
    EXPECT_EQ(compiled.Emitters[0].LocalParticleOffset, 0u);
    EXPECT_EQ(compiled.Emitters[1].LocalParticleOffset, 3u);
    EXPECT_EQ(compiled.Emitters[2].LocalParticleOffset, 10u);
    EXPECT_EQ(compiled.TotalMaxParticles, 21u);
}

TEST(SystemTest, CompileResolvesTriggerEmitterByCompiledIndex)
{
    const auto system = CreateRef<System>("Trigger contract");
    system->AddEmitter("Source", 8, 0.0f);

    auto& target = system->AddEmitter("Target", 8, 0.0f);
    target.SetBurst(8, 1.0f);
    target.SetTriggerEmitter("Source", 0.25f);

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.Emitters.size(), 2u);

    EXPECT_TRUE(compiled.Emitters[1].IsTriggerDriven);
    EXPECT_EQ(compiled.Emitters[0].TriggerTargetOffset, 0);
    EXPECT_EQ(compiled.Emitters[0].TriggerTargetCount, 1);

    ASSERT_EQ(compiled.TriggerTargets.size(), 1);
    EXPECT_EQ(compiled.TriggerTargets[0].TargetEmitterIndex, 1);
    EXPECT_EQ(compiled.TriggerTargets[0].BurstCount, 8);
    EXPECT_FLOAT_EQ(compiled.TriggerTargets[0].DelaySeconds, 0.25f);
}

TEST(SystemTest, CompileExposesOnlyAuthoredParameters)
{
    const auto system = CreateRef<System>("Parameter contract");
    system->GetParameters().SetFloat("SystemRate", 4.0f);
    system->GetCurves().SetCurve("SizeOverLife", { 0.0f, 1.0f });

    auto& emitter = system->AddEmitter("Smoke", 8, 0.0f);
    emitter.GetParameters().SetFloat4("Tint", { 1.0f, 0.5f, 0.25f, 1.0f });

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.ExposedParameters.size(), 2);
    EXPECT_EQ(compiled.ExposedParameters[0].Name, "SystemRate");
    EXPECT_EQ(compiled.ExposedParameters[0].ParameterIndex, 0);
    EXPECT_EQ(compiled.ExposedParameters[1].Name, "Smoke.Tint");
    EXPECT_EQ(compiled.ExposedParameters[1].ParameterIndex, 1);

    ASSERT_EQ(compiled.Parameters.size(), 4);
    EXPECT_EQ(compiled.Parameters[2].Name, "SizeOverLife:0");
    EXPECT_EQ(compiled.Parameters[3].Name, "SizeOverLife:1");
}

TEST(SystemTest, FindsNamedEmitterForMaterialPublication)
{
    System system{ "Named emitters" };
    auto& flame = system.AddEmitter("FlameCore", 8, 0.0f);
    system.AddEmitter("Smoke", 8, 0.0f);

    EXPECT_EQ(system.FindEmitter("FlameCore"), &flame);
    EXPECT_NE(system.FindEmitter("Smoke"), nullptr);
    EXPECT_EQ(system.FindEmitter("Missing"), nullptr);
}

TEST(SystemTest, CompilePublishesParticleMaterialInstance)
{
    const auto material = CreateRef<Material>("Particle tint", EMaterialUsage::Particle);
    ASSERT_TRUE(material->DefineParameter("Tint", {
        .Kind = EMaterialParameterKind::Value,
        .ValueType = EMaterialValueType::Float4,
        .DefaultValue = SMaterialParameter::MakeVector({ 1.0f, 1.0f, 1.0f, 1.0f }),
    }));

    const auto instance = material->CreateInstance();
    ASSERT_TRUE(instance->SetVector("Tint", { 0.25f, 0.5f, 0.75f, 1.0f }));

    const auto system = CreateRef<System>("Material snapshot contract");
    auto& emitter = system->AddEmitter("Smoke", 8, 0.0f);

    emitter.SetMaterial(instance);
    const auto first = Compile(system);

    ASSERT_EQ(first.Emitters.size(), 1);
    ASSERT_TRUE(first.Emitters[0].Material);
    EXPECT_EQ(first.Emitters[0].Material->GetParent()->GetUsage(), EMaterialUsage::Particle);
    EXPECT_EQ(first.Emitters[0].Material, instance);
    EXPECT_FLOAT_EQ(first.Emitters[0].Material->GetVector("Tint").x, 0.25f);

    ASSERT_TRUE(instance->SetVector("Tint", { 0.75f, 0.5f, 0.25f, 1.0f }));

    emitter.SetMaterial(instance);
    const auto second = Compile(system);

    ASSERT_TRUE(second.Emitters[0].Material);
    EXPECT_EQ(first.Emitters[0].Material, instance);
    EXPECT_FLOAT_EQ(second.Emitters[0].Material->GetVector("Tint").x, 0.75f);
}

TEST(SystemTest, CompileSnapshotsParticleRibbonMaterialForRenderData)
{
    const auto material = CreateRef<Material>("Particle ribbon", EMaterialUsage::Particle);

    const auto system = CreateRef<System>("Ribbon material snapshot contract");
    auto& emitter = system->AddEmitter("Ribbon", 8, 0.0f);
    emitter.SetRenderMode(EParticleRenderMode::Ribbon);

    const auto instance = material->CreateInstance();
    emitter.SetMaterial(instance);

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.Emitters.size(), 1);
    ASSERT_TRUE(compiled.Emitters[0].Material);
    EXPECT_EQ(
        compiled.Emitters[0].Material->GetParent()->GetUsage(),
        EMaterialUsage::Particle
    );
}

TEST(SystemTest, CompilePreservesParticleMaterialWhenRenderModeChanges)
{
    const auto material = CreateRef<Material>("Particle material", EMaterialUsage::Particle);

    const auto system = CreateRef<System>("Particle render mode contract");
    auto& emitter = system->AddEmitter("Emitter", 8, 0.0f);
    const auto instance = material->CreateInstance();
    emitter.SetMaterial(instance);
    emitter.SetRenderMode(EParticleRenderMode::Ribbon);

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.Emitters.size(), 1);
    EXPECT_EQ(compiled.Emitters[0].RenderMode, EParticleRenderMode::Ribbon);
    EXPECT_EQ(compiled.Emitters[0].Material, instance);
    EXPECT_EQ(
        compiled.Emitters[0].Material->GetParent()->GetUsage(),
        EMaterialUsage::Particle
    );
}

TEST(SystemTest, CompileSnapshotsParticleMeshMaterialForRenderData)
{
    const auto material = CreateRef<Material>("Particle mesh", EMaterialUsage::Particle);

    const auto system = CreateRef<System>("Mesh material");
    auto& emitter = system->AddEmitter("Mesh", 8, 0.0f);
    emitter.SetRenderMode(EParticleRenderMode::Mesh);

    const auto instance = material->CreateInstance();
    emitter.SetMaterial(instance);

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.Emitters.size(), 1);
    ASSERT_TRUE(compiled.Emitters[0].Material);
    EXPECT_EQ(
        compiled.Emitters[0].Material->GetParent()->GetUsage(),
        EMaterialUsage::Particle
    );
}

TEST(SystemTest, CompileAssignsTheDefaultMaterialWhenNoneIsExplicit)
{
    const auto system = CreateRef<System>("Default material contract");
    system->AddEmitter("Smoke", 8, 0.0f);

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.Emitters.size(), 1);
    ASSERT_TRUE(compiled.Emitters[0].Material);
    EXPECT_EQ(
        compiled.Emitters[0].Material->GetParent()->GetUsage(),
        EMaterialUsage::Particle
    );
}

TEST(SystemTest, CompilesConstraintModulesWithoutChangingParticleOrUpdateRanges)
{
    const auto system = CreateRef<System>("Constraint modules");
    auto& first = system->AddEmitter("First", 96, 0);
    first.SetRenderMode(EParticleRenderMode::Strand);
    Simulation::SDistanceConstraint config;
    config.MaxPoints = 32;
    config.OrderStep = 3;
    first.AddModule(CreateScope<Modules::DistanceConstraint>(config));
    first.AddModule(CreateScope<Modules::ApplyLinearDrag>(2.4f));
    Simulation::SSolverConfig solver;
    solver.Iterations = 12;
    ASSERT_TRUE(first.SetSolverConfig(solver));
    Aether::Rendering::SStrandRenderConfig render;
    render.RadialSegments = 6;
    render.OrderStep = 3;
    render.MaxPoints = 32;
    const auto renderConfig =
        CreateRef<const Aether::Rendering::SStrandRenderConfig>(render);
    ASSERT_TRUE(first.SetRenderConfig(renderConfig));
    auto& sprites = system->AddEmitter("Sprites", 7, 0);
    sprites.AddModule(CreateScope<Modules::DistanceConstraint>(config));
    auto& second = system->AddEmitter("Second", 48, 0);
    second.SetRenderMode(EParticleRenderMode::Strand);
    const auto compiled = Compile(system);
    ASSERT_EQ(compiled.Emitters.size(), 3u);
    EXPECT_EQ(compiled.TotalMaxParticles, 151u);
    EXPECT_EQ(compiled.Emitters[1].LocalParticleOffset, 96u);
    EXPECT_EQ(compiled.Emitters[2].LocalParticleOffset, 103u);
    EXPECT_EQ(compiled.Emitters[0].ConstraintOffset, 0u);
    EXPECT_EQ(compiled.Emitters[0].ConstraintCount, 1u);
    EXPECT_EQ(compiled.Emitters[1].ConstraintOffset, 1u);
    EXPECT_EQ(compiled.Emitters[1].ConstraintCount, 1u);
    EXPECT_EQ(compiled.Emitters[2].ConstraintCount, 0u);
    ASSERT_EQ(compiled.ModuleData.Constraints.size(), 2u);
    EXPECT_EQ(compiled.ModuleData.Constraints[0].MaxPoints, 32u);
    EXPECT_EQ(compiled.Emitters[0].Solver.Iterations, 12u);
    EXPECT_EQ(compiled.Emitters[0].UpdateOperationCount, 1u);
    EXPECT_EQ(compiled.ModuleData.Operations[0].Type, EOperationType::Dampen);
    const auto* snapshot = dynamic_cast<const Aether::Rendering::SStrandRenderConfig*>(
        compiled.Emitters[0].RenderConfig.get()
    );
    ASSERT_NE(snapshot, nullptr);
    render.RadialSegments = 12;
    const auto changedRenderConfig =
        CreateRef<const Aether::Rendering::SStrandRenderConfig>(render);
    ASSERT_TRUE(first.SetRenderConfig(changedRenderConfig));
    config.Stiffness = 0.5f;
    first.AddModule(CreateScope<Modules::DistanceConstraint>(config));
    solver.Iterations = 4;
    ASSERT_TRUE(first.SetSolverConfig(solver));
    const auto changed = Compile(system);
    EXPECT_EQ(changed.Emitters[0].ConstraintCount, 2u);
    EXPECT_EQ(changed.Emitters[1].ConstraintOffset, 2u);
    EXPECT_EQ(changed.TotalMaxParticles, compiled.TotalMaxParticles);
    EXPECT_EQ(compiled.Emitters[0].ConstraintCount, 1u);
    EXPECT_EQ(compiled.Emitters[0].Solver.Iterations, 12u);
    EXPECT_EQ(snapshot->RadialSegments, 6u);
    solver.Iterations = 0;
    EXPECT_FALSE(first.SetSolverConfig(solver));
    EXPECT_EQ(first.GetSolverConfig().Iterations, 4u);
    solver.Iterations = 17;
    EXPECT_FALSE(first.SetSolverConfig(solver));
}

TEST(SystemTest, RejectsInvalidConstraintModulesDuringCompilation)
{
    const auto system = CreateRef<System>("Invalid constraint");
    auto& emitter = system->AddEmitter("Particles", 8, 0);
    Simulation::SDistanceConstraint config;
    config.Stiffness = std::numeric_limits<float>::quiet_NaN();
    emitter.AddModule(CreateScope<Modules::DistanceConstraint>(config));
    TestInstanceRegistry runtime;
    const auto instance = system->CreateInstance();
    ASSERT_TRUE(instance);
    EXPECT_FALSE(runtime.Registry.Register(instance));
}

TEST(SystemTest, RenderConfigIsValidatedSharedAndCanBeCleared)
{
    const auto system = CreateRef<System>("Generic render configuration");
    auto& emitter = system->AddEmitter("Particles", 8, 0);
    EXPECT_FALSE(emitter.GetRenderConfig());
    const auto config = CreateRef<const Aether::Rendering::SStrandRenderConfig>();
    ASSERT_TRUE(emitter.SetRenderConfig(config));
    EXPECT_EQ(emitter.GetRenderConfig(), config);
    const auto compiled = Compile(system);
    ASSERT_EQ(compiled.Emitters.size(), 1u);
    EXPECT_EQ(compiled.Emitters[0].RenderConfig, config);
    Aether::Rendering::SStrandRenderConfig invalid;
    invalid.WidthScale = 0;
    const auto invalidConfig =
        CreateRef<const Aether::Rendering::SStrandRenderConfig>(invalid);
    EXPECT_FALSE(emitter.SetRenderConfig(invalidConfig));
    EXPECT_EQ(emitter.GetRenderConfig(), config);
    ASSERT_TRUE(emitter.SetRenderConfig(nullptr));
    EXPECT_FALSE(emitter.GetRenderConfig());
    EXPECT_FALSE(Compile(system).Emitters[0].RenderConfig);
    EXPECT_EQ(compiled.Emitters[0].RenderConfig, config);
}

TEST(SystemTest, LifetimeDeathPolicyDefaultsToEnabledAndIsSnapshottedIndependently)
{
    const auto system = CreateRef<System>("Particle death policy");
    auto& emitter = system->AddEmitter("Persistent", 1, 0);
    EXPECT_TRUE(emitter.ShouldKillOnLifetimeExpired());
    emitter.SetKillOnLifetimeExpired(false);
    const auto compiled = Compile(system);
    ASSERT_EQ(compiled.Emitters.size(), 1);
    EXPECT_FALSE(compiled.Emitters[0].KillOnLifetimeExpired);
    emitter.SetKillOnLifetimeExpired(true);
    EXPECT_FALSE(compiled.Emitters[0].KillOnLifetimeExpired);
    EXPECT_TRUE(Compile(system).Emitters[0].KillOnLifetimeExpired);
}

TEST(SystemTest, CompilesEveryUpdateModuleIntoOneParticleProgram)
{
    const auto system = CreateRef<System>("Targets");
    auto& emitter = system->AddEmitter("Strand", 1, 0);
    emitter.SetRenderMode(EParticleRenderMode::Strand);
    emitter.AddModule(CreateScope<Modules::ApplyGravity>(glm::vec3(0, -1, 0)));
    auto curl = CreateScope<Modules::CurlNoiseForce>();
    emitter.AddModule(std::move(curl));
    auto gravity = CreateScope<Modules::ApplyGravity>(glm::vec3(0, -2, 0));
    emitter.AddModule(std::move(gravity));
    emitter.AddModule(CreateScope<Modules::CurlNoiseForce>());
    const auto compiled = Compile(system);
    ASSERT_EQ(compiled.Emitters.size(), 1);
    const auto& result = compiled.Emitters[0];
    EXPECT_EQ(result.UpdateOperationCount, 24u);
    for (const uint32_t offset : {0u, 12u})
    {
        EXPECT_EQ(
            compiled.ModuleData.Operations[result.UpdateOperationOffset + offset].Type,
            EOperationType::AddWithDelta
        );
        EXPECT_EQ(
            compiled.ModuleData.Operations[result.UpdateOperationOffset + offset + 8].Type,
            EOperationType::SampleCurlNoise
        );
    }
}
