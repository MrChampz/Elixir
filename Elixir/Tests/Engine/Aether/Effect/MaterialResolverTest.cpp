#include <gtest/gtest.h>

#include <Engine/Aether/System.h>
#include <Engine/Aether/Effect/MaterialResolver.h>
#include <Engine/Materials/MaterialRegistry.h>

using namespace Elixir;
using namespace Elixir::Aether;
using namespace Elixir::Aether::Core;
using namespace Elixir::Materials;

TEST(MaterialResolverTest, CreatesAuthoredMaterialsAndUsesUsageDefaults)
{
    MaterialRegistry registry;
    const Effect::MaterialResolver resolver{ registry };
    System system{ "Effect material resolution" };

    auto& sprite = system.AddEmitter("Sprite", 8, 0.0f);
    sprite.SetMaterialDescription({
        .BaseColor = { 0.25f, 0.5f, 0.75f },
        .Opacity = 0.4f,
        .Emissive = {0.1f, 0.0f, 0.0f },
    });

    auto& ribbon = system.AddEmitter("Ribbon", 8, 0.0f);
    ribbon.SetRenderMode(EParticleRenderMode::Ribbon);

    ASSERT_TRUE(resolver.Resolve(system));

    ASSERT_TRUE(sprite.GetMaterial());
    EXPECT_NE(sprite.GetMaterial()->GetParent(), registry.GetDefault(EMaterialUsage::Particle));

    EXPECT_EQ(sprite.GetMaterial()->GetParent()->GetUsage(), EMaterialUsage::Particle);

    ASSERT_TRUE(ribbon.GetMaterial());
    EXPECT_EQ(ribbon.GetMaterial()->GetParent(), registry.GetDefault(EMaterialUsage::Particle));

    EXPECT_TRUE(resolver.Resolve(system));
}

TEST(MaterialResolverTest, RefreshesAuthoredMaterialsWhenDescriptionsChange)
{
    MaterialRegistry registry;
    const Effect::MaterialResolver resolver{ registry };
    System system{ "Effect material refresh" };

    auto& sprite = system.AddEmitter("Sprite", 8, 0.0f);
    sprite.SetMaterialDescription({ .BaseColor = { 0.25f, 0.5f, 0.75f } });
    ASSERT_TRUE(resolver.Resolve(system));

    const auto initialMaterial = sprite.GetMaterial()->GetParent();

    sprite.SetMaterialDescription({ .BaseColor = { 0.75f, 0.5f, 0.25f } });
    ASSERT_TRUE(resolver.Resolve(system));

    ASSERT_TRUE(sprite.GetMaterial());
    EXPECT_NE(sprite.GetMaterial()->GetParent(), initialMaterial);
    EXPECT_EQ(
        registry.Find("Aether." + system.GetId() + ".Sprite"),
        sprite.GetMaterial()->GetParent()
    );
}
