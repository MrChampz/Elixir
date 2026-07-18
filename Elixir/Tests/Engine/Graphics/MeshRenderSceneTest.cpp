#include <gtest/gtest.h>

#include <Engine/Graphics/MeshRenderScene.h>

#include <thread>

using namespace Elixir;

namespace
{
    Ref<const MeshSceneProxy> MakeProxy(const SModelRenderHandle handle,
        const Ref<SMeshSceneLifetime>& lifetime)
    {
        return CreateRef<const MeshSceneProxy>(handle,
            CreateRef<const MeshSceneProxy::PrimitiveList>(),
            CreateRef<const MeshSceneProxy::MaterialList>(), lifetime);
    }
}

TEST(MeshRenderScene, AppliesUpdatesAndRemovalAtFrameBoundaries)
{
    MeshRenderScene scene;
    const auto lifetime = CreateRef<SMeshSceneLifetime>();
    const SModelRenderHandle handle{ 7, 3 };
    const Ref<const MeshSceneProxy> first = MakeProxy(handle, lifetime);
    const Ref<const MeshSceneProxy> replacement = MakeProxy(handle, lifetime);

    scene.QueueUpdate(first);
    EXPECT_TRUE(scene.GetProxies().empty());
    EXPECT_TRUE(scene.ApplyPending().empty());
    ASSERT_EQ(scene.GetProxies().size(), 1u);
    EXPECT_EQ(scene.GetProxies().front(), first);

    scene.QueueUpdate(replacement);
    EXPECT_EQ(scene.GetProxies().front(), first);
    EXPECT_TRUE(scene.ApplyPending().empty());
    ASSERT_EQ(scene.GetProxies().size(), 1u);
    EXPECT_EQ(scene.GetProxies().front(), replacement);

    scene.QueueRemove(handle);
    EXPECT_EQ(scene.GetProxies().size(), 1u);
    EXPECT_EQ(scene.ApplyPending(), std::vector<SModelRenderHandle>{ handle });
    EXPECT_TRUE(scene.GetProxies().empty());
}

TEST(MeshRenderScene, RetiresAProxyWhenItsModelLifetimeEnds)
{
    MeshRenderScene scene;
    Ref<Model> model = CreateRef<Model>();
    model->PublishMaterialRenderProxies();
    const SModelRenderHandle handle = model->GetRenderHandle();

    scene.QueueUpdate(model->CreateSceneProxy());
    EXPECT_TRUE(scene.ApplyPending().empty());
    ASSERT_EQ(scene.GetProxies().size(), 1u);

    model.reset();
    EXPECT_EQ(scene.ApplyPending(), std::vector<SModelRenderHandle>{ handle });
    EXPECT_TRUE(scene.GetProxies().empty());
}

TEST(MeshRenderScene, AcceptsConcurrentProxyPublication)
{
    constexpr size_t PROXY_COUNT = 16;
    MeshRenderScene scene;
    const auto lifetime = CreateRef<SMeshSceneLifetime>();
    std::vector<std::thread> producers;
    producers.reserve(PROXY_COUNT);

    for (size_t index = 0; index < PROXY_COUNT; ++index)
    {
        producers.emplace_back([&, index]
        {
            scene.QueueUpdate(MakeProxy(
                { (uint64_t)index, 1 }, lifetime));
        });
    }
    for (std::thread& producer : producers)
        producer.join();

    EXPECT_TRUE(scene.ApplyPending().empty());
    EXPECT_EQ(scene.GetProxies().size(), PROXY_COUNT);
}

TEST(MeshSceneProxy, ReusesGeometryAcrossMaterialSnapshots)
{
    const Ref<Model> model = CreateRef<Model>();
    model->PublishMaterialRenderProxies();
    const Ref<const MeshSceneProxy> first = model->CreateSceneProxy();

    model->PublishMaterialRenderProxies();
    const Ref<const MeshSceneProxy> second = model->CreateSceneProxy();

    EXPECT_NE(first, second);
    EXPECT_EQ(first->GetPrimitiveSnapshot(), second->GetPrimitiveSnapshot());
    EXPECT_NE(first->GetMaterials(), second->GetMaterials());
    EXPECT_TRUE(first->IsOwnerAlive());
    EXPECT_TRUE(second->IsOwnerAlive());
}
