#include <gtest/gtest.h>

#include "TestInstanceRegistry.h"
#include <Engine/Aether/Effect/Effect.h>
#include <Engine/Aether/Modules/DistanceConstraint.h>
#include <Engine/Aether/Modules/StrandGrowth.h>
#include <Engine/Aether/Simulation/ConstraintData.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace Elixir;
using namespace Elixir::Aether;

namespace
{
    Ref<System> MakeGrowingSystem()
    {
        auto system = CreateRef<System>("Growing chains");
        auto& emitter = system->AddEmitter("Vines", 48, 0);
        emitter.SetKillOnLifetimeExpired(false);
        emitter.SetBurst(48, 0);
        Simulation::SDistanceConstraint constraint;
        constraint.MaxPoints = 12;
        constraint.OrderStep = 4;
        Core::SStrandGrowthSettings growth;
        growth.Speed = 0.2f;
        growth.BranchCount = 1;
        growth.BranchInterval = 4;
        emitter.AddModule(CreateScope<Modules::StrandGrowth>(growth));
        emitter.AddModule(CreateScope<Modules::DistanceConstraint>(constraint));
        return system;
    }
}

TEST(ChainDevelopmentTest, CompilesGrowthIntoExistingParticleConstraint)
{
    auto system = MakeGrowingSystem();
    TestInstanceRegistry registry;
    auto instance = registry.CreateRegisteredInstance(system);
    ASSERT_TRUE(instance);
    Aether::Rendering::FrameSubmission submission;
    ASSERT_TRUE(submission.Submit(*instance));
    const auto& compiled = submission.GetRenderProxies().front()->GetCompiledSystem();
    ASSERT_EQ(compiled.ModuleData.Constraints.size(), 1u);
    EXPECT_TRUE(compiled.ModuleData.Constraints.front().HasGrowth);
    EXPECT_EQ(compiled.ModuleData.Constraints.front().Growth.BranchCount, 1u);
    EXPECT_EQ(compiled.Emitters.front().MaxParticles, 48u);
    EXPECT_EQ(compiled.Emitters.front().UpdateOperationCount, 0u);
}

TEST(ChainDevelopmentTest, RejectsIncompleteBurstAndExpiringParticles)
{
    for (int invalid = 0; invalid < 4; ++invalid)
    {
        auto system = MakeGrowingSystem();
        auto& emitter = *system->GetEmitters().front();
        if (invalid == 0)
            emitter.SetBurst(47, 0);
        if (invalid == 1)
            emitter.SetBurst(48, 1);
        if (invalid == 2)
            emitter.SetKillOnLifetimeExpired(true);
        if (invalid == 3)
            emitter.AddModule(CreateScope<Modules::StrandGrowth>());
        TestInstanceRegistry registry;
        EXPECT_FALSE(registry.CreateRegisteredInstance(system)) << invalid;
    }
}

TEST(ChainDevelopmentTest, ValidatesBranchTopologyAndEndpointCompatibility)
{
    Simulation::SDistanceConstraint config;
    config.HasGrowth = true;
    config.Growth.Speed = 1;
    config.Growth.BranchCount = 2;
    config.Growth.BranchInterval = 4;
    EXPECT_FALSE(config.IsValid());
    config.OrderStep = 3;
    EXPECT_TRUE(config.IsValid());
    config.EndAttachment.Enabled = true;
    EXPECT_FALSE(config.IsValid());
    config.Growth.State = Core::EStrandGrowthState::Mature;
    EXPECT_TRUE(config.IsValid());
    config.Growth.BranchInterval = 12;
    EXPECT_FALSE(config.IsValid());
}

TEST(ChainDevelopmentTest, PacksWorldSurfacesAndEndpointSpaceWithoutMutatingSettings)
{
    Simulation::SDistanceConstraint config;
    config.EndAttachment.Enabled = true;
    config.EndAttachment.Space = Core::EStrandEndSpace::World;
    config.EndAttachment.Position = {1, 2, 3};
    config.SurfaceAttachment.Count = 1;
    auto& shape = config.SurfaceAttachment.Surfaces.front();
    shape.Type = Core::EStrandSurfaceType::Plane;
    shape.Normal = {0, 0, 2};
    auto transform = glm::translate(glm::mat4(1), glm::vec3(3, 4, 5));
    const auto data = Simulation::BuildConstraintFrameData(config, transform);
    ASSERT_TRUE(data);
    EXPECT_EQ(data->Counts, glm::uvec4(0, 1, 1, 1));
    EXPECT_EQ(glm::vec3(data->Surfaces.front().NormalRadius), glm::vec3(0, 0, 1));
    EXPECT_EQ(glm::vec3(data->Simulation * glm::vec4(3, 4, 5, 1)), glm::vec3(0));
    EXPECT_EQ(shape.Normal.z, 2);
    EXPECT_FALSE(Simulation::BuildConstraintFrameData(config, glm::mat4(0)));
    config.SurfaceAttachment.Surfaces.front().Normal = {};
    EXPECT_FALSE(Simulation::BuildConstraintFrameData(config, transform));
}

TEST(ChainDevelopmentTest, LoadsMigratedSamplesUsingCurrentParticleBudgets)
{
    const auto root = std::filesystem::path(__FILE__)
                          .parent_path()
                          .parent_path()
                          .parent_path()
                          .parent_path()
                          .parent_path();
    for (const auto* name :
         {"StrandIvy",
          "StrandClimbingIvy",
          "StrandFern",
          "StrandWillow",
          "StrandPowerLines",
          "StrandBalloons"})
    {
        SCOPED_TRACE(name);
        const auto system =
            Effect::LoadEffectFile(root / "Assets/VFX" / (std::string(name) + ".json"));
        ASSERT_TRUE(system);
        TestInstanceRegistry registry;
        EXPECT_TRUE(registry.CreateRegisteredInstance(system));
    }
}

TEST(ChainDevelopmentTest, RejectsMalformedSurfaceAndGrowthFields)
{
    for (const auto* module : {
             R"({"type":"StrandGrowth","state":"Unknown"})",
             R"({"type":"StrandGrowth","speed":-1})",
             R"({"type":"StrandGrowth","branchCount":5})",
             R"({"type":"DistanceConstraint","endAttachment":{"space":"Bad"}})",
             R"({"type":"DistanceConstraint","surfaceAttachment":{"surfaces":[{"type":"Plane","normal":[0,0,0]}]}})",
             R"({"type":"DistanceConstraint","surfaceAttachment":{"surfaces":[{"type":"Box","halfExtents":[0,1,1]}]}})",
             R"({"type":"DistanceConstraint","surfaceAttachment":{"surfaces":[{"type":"Sphere","radius":-1}]}})"
         })
    {
        const auto json =
            std::string(
                R"({"name":"Invalid","emitters":[{"name":"Test","maxParticles":24,"spawnRate":0,"updateModules":[)"
            ) +
            module + "]}]}";
        std::string error;
        EXPECT_FALSE(Effect::ParseEffect(json, &error)) << module;
        EXPECT_FALSE(error.empty());
    }
}
