#include <gtest/gtest.h>

#include <Engine/Aether/System.h>
#include <Engine/Material/MaterialInstance.h>
#include <Engine/Material/MaterialCompiler.h>
#include <Engine/Material/MaterialResolver.h>
#include <Engine/Material/MaterialRenderProxy.h>

#include "TestMaterialResolver.h"

using namespace Elixir;
using namespace Elixir::Aether;
using namespace Elixir::Aether::Core;

namespace
{
    SCompiledSystem Compile(const System& system)
    {
        TestMaterialResolver resolver;
        return system.Compile(resolver);
    }
}

TEST(AetherSystemTest, CompilePreservesEmitterSimulationSpace)
{
    System system{ "Simulation space contract" };
    system.AddEmitter("World", 8, 0.0f); // world emitter
    auto& localEmitter = system.AddEmitter("Local", 8, 0.0f);
    localEmitter.SetSimulationSpace(EParticleSimulationSpace::Local);

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.Emitters.size(), 2);
    EXPECT_EQ(compiled.Emitters[0].SimulationSpace, EParticleSimulationSpace::World);
    EXPECT_EQ(compiled.Emitters[1].SimulationSpace, EParticleSimulationSpace::Local);
}

TEST(AetherSystemTest, CompileAssignsContiguousLocalEmitterParticleOffsets)
{
    System system{ "Particle offset contract" };
    system.AddEmitter("First", 3u, 0.0f);
    system.AddEmitter("Second", 7u, 0.0f);
    system.AddEmitter("Third", 11u, 0.0f);

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.Emitters.size(), 3u);
    EXPECT_EQ(compiled.ParticleStateLayout, EParticleStateLayout::CoreV1);
    EXPECT_EQ(compiled.Emitters[0].LocalParticleOffset, 0u);
    EXPECT_EQ(compiled.Emitters[1].LocalParticleOffset, 3u);
    EXPECT_EQ(compiled.Emitters[2].LocalParticleOffset, 10u);
    EXPECT_EQ(compiled.TotalMaxParticles, 21u);
}

TEST(AetherSystemTest, CompileResolvesTriggerEmitterByCompiledIndex)
{
    System system{ "Trigger contract" };
    system.AddEmitter("Source", 8, 0.0f);

    auto& target = system.AddEmitter("Target", 8, 0.0f);
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

TEST(AetherSystemTest, CompileExposesOnlyAuthoredParameters)
{
    System system{ "Parameter contract" };
    system.GetParameters().SetFloat("SystemRate", 4.0f);
    system.GetCurves().SetCurve("SizeOverLife", { 0.0f, 1.0f });

    auto& emitter = system.AddEmitter("Smoke", 8, 0.0f);
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

TEST(AetherSystemTest, FindsNamedEmitterForMaterialPublication)
{
    System system{ "Named emitters" };
    auto& flame = system.AddEmitter("FlameCore", 8, 0.0f);
    system.AddEmitter("Smoke", 8, 0.0f);

    EXPECT_EQ(system.FindEmitter("FlameCore"), &flame);
    EXPECT_NE(system.FindEmitter("Smoke"), nullptr);
    EXPECT_EQ(system.FindEmitter("Missing"), nullptr);
}

TEST(AetherSystemTest, CompileSnapshotsParticleSpriteMaterialForRenderData)
{
    const auto material = CreateRef<Material>("Particle tint");
    ASSERT_TRUE(material->SetUsage(EMaterialUsage::ParticleSprite, true));
    ASSERT_TRUE(material->DefineParameter("Tint", {
        .Kind = EMaterialParameterKind::Value,
        .ValueType = EMaterialGraphValueType::Float4,
        .DefaultValue = SMaterialParam::MakeVector({ 1.0f, 1.0f, 1.0f, 1.0f }),
    }));

    const auto instance = material->CreateInstance();
    ASSERT_TRUE(instance->SetVector("Tint", { 0.25f, 0.5f, 0.75f, 1.0f }));

    System system{ "Material snapshot contract" };
    auto& emitter = system.AddEmitter("Smoke", 8, 0.0f);

    emitter.SetMaterial(instance);
    const auto first = Compile(system);

    ASSERT_EQ(first.Emitters.size(), 1);
    ASSERT_TRUE(first.Emitters[0].Material);
    EXPECT_TRUE(first.Emitters[0].Material->GetCompiledMaterial()->SupportsUsage(
        EMaterialUsage::ParticleSprite
    ));
    EXPECT_EQ(first.Emitters[0].Material->GetInstanceRevision(), instance->GetRevision());
    EXPECT_FLOAT_EQ(first.Emitters[0].Material->GetValues()[0].x, 0.25f);

    ASSERT_TRUE(instance->SetVector("Tint", { 0.75f, 0.5f, 0.25f, 1.0f }));

    emitter.SetMaterial(instance);
    const auto second = Compile(system);

    ASSERT_TRUE(second.Emitters[0].Material);
    EXPECT_FLOAT_EQ(first.Emitters[0].Material->GetValues()[0].x, 0.25f);
    EXPECT_FLOAT_EQ(second.Emitters[0].Material->GetValues()[0].x, 0.75f);
}

TEST(AetherSystemTest, CompileSnapshotsParticleRibbonMaterialForRenderData)
{
    const auto material = CreateRef<Material>("Particle ribbon");
    ASSERT_TRUE(material->SetUsage(EMaterialUsage::ParticleRibbon, true));

    System system{ "Ribbon material snapshot contract" };
    auto& emitter = system.AddEmitter("Ribbon", 8, 0.0f);
    emitter.SetRenderMode(EParticleRenderMode::Ribbon);

    const auto instance = material->CreateInstance();
    emitter.SetMaterial(instance);

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.Emitters.size(), 1);
    ASSERT_TRUE(compiled.Emitters[0].Material);
    EXPECT_TRUE(compiled.Emitters[0].Material->GetCompiledMaterial()->SupportsUsage(
        EMaterialUsage::ParticleRibbon
    ));
}

TEST(AetherSystemTest, CompileSnapshotsParticleMeshMaterialForRenderData)
{
    const auto material = CreateRef<Material>("Particle mesh");
    material->SetUsage(EMaterialUsage::ParticleMesh, true);

    System system{ "Mesh material" };
    auto& emitter = system.AddEmitter("Mesh", 8, 0.0f);
    emitter.SetRenderMode(EParticleRenderMode::Mesh);

    const auto instance = material->CreateInstance();
    emitter.SetMaterial(instance);

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.Emitters.size(), 1);
    ASSERT_TRUE(compiled.Emitters[0].Material);
    EXPECT_TRUE(compiled.Emitters[0].Material->GetCompiledMaterial()->SupportsUsage(
        EMaterialUsage::ParticleMesh
    ));
}

TEST(AetherSystemTest, KeepsAnEmitterWithoutAnExplicitMaterialUnbound)
{
    System system{ "Explicit material contract" };
    system.AddEmitter("Smoke", 8, 0.0f);

    const auto compiled = Compile(system);

    ASSERT_EQ(compiled.Emitters.size(), 1);
    EXPECT_FALSE(compiled.Emitters[0].Material);
}
