#include <gtest/gtest.h>
#include <epch.h>
#include "VFX/Vegetation/VegetationEffect.h"
#include "VFX/Vegetation/VegetationData.h"
#include "Engine/Aether/TestInstanceRegistry.h"
#include <Engine/Aether/Effect/Effect.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace Elixir;
using namespace EditorVFX;

namespace
{
    std::string LoadSample(const char* name)
    {
        std::ifstream stream(
            std::filesystem::path(ELIXIR_EDITOR_PROJECT_ROOT) / "Assets/VFX" /
            (std::string(name) + ".json")
        );
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }
}

TEST(EditorVegetationTest, ApplicationOwnsDevelopmentAndUsesGenericEngineExtension)
{
    for (const auto* name :
         {"StrandIvy",
          "StrandClimbingIvy",
          "StrandFern",
          "StrandWillow",
          "StrandPowerLines",
          "StrandBalloons"})
    {
        SCOPED_TRACE(name);
        std::string error;
        const auto system = ParseVegetationEffect(LoadSample(name), &error);
        ASSERT_TRUE(system) << error;
        TestInstanceRegistry registry;
        const auto instance = registry.CreateRegisteredInstance(system);
        ASSERT_TRUE(instance);
        Aether::Rendering::FrameSubmission submission;
        ASSERT_TRUE(submission.Submit(*instance));
        const auto& compiled = submission.GetRenderProxies().front()->GetCompiledSystem();
        EXPECT_TRUE(compiled.ModuleData.Constraints.empty());
        for (const auto& emitter : compiled.Emitters)
        {
            EXPECT_EQ(emitter.RenderMode, Aether::Core::EParticleRenderMode::None);
            EXPECT_NE(emitter.Processor, nullptr);
            EXPECT_NE(
                dynamic_cast<const VegetationRenderConfig*>(emitter.RenderConfig.get()),
                nullptr
            );
        }
    }
}

TEST(EditorVegetationTest, EngineDoesNotRecognizeApplicationGrowthModule)
{
    std::string error;
    EXPECT_FALSE(Aether::Effect::ParseEffect(LoadSample("StrandIvy"), &error));
    EXPECT_FALSE(error.empty());
}

TEST(EditorVegetationTest, PreservesOrdinaryAetherEffectsWithoutApplicationProcessing)
{
    std::string error;
    const auto system = ParseVegetationEffect(LoadSample("StrandPlayground"), &error);
    ASSERT_TRUE(system) << error;
    EXPECT_EQ(system->GetEmitters().front()->GetParticleProcessor(), nullptr);
    EXPECT_EQ(
        system->GetEmitters().front()->GetRenderMode(),
        Aether::Core::EParticleRenderMode::Strand
    );
}

TEST(EditorVegetationTest, RejectsDuplicateGrowthAndIncompleteBudgets)
{
    Value asset;
    std::string error;
    ASSERT_TRUE(Value::Parse(LoadSample("StrandIvy"), asset, error));
    auto& emitter = asset.Object["emitters"].Array.front();
    auto& modules = emitter.Object["updateModules"].Array;
    modules.push_back(modules.front());
    EXPECT_FALSE(ParseVegetationEffect(asset.JSON(), &error));
    modules.pop_back();
    emitter.Object["maxParticles"].Text = "1";
    EXPECT_FALSE(ParseVegetationEffect(asset.JSON(), &error));
}

TEST(EditorVegetationTest, ValidatesBranchTopologyAndEndpointCompatibility)
{
    VegetationSettings config;
    config.HasGrowth = true;
    config.Growth.Speed = 1;
    config.Growth.BranchCount = 2;
    config.Growth.BranchInterval = 4;
    EXPECT_FALSE(config.IsValid());
    config.OrderStep = 3;
    EXPECT_TRUE(config.IsValid());
    config.EndAttachment.Enabled = true;
    EXPECT_FALSE(config.IsValid());
    config.Growth.State = EStrandGrowthState::Mature;
    EXPECT_TRUE(config.IsValid());
    config.Growth.BranchInterval = 12;
    EXPECT_FALSE(config.IsValid());
}

TEST(EditorVegetationTest, PacksWorldSurfacesAndEndpointSpaceWithoutMutatingSettings)
{
    VegetationSettings config;
    config.EndAttachment.Enabled = true;
    config.EndAttachment.Space = EStrandEndSpace::World;
    config.EndAttachment.Position = {1, 2, 3};
    config.SurfaceAttachment.Count = 1;
    auto& shape = config.SurfaceAttachment.Surfaces.front();
    shape.Type = EStrandSurfaceType::Plane;
    shape.Normal = {0, 0, 2};
    auto transform = glm::translate(glm::mat4(1), glm::vec3(3, 4, 5));
    const auto data = BuildConstraintFrameData(config, transform);
    ASSERT_TRUE(data);
    EXPECT_EQ(data->Counts, glm::uvec4(0, 1, 1, 1));
    EXPECT_EQ(glm::vec3(data->Surfaces.front().NormalRadius), glm::vec3(0, 0, 1));
    EXPECT_EQ(glm::vec3(data->Simulation * glm::vec4(3, 4, 5, 1)), glm::vec3(0));
    EXPECT_EQ(shape.Normal.z, 2);
    EXPECT_FALSE(BuildConstraintFrameData(config, glm::mat4(0)));
    config.SurfaceAttachment.Surfaces.front().Normal = {};
    EXPECT_FALSE(BuildConstraintFrameData(config, transform));
}
