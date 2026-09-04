#include <gtest/gtest.h>

#include <type_traits>

#include <Engine/Aether/Rendering/FrameSubmission.h>
#include <Engine/Aether/Rendering/Renderer.h>

namespace Elixir { class ShaderLoader; }

using namespace Elixir;
using namespace Elixir::Aether::Rendering;

namespace
{
    template <typename T>
    concept RendersFrameSubmission = requires(
        T& renderer,
        const FrameSubmission& submission,
        const Camera& camera,
        const Ref<CommandBuffer>& cmd
    )
    {
        renderer.Render(submission, camera, cmd);
    };
}

static_assert(!RendersFrameSubmission<Aether::Rendering::Renderer>);
static_assert(std::is_constructible_v<
    Aether::Rendering::Renderer,
    const GraphicsContext*
>);
static_assert(!std::is_constructible_v<
    Aether::Rendering::Renderer,
    const GraphicsContext*,
    const ShaderLoader*
>);

TEST(RendererTest, MetricsContainOnlyRenderingResults)
{
    constexpr SRenderingMetrics metrics{
        .SubmissionSerial = 12u,
        .RenderBatchCount = 4u,
        .SubmittedRenderItemCount = 6u,
        .SubmittedMaterialCount = 2u,
    };

    EXPECT_EQ(metrics.SubmissionSerial, 12u);
    EXPECT_EQ(metrics.RenderBatchCount, 4u);
    EXPECT_EQ(metrics.SubmittedRenderItemCount, 6u);
    EXPECT_EQ(metrics.SubmittedMaterialCount, 2u);
}
