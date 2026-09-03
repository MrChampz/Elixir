#include <gtest/gtest.h>

#include <concepts>
#include <type_traits>
#include <utility>

#include <Engine/Aether/Rendering/FrameSubmission.h>
#include <Engine/Aether/Simulation/Simulator.h>

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
