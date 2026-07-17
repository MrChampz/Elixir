#include <gtest/gtest.h>

#include <Engine/Graphics/MeshRenderer.h>
#include <Engine/Graphics/Model.h>

#include <thread>
#include <unordered_map>
#include <unordered_set>

using namespace Elixir;

TEST(ModelRenderHandle, DistinguishesLiveModels)
{
    const Ref<Model> first = CreateRef<Model>();
    const Ref<Model> second = CreateRef<Model>();

    EXPECT_TRUE(first->GetRenderHandle().IsValid());
    EXPECT_TRUE(second->GetRenderHandle().IsValid());
    EXPECT_NE(first->GetRenderHandle(), second->GetRenderHandle());
}

TEST(ModelRenderHandle, IncrementsTheGenerationWhenAnIdIsReused)
{
    Ref<Model> first = CreateRef<Model>();
    const SModelRenderHandle released = first->GetRenderHandle();
    first.reset();

    const Ref<Model> replacement = CreateRef<Model>();
    const SModelRenderHandle acquired = replacement->GetRenderHandle();

    EXPECT_EQ(acquired.Id, released.Id);
    EXPECT_NE(acquired.Generation, released.Generation);

    std::unordered_map<SModelRenderHandle, int, SModelRenderHandleHash> states;
    states[released] = 1;
    states[acquired] = 2;
    EXPECT_EQ(states.size(), 2u);
}

TEST(ModelRenderHandle, AllocatesUniqueHandlesAcrossConcurrentLifetimes)
{
    constexpr size_t MODEL_COUNT = 16;
    std::vector<Ref<Model>> models(MODEL_COUNT);
    std::vector<std::thread> workers;
    workers.reserve(MODEL_COUNT);
    for (size_t index = 0; index < MODEL_COUNT; ++index)
        workers.emplace_back([&, index] { models[index] = CreateRef<Model>(); });
    for (std::thread& worker : workers)
        worker.join();

    std::unordered_set<SModelRenderHandle, SModelRenderHandleHash> handles;
    for (const Ref<Model>& model : models)
        handles.insert(model->GetRenderHandle());
    EXPECT_EQ(handles.size(), MODEL_COUNT);
}

TEST(ModelRenderHandle, ScopesMaterialSlotsToTheirOwningModel)
{
    const Ref<Model> first = CreateRef<Model>();
    const Ref<Model> second = CreateRef<Model>();

    std::unordered_map<SModelMaterialHandle, int,
        SModelMaterialHandleHash> materials;
    materials[{ first->GetRenderHandle(), 0 }] = 1;
    materials[{ second->GetRenderHandle(), 0 }] = 2;
    materials[{ first->GetRenderHandle(), 1 }] = 3;

    EXPECT_EQ(materials.size(), 3u);
    EXPECT_EQ(materials.at({ first->GetRenderHandle(), 0 }), 1);
    EXPECT_EQ(materials.at({ second->GetRenderHandle(), 0 }), 2);
    EXPECT_EQ(materials.at({ first->GetRenderHandle(), 1 }), 3);
}
