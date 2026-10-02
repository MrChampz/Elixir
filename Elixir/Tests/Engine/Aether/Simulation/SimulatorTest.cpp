#include <gtest/gtest.h>

#include <concepts>
#include <type_traits>
#include <utility>

#include <Engine/Aether/Rendering/FrameSubmission.h>
#include <Engine/Aether/Simulation/Simulator.h>
#include <Engine/Aether/Simulation/ParticleOpData.h>

namespace Elixir::Materials { class MaterialSystem; }

using namespace Elixir;
using namespace Elixir::Aether::Core;
using namespace Elixir::Aether::Rendering;
using namespace Elixir::Aether::Simulation;

using TSimulatorResult = decltype(
    std::declval<Simulator&>().Simulate(
        std::declval<const FrameSubmission&>(),
        std::declval<const Ref<CommandBuffer>&>()
    )
);

static_assert(std::same_as<TSimulatorResult, Ref<const RenderFrame>>);
static_assert(std::is_constructible_v<
    Simulator,
    const GraphicsContext*,
    const ShaderLoader*,
    const SResourcePoolLimits&
>);
static_assert(!std::is_constructible_v<
    Simulator,
    const GraphicsContext*,
    Materials::MaterialSystem&
>);

TEST(SimulatorTest, MetricsContainOnlySimulationResults)
{
    const SSimulationMetrics metrics{
        .SubmissionSerial = 12u,
        .SubmittedSystemInstanceCount = 3u,
        .SimulationBatchCount = 2u,
    };

    EXPECT_EQ(metrics.SubmissionSerial, 12u);
    EXPECT_EQ(metrics.SubmittedSystemInstanceCount, 3u);
    EXPECT_EQ(metrics.SimulationBatchCount, 2u);
}

TEST(SimulatorTest, RebasesConeAngleAndPreservesLiteralSentinels)
{
    for (const uint32_t offset : { 0u, 100u })
    {
        SCOPED_TRACE(offset);
        for (const float angleIndex : { 0.0f, 2.0f, -1.0f })
        {
            SCOPED_TRACE(angleIndex);
            const Elixir::Aether::Modules::SGPUParticleOp operation{
                .Type = EParticleOp::SampleCone,
                .Target = EParticleAttribute::Velocity,
                .Parameter0Index = 1u,
                .Parameter1Index = UINT32_MAX,
                .Data0 = glm::vec4(0, 1, 0, 0.25f),
                .Data1 = glm::vec4(3, 4, angleIndex, 0),
            };
            const auto uploaded = Detail::ToOpData(operation, offset);
            EXPECT_FLOAT_EQ(uploaded.Header.z, float(offset + 1u));
            EXPECT_FLOAT_EQ(uploaded.Header.w, -1.0f);
            EXPECT_FLOAT_EQ(uploaded.Data1.z, angleIndex < 0 ? -1.0f : float(offset) + angleIndex);
            EXPECT_EQ(uploaded.Data0, operation.Data0);
            EXPECT_FLOAT_EQ(uploaded.Data1.x, 3.0f);
            EXPECT_FLOAT_EQ(uploaded.Data1.y, 4.0f);
            EXPECT_EQ(uploaded.Data2, operation.Data2);
            EXPECT_FLOAT_EQ(operation.Data1.z, angleIndex);
        }
    }
}

TEST(SimulatorTest, RebasesVortexIndicesWithoutChangingOtherPayloads)
{
    Elixir::Aether::Modules::SGPUParticleOp operation{
        .Type = EParticleOp::ApplyVortex,
        .Data1 = glm::vec4(0, 0, 1, 0),
        .Data2 = glm::vec4(3, 4, 2, -1),
    };
    const auto vortex = Detail::ToOpData(operation, 100u);
    EXPECT_EQ(vortex.Data1, operation.Data1);
    EXPECT_EQ(vortex.Data2, glm::vec4(3, 4, 102, -1));

    operation.Type = EParticleOp::RandomRange;
    const auto range = Detail::ToOpData(operation, 100u);
    EXPECT_EQ(range.Data1, operation.Data1);
    EXPECT_EQ(range.Data2, operation.Data2);
}
