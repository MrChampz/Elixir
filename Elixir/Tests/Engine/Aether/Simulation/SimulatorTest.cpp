#include <gtest/gtest.h>

#include <concepts>
#include <type_traits>
#include <utility>

#include <Engine/Aether/Rendering/FrameSubmission.h>
#include <Engine/Aether/Manager.h>
#include <Engine/Aether/Simulation/Simulator.h>
#include <Engine/Aether/Core/Operation.h>

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

namespace
{
    template <typename T>
    concept HasReplay = requires(T& manager, const Camera& camera, const Ref<Image>& target)
    {
        manager.Replay(camera, target, 1.0f);
    };

    template <typename T>
    concept HasSimulatorReplay = requires(T& simulator, const FrameSubmission& submission,
        const Ref<CommandBuffer>& cmd)
    {
        simulator.Replay(submission, cmd, 1.0f);
    };

    template <typename T>
    concept HasPublicRenderSteps = requires(T& manager, const Camera& camera, const Ref<Image>& target)
    {
        manager.RenderSteps(camera, target, std::span<const float>{}, 0.0f);
    };

    class ExtendedManager final : public Aether::Manager
    {
    public:
        using Manager::RenderSteps;
    };
}

static_assert(!std::is_final_v<Aether::Manager>);
static_assert(std::has_virtual_destructor_v<Aether::Manager>);
static_assert(!HasReplay<Aether::Manager>);
static_assert(!HasSimulatorReplay<Simulator>);
static_assert(!HasPublicRenderSteps<Aether::Manager>);
static_assert(HasPublicRenderSteps<ExtendedManager>);

TEST(ManagerTest, ProtectedSimulationExtensionLinksOutsideTheEngine)
{
    // Referencing the protected method checks its DLL export when tests link on Windows.
    const auto renderSteps = &ExtendedManager::RenderSteps;
    EXPECT_NE(renderSteps, nullptr);
}
