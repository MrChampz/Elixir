#include <gtest/gtest.h>

#include <Engine/Aether/Manager.h>
#include <Engine/Material/MaterialCompiler.h>
#include <Engine/Material/MaterialRegistry.h>
#include <Engine/Material/MaterialResolver.h>

#include "TestMaterialResolver.h"

using namespace Elixir;
using namespace Elixir::Aether;

TEST(AetherManagerTest, ResolvesEffectMaterialsBeforePublishingACompiledSystem)
{
    MaterialRegistry registry;
    TestMaterialResolver resolver;
    const Manager manager{ registry, resolver };
    System system{ "Managed effect" };

    auto& emitter = system.AddEmitter("Sprite", 8, 0.0f);
    emitter.SetMaterialDefinition({
        .BaseColor = { 0.25f, 0.5f, 0.75f },
        .Opacity = 0.4f,
        .Emissive = { 0.1f, 0.0f, 0.0f },
    });

    const auto compiled = manager.Compile(system);

    ASSERT_TRUE(compiled);
    ASSERT_EQ(compiled->Emitters.size(), 1);
    ASSERT_TRUE(compiled->Emitters[0].Material);
    EXPECT_TRUE(compiled->Emitters[0].Material->GetCompiledMaterial()->SupportsUsage(
        EMaterialUsage::ParticleSprite
    ));

    const auto instance = manager.CreateInstance(compiled);

    ASSERT_TRUE(instance);
    EXPECT_EQ(&instance->GetCompiledSystem(), compiled.get());
}

TEST(AetherManagerTest, PreservesAnExplicitMaterialBeforeCompiling)
{
    MaterialRegistry registry;
    TestMaterialResolver resolver;
    const Manager manager{ registry, resolver };
    System system{ "Explicit material" };

    auto& emitter = system.AddEmitter("Sprite", 8, 0.0f);
    emitter.SetMaterialDefinition({
        .Opacity = 0.4f,
    });

    const auto material = CreateRef<Material>("Explicit particle material");
    ASSERT_TRUE(material->SetUsage(EMaterialUsage::ParticleSprite, true));
    ASSERT_TRUE(material->DefineParameter("Tint", {
        .Kind = EMaterialParameterKind::Value,
        .ValueType = EMaterialGraphValueType::Float4,
        .DefaultValue = SMaterialParam::MakeVector({ 1.0f, 1.0f, 1.0f, 1.0f }),
    }));

    const auto explicitInstance = material->CreateInstance();
    ASSERT_TRUE(explicitInstance->SetVector("Tint", { 0.2f, 0.4f, 0.6f, 1.0f }));

    emitter.SetMaterial(explicitInstance);

    const auto compiled = manager.Compile(system);

    ASSERT_TRUE(compiled);
    ASSERT_TRUE(compiled->Emitters[0].Material);
    EXPECT_EQ(
        compiled->Emitters[0].Material->GetInstanceRevision(),
        explicitInstance->GetRevision()
    );
}