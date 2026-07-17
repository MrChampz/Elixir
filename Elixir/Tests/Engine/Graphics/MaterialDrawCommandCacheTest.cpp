#include <gtest/gtest.h>

#include <Engine/Graphics/Material/MaterialDrawCommandCache.h>
#include <Engine/Graphics/Material/MaterialRenderProxy.h>

using namespace Elixir;

namespace
{
    Ref<const MaterialResource> MakeResource(const float roughness)
    {
        const Ref<Material> material = Material::CreateStandardPBR();
        MaterialInstance instance(material);
        instance.SetScalar("Roughness", roughness);
        return MaterialRenderProxy::Create(instance)->GetResource();
    }
}

TEST(MaterialDrawCommandCache, MarksEverySlotDirtyOnFirstBuild)
{
    MaterialDrawCommandCache cache;
    const auto first = MakeResource(0.25f);
    const auto second = MakeResource(0.75f);

    const auto update = cache.Update({ { first, 1, false }, { second, 1, true } });

    EXPECT_TRUE(update.LayoutChanged);
    EXPECT_EQ(update.DirtySlots, (std::vector<uint32_t>{ 0, 1 }));
    EXPECT_TRUE(update.HasChanges());
}

TEST(MaterialDrawCommandCache, ReusesUnchangedCommands)
{
    MaterialDrawCommandCache cache;
    const auto resource = MakeResource(0.25f);
    cache.Update({ { resource, 1, false } });

    const auto update = cache.Update({ { resource, 1, false } });

    EXPECT_FALSE(update.LayoutChanged);
    EXPECT_TRUE(update.DirtySlots.empty());
    EXPECT_FALSE(update.HasChanges());
}

TEST(MaterialDrawCommandCache, InvalidatesAChangedResourceBindingOrPass)
{
    MaterialDrawCommandCache cache;
    const auto first = MakeResource(0.25f);
    const auto second = MakeResource(0.75f);
    const auto replacement = MakeResource(0.5f);
    cache.Update({ { first, 1, false }, { second, 1, false } });

    const auto resourceUpdate = cache.Update(
        { { replacement, 1, false }, { second, 1, false } });
    EXPECT_EQ(resourceUpdate.DirtySlots, (std::vector<uint32_t>{ 0 }));

    const auto bindingUpdate = cache.Update(
        { { replacement, 1, false }, { second, 2, false } });
    EXPECT_EQ(bindingUpdate.DirtySlots, (std::vector<uint32_t>{ 1 }));

    const auto passUpdate = cache.Update(
        { { replacement, 1, true }, { second, 2, false } });
    EXPECT_EQ(passUpdate.DirtySlots, (std::vector<uint32_t>{ 0 }));
}

TEST(MaterialDrawCommandCache, RebuildsWhenTheSlotLayoutChanges)
{
    MaterialDrawCommandCache cache;
    const auto first = MakeResource(0.25f);
    const auto second = MakeResource(0.75f);
    cache.Update({ { first, 1, false } });

    const auto grown = cache.Update({ { first, 1, false }, { second, 1, false } });
    EXPECT_TRUE(grown.LayoutChanged);
    EXPECT_EQ(grown.DirtySlots, (std::vector<uint32_t>{ 1 }));

    const auto shrunk = cache.Update({ { first, 1, false } });
    EXPECT_TRUE(shrunk.LayoutChanged);
    EXPECT_TRUE(shrunk.DirtySlots.empty());
    EXPECT_TRUE(shrunk.HasChanges());
}
