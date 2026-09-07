#include <gtest/gtest.h>

#include <Engine/Aether/System.h>
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

TEST(SystemTest, CompilePublishesParticleSpriteMaterialInstance)
{
    const auto material = CreateRef<Material>("Particle tint");
    ASSERT_TRUE(material->SetUsage(EMaterialUsage::ParticleSprite, true));
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
    EXPECT_TRUE(first.Emitters[0].Material->GetParent()->SupportsUsage(
        EMaterialUsage::ParticleSprite
    ));
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
    const auto material = CreateRef<Material>("Particle ribbon");
    ASSERT_TRUE(material->SetUsage(EMaterialUsage::ParticleRibbon, true));

    const auto system = CreateRef<System>("Ribbon material snapshot contract");
    auto& emitter = system->AddEmitter("Ribbon", 8, 0.0f);
    emitter.SetRenderMode(EParticleRenderMode::Ribbon);

    const auto instance = material->CreateInstance();
    emitter.SetMaterial(instance);

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.Emitters.size(), 1);
    ASSERT_TRUE(compiled.Emitters[0].Material);
    EXPECT_TRUE(compiled.Emitters[0].Material->GetParent()->SupportsUsage(
        EMaterialUsage::ParticleRibbon
    ));
}

TEST(SystemTest, CompileSnapshotsParticleMeshMaterialForRenderData)
{
    const auto material = CreateRef<Material>("Particle mesh");
    material->SetUsage(EMaterialUsage::ParticleMesh, true);

    const auto system = CreateRef<System>("Mesh material");
    auto& emitter = system->AddEmitter("Mesh", 8, 0.0f);
    emitter.SetRenderMode(EParticleRenderMode::Mesh);

    const auto instance = material->CreateInstance();
    emitter.SetMaterial(instance);

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.Emitters.size(), 1);
    ASSERT_TRUE(compiled.Emitters[0].Material);
    EXPECT_TRUE(compiled.Emitters[0].Material->GetParent()->SupportsUsage(
        EMaterialUsage::ParticleMesh
    ));
}

TEST(SystemTest, CompileAssignsTheDefaultMaterialWhenNoneIsExplicit)
{
    const auto system = CreateRef<System>("Default material contract");
    system->AddEmitter("Smoke", 8, 0.0f);

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.Emitters.size(), 1);
    ASSERT_TRUE(compiled.Emitters[0].Material);
    EXPECT_TRUE(compiled.Emitters[0].Material->GetParent()->SupportsUsage(
        EMaterialUsage::ParticleSprite
    ));
}
