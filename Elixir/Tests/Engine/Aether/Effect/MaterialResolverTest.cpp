#include <gtest/gtest.h>

#include <Engine/Aether/System.h>
#include <Engine/Aether/Effect/MaterialResolver.h>
#include <Engine/Material/MaterialRegistry.h>

using namespace Elixir;
using namespace Elixir::Aether;
using namespace Elixir::Aether::Core;

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
    EXPECT_NE(sprite.GetMaterial()->GetParent(), registry.GetDefault(EMaterialUsage::ParticleSprite));

    EXPECT_TRUE(sprite.GetMaterial()->GetParent()->SupportsUsage(EMaterialUsage::ParticleSprite));

    ASSERT_TRUE(ribbon.GetMaterial());
    EXPECT_EQ(ribbon.GetMaterial()->GetParent(), registry.GetDefault(EMaterialUsage::ParticleRibbon));

    EXPECT_TRUE(resolver.Resolve(system));
}
