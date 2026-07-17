#include <gtest/gtest.h>

#include <Engine/Graphics/Material/MaterialGPUParameterCache.h>

using namespace Elixir;

namespace
{
    Ref<const MaterialRenderProxy> MakeProxy(const float roughness)
    {
        const Ref<Material> material = Material::CreateStandardPBR();
        MaterialInstance instance(material);
        instance.SetScalar("Roughness", roughness);
        return MaterialRenderProxy::Create(instance);
    }
}

TEST(MaterialGPUParameterCache, MarksEverySlotDirtyOnFirstPublication)
{
    MaterialGPUParameterCache cache;
    const auto first = MakeProxy(0.25f);
    const auto second = MakeProxy(0.75f);

    const auto update = cache.Update({ first, second });

    EXPECT_TRUE(update.LayoutChanged);
    EXPECT_EQ(update.DirtySlots, (std::vector<uint32_t>{ 0, 1 }));
    EXPECT_TRUE(update.HasChanges());
    ASSERT_EQ(cache.GetProxies().size(), 2u);
    EXPECT_EQ(cache.GetProxies()[0], first);
    EXPECT_EQ(cache.GetProxies()[1], second);
}

TEST(MaterialGPUParameterCache, ReusesAnUnchangedProxyList)
{
    MaterialGPUParameterCache cache;
    const auto first = MakeProxy(0.25f);
    const auto second = MakeProxy(0.75f);
    cache.Update({ first, second });

    const auto update = cache.Update({ first, second });

    EXPECT_FALSE(update.LayoutChanged);
    EXPECT_TRUE(update.DirtySlots.empty());
    EXPECT_FALSE(update.HasChanges());
}

TEST(MaterialGPUParameterCache, InvalidatesOnlyTheReplacedSlot)
{
    MaterialGPUParameterCache cache;
    const auto first = MakeProxy(0.25f);
    const auto second = MakeProxy(0.75f);
    cache.Update({ first, second });
    const auto replacement = MakeProxy(0.5f);

    const auto update = cache.Update({ first, replacement });

    EXPECT_FALSE(update.LayoutChanged);
    EXPECT_EQ(update.DirtySlots, (std::vector<uint32_t>{ 1 }));
    EXPECT_EQ(cache.GetProxies()[0], first);
    EXPECT_EQ(cache.GetProxies()[1], replacement);
}

TEST(MaterialGPUParameterCache, RebuildsTheLayoutWhenSlotsAreAddedOrRemoved)
{
    MaterialGPUParameterCache cache;
    const auto first = MakeProxy(0.25f);
    const auto second = MakeProxy(0.75f);
    cache.Update({ first });

    const auto grown = cache.Update({ first, second });
    EXPECT_TRUE(grown.LayoutChanged);
    EXPECT_EQ(grown.DirtySlots, (std::vector<uint32_t>{ 1 }));

    const auto shrunk = cache.Update({ first });
    EXPECT_TRUE(shrunk.LayoutChanged);
    EXPECT_TRUE(shrunk.DirtySlots.empty());
    EXPECT_TRUE(shrunk.HasChanges());
}
