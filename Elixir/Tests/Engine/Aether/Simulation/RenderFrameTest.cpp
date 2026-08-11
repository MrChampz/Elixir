#include <gtest/gtest.h>

#include <concepts>
#include <type_traits>
#include <utility>

#include <Engine/Aether/Simulation/RenderFrame.h>

using namespace Elixir;
using namespace Elixir::Aether::Core;
using namespace Elixir::Aether::Simulation;

static_assert(!std::is_copy_constructible_v<RenderFrame>);
static_assert(!std::is_copy_assignable_v<RenderFrame>);
static_assert(!std::is_move_constructible_v<RenderFrame>);
static_assert(!std::is_move_assignable_v<RenderFrame>);

static_assert(std::same_as<
    decltype(std::declval<const RenderFrame&>().GetResources()),
    const std::vector<SParticleStateRenderResource>&
>);
static_assert(std::same_as<
    decltype(std::declval<const RenderFrame&>().GetEmitterBuffer()),
    const Ref<DynamicStorageBuffer>&
>);
static_assert(std::same_as<
    decltype(std::declval<const RenderFrame&>().GetItems()),
    const std::vector<SRenderItem>&
>);

TEST(RenderFrameTest, PublishesResolvedSimulationData)
{
    SSystemInstanceAllocation allocation{
        .InstanceIndex = 7u,
        .ParticleStateLayout = EParticleStateLayout::CoreV1,
        .Generation = 3u,
        .Particles = { .Offset = 128u, .Count = 64u },
        .Emitters = { .Offset = 11u, .Count = 2u },
    };

    glm::mat4 transform{ 1.0f };
    transform[3] = { 4.0f, 5.0f, 6.0f, 1.0f };

    std::vector<SParticleStateRenderResource> resources{
        {
            .Layout = EParticleStateLayout::CoreV1,
            .ParticleStateBuffer = {},
        },
    };
    std::vector<SRenderItem> items{
        {
            .Allocation = allocation,
            .ParticleStateLayout = EParticleStateLayout::CoreV1,
            .RenderMode = EParticleRenderMode::Ribbon,
            .Material = {},
            .WorldTransform = transform,
            .EmitterIndex = 1u,
            .LocalParticleOffset = 16u,
            .ParticleCount = 48u,
        },
    };

    const RenderFrame frame{
        std::move(resources),
        {},
        std::move(items),
        42u,
        3.5f
    };

    ASSERT_EQ(frame.GetResources().size(), 1u);
    EXPECT_EQ(frame.GetResources()[0].Layout, EParticleStateLayout::CoreV1);
    EXPECT_FALSE(frame.GetResources()[0].ParticleStateBuffer);
    EXPECT_FALSE(frame.GetEmitterBuffer());

    ASSERT_EQ(frame.GetItems().size(), 1u);
    const auto& item = frame.GetItems()[0];
    EXPECT_EQ(item.Allocation.InstanceIndex, 7u);
    EXPECT_EQ(item.Allocation.Generation, 3u);
    EXPECT_EQ(item.Allocation.Particles.Offset, 128u);
    EXPECT_EQ(item.Allocation.Emitters.Offset, 11u);
    EXPECT_EQ(item.ParticleStateLayout, EParticleStateLayout::CoreV1);
    EXPECT_EQ(item.RenderMode, EParticleRenderMode::Ribbon);
    EXPECT_EQ(item.EmitterIndex, 1u);
    EXPECT_EQ(item.LocalParticleOffset, 16u);
    EXPECT_EQ(item.ParticleCount, 48u);
    EXPECT_EQ(item.WorldTransform, transform);
    EXPECT_FALSE(item.Material);

    EXPECT_EQ(frame.GetSubmissionSerial(), 42u);
    EXPECT_FLOAT_EQ(frame.GetElapsedTimeSeconds(), 3.5f);
}
