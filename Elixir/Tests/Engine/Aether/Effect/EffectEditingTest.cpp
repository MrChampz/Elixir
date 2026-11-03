#include <gtest/gtest.h>
#include <Engine/Aether/Effect/Effect.h>
#include "../TestInstanceRegistry.h"

using namespace Elixir;
using namespace Elixir::Aether;

TEST(EffectEditingTest, ParsesUnsavedDefinitionAndSkipsDisabledEmittersAndModules)
{
    std::string error;
    const auto system = Effect::LoadEffectText(R"({"name":"Edited","emitters":[
        {"name":"Active","maxParticles":100,"spawnRate":10,"spawnModules":[
            {"type":"SetLifetime","min":1,"max":2},
            {"type":"SetSize","enabled":false,"min":0.1,"max":0.5}]},
        {"name":"Disabled","enabled":false,"maxParticles":100,"spawnRate":10}]})", &error);
    ASSERT_TRUE(system) << error;
    ASSERT_EQ(system->GetEmitters().size(), 1u);
    EXPECT_EQ(system->GetEmitters()[0]->GetName(), "Active");
    TestInstanceRegistry runtime;
    const auto instance = runtime.CreateRegisteredInstance(system);
    ASSERT_TRUE(instance);
    Elixir::Aether::Rendering::FrameSubmission submission;
    ASSERT_TRUE(submission.Submit(*instance));
    const auto& compiled = submission.GetRenderProxies().front()->GetCompiledSystem();
    EXPECT_EQ(compiled.Emitters[0].SpawnOperationCount, 1u);
}

TEST(EffectEditingTest, ReturnsValidationErrorsForMalformedDefinitions)
{
    std::string error;
    EXPECT_EQ(Effect::LoadEffectText(R"({"name":"Bad","emitters":[{"enabled":"false"}]})", &error), nullptr);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(Effect::LoadEffectText("[]", &error), nullptr);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(Effect::LoadEffectText("{", &error), nullptr);
    EXPECT_FALSE(error.empty());
}

TEST(EffectEditingTest, SwapsDefinitionsWithoutChangingSystemIdentities)
{
    auto original = CreateRef<System>("Original");
    auto replacement = CreateRef<System>("Edited");
    const auto originalId = original->GetId();
    const auto replacementId = replacement->GetId();
    original->AddEmitter("Old emitter", 10, 1);
    replacement->AddEmitter("New emitter", 20, 2);
    original->GetParameters().SetFloat("Rate", 1);
    replacement->GetParameters().SetFloat("Rate", 2);
    original->Swap(*replacement);
    EXPECT_EQ(original->GetId(), originalId);
    EXPECT_EQ(replacement->GetId(), replacementId);
    EXPECT_EQ(original->GetName(), "Edited");
    EXPECT_EQ(original->GetEmitters()[0]->GetName(), "New emitter");
    EXPECT_FLOAT_EQ(original->GetParameters().GetFloat("Rate"), 2);
    original->Swap(*replacement);
    EXPECT_EQ(original->GetName(), "Original");
    EXPECT_EQ(original->GetEmitters()[0]->GetName(), "Old emitter");
    EXPECT_FLOAT_EQ(original->GetParameters().GetFloat("Rate"), 1);
}
