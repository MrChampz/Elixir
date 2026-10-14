#include <gtest/gtest.h>

#include <Engine/Aether/System.h>
#include <Engine/Aether/Modules/ApplyGravity.h>
#include <Engine/Aether/Modules/StrandGrowth.h>
#include <Engine/Aether/Modules/ApplyCurlNoise.h>
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

TEST(SystemTest, CompilesIndependentStrandPointRangesAndSnapshotsSettings)
{
    const auto system = CreateRef<System>("Strand ranges");
    auto& first = system->AddEmitter("First", 3, 0);
    first.SetRenderMode(EParticleRenderMode::Strand);
    SStrandSettings settings;
    settings.PointCount = 32;
    ASSERT_TRUE(first.SetStrandSettings(settings));
    system->AddEmitter("Sprites", 7, 0);
    auto& second = system->AddEmitter("Second", 2, 0);
    second.SetRenderMode(EParticleRenderMode::Strand);
    const auto compiled = Compile(system);
    ASSERT_EQ(compiled.Emitters.size(), 3);
    EXPECT_EQ(compiled.TotalStrandPoints, 3 * 32 + 2 * 24);
    EXPECT_EQ(compiled.Emitters[0].LocalStrandPointOffset, 0);
    EXPECT_EQ(compiled.Emitters[2].LocalStrandPointOffset, 96);
    EXPECT_EQ(compiled.Emitters[2].LocalParticleOffset, 10);
    settings.PointCount = 12;
    ASSERT_TRUE(first.SetStrandSettings(settings));
    EXPECT_EQ(compiled.Emitters[0].Strand.PointCount, 32);
    settings.PointCount = 97;
    EXPECT_FALSE(first.SetStrandSettings(settings));
    EXPECT_EQ(first.GetStrandSettings().PointCount, 12);
    settings.PointCount = 12;
    settings.Damping = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(first.SetStrandSettings(settings));
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

TEST(SystemTest, PartitionsUpdateModulesByTargetAndPreservesEachTargetOrder)
{
    const auto system = CreateRef<System>("Targets");
    auto& emitter = system->AddEmitter("Strand", 1, 0);
    emitter.SetRenderMode(EParticleRenderMode::Strand);
    emitter.AddModule(CreateScope<Modules::ApplyGravity>(glm::vec3(0, -1, 0)));
    auto curl = CreateScope<Modules::ApplyCurlNoise>();
    ASSERT_TRUE(curl->SetTarget(EModuleTarget::StrandPoints));
    emitter.AddModule(std::move(curl));
    auto gravity = CreateScope<Modules::ApplyGravity>(glm::vec3(0, -2, 0));
    ASSERT_TRUE(gravity->SetTarget(EModuleTarget::StrandPoints));
    emitter.AddModule(std::move(gravity));
    emitter.AddModule(CreateScope<Modules::ApplyCurlNoise>());
    const auto compiled = Compile(system);
    ASSERT_EQ(compiled.Emitters.size(), 1);
    const auto& result = compiled.Emitters[0];
    EXPECT_EQ(result.UpdateOperationCount, 2);
    EXPECT_EQ(result.StrandUpdateOperationCount, 2);
    EXPECT_EQ(compiled.ModuleData.Operations[result.UpdateOperationOffset].Type, EOperationType::AddWithDelta);
    EXPECT_EQ(compiled.ModuleData.Operations[result.UpdateOperationOffset + 1].Type, EOperationType::ApplyCurlNoise);
    EXPECT_EQ(compiled.ModuleData.Operations[result.StrandUpdateOperationOffset].Type, EOperationType::ApplyCurlNoise);
    EXPECT_EQ(compiled.ModuleData.Operations[result.StrandUpdateOperationOffset + 1].Type, EOperationType::AddWithDelta);
}

TEST(SystemTest, BranchesReserveIndependentPersistentPointRanges)
{
    auto system = CreateRef<System>("Growth");
    auto& emitter = system->AddEmitter("Vines", 8, 0);
    emitter.SetRenderMode(EParticleRenderMode::Strand);
    SStrandSettings settings;
    settings.PointCount = 40;
    settings.BranchCount = 3;
    ASSERT_TRUE(emitter.SetStrandSettings(settings));
    const auto compiled = Compile(system);
    EXPECT_EQ(compiled.TotalStrandPoints, 8u * 40u * 4u);
    EXPECT_EQ(compiled.Emitters.front().Strand.BranchCount, 3u);
    settings.GrowthSpeed = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(settings.IsValid());
}

TEST(SystemTest, StrandGrowthCompilesDevelopmentWithoutPointForceInstructions)
{
    auto system = CreateRef<System>("Plant");
    auto& emitter = system->AddEmitter("Stem",5,0);
    emitter.SetRenderMode(EParticleRenderMode::Strand);
    SStrandGrowthSettings growth;
    growth.Speed=.18f; growth.BranchCount=2;
    growth.BranchVariation=.5f; growth.Seed=123;
    emitter.AddModule(CreateScope<Modules::StrandGrowth>(growth));
    const auto compiled = Compile(system);
    ASSERT_EQ(compiled.Emitters.size(),1u);
    EXPECT_TRUE(compiled.Emitters[0].Strand.HasGrowthModule);
    EXPECT_FLOAT_EQ(compiled.Emitters[0].Strand.GrowthSpeed,.18f);
    EXPECT_EQ(compiled.TotalStrandPoints,5u * 24u * 3u);
    EXPECT_EQ(compiled.Emitters[0].StrandUpdateOperationCount,0u);
    TestInstanceRegistry registry;
    emitter.AddModule(CreateScope<Modules::StrandGrowth>(growth));
    EXPECT_FALSE(registry.Registry.Register(system->CreateInstance()));
}

TEST(SystemTest, FixedEndsRejectGrowthAfterModuleCompilation)
{
    auto system = CreateRef<System>("Cable");
    auto& emitter = system->AddEmitter("Wire",1,0);
    emitter.SetRenderMode(EParticleRenderMode::Strand);
    SStrandSettings settings;
    settings.EndAttachment.Enabled=true;
    settings.EndAttachment.Position={1,0,0};
    ASSERT_TRUE(emitter.SetStrandSettings(settings));
    auto compiled=Compile(system);
    ASSERT_EQ(compiled.Emitters.size(),1u);
    EXPECT_TRUE(compiled.Emitters[0].Strand.EndAttachment.Enabled);
    SStrandGrowthSettings growth;
    growth.Speed=.1f;
    emitter.AddModule(CreateScope<Modules::StrandGrowth>(growth));
    TestInstanceRegistry registry;
    EXPECT_FALSE(registry.Registry.Register(system->CreateInstance()));
    settings.EndAttachment.Position.x=std::numeric_limits<float>::infinity();
    EXPECT_FALSE(settings.IsValid());
}
