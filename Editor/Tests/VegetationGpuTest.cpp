#include <gtest/gtest.h>
#include <epch.h>
#include "VFX/Vegetation/VegetationEffect.h"
#include "VFX/Rendering/VegetationRenderer.h"
#include "VFX/Rendering/LeafRenderer.h"
#include "VFX/Rendering/BalloonRenderer.h"
#include "Graphics/Vulkan/VulkanTestContext.h"
#include <Engine/Aether/Manager.h>
#include <Engine/Materials/MaterialSystem.h>
#include <Engine/Materials/MaterialRegistry.h>
#include <Engine/Camera/PerspectiveCamera.h>
#include <Graphics/Vulkan/VulkanBuffer.h>
#include <Graphics/Vulkan/VulkanShader.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Graphics/CommandBuffer.h>

using namespace Elixir;
using namespace EditorVFX;

namespace
{
    class CapturingManager final : public Aether::Manager
    {
      public:
        using Manager::Manager;
        Ref<const Aether::Simulation::RenderFrame> LastFrame;

      protected:
        /**
         * @brief Retains the frame for application rendering and readback.
         * @param frame Final simulation output.
         * @param camera Unused camera.
         * @param target Unused color target.
         */
        void OnRenderFrame(
            const Ref<const Aether::Simulation::RenderFrame>& frame,
            const Camera& camera,
            const Ref<Image>& target
        ) override
        {
            LastFrame = frame;
        }
    };
}

TEST(EditorVegetationGpuTest, ApplicationStagesSimulateRenderAndPauseAllSamples)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    for (const auto* shaderName : {"Vegetation", "Leaf", "Balloon"})
    {
        const auto shader = loader.LoadShader("./EditorShaders", shaderName);
        ASSERT_TRUE(shader) << shaderName;
        const auto ranges =
            static_cast<Vulkan::VulkanShader*>(shader.get())->GetPushConstantRanges();
        ASSERT_FALSE(ranges.empty());
        EXPECT_LE(ranges.front().size, 128u);
        EXPECT_TRUE(shader->HasBinding("particles"));
    }
    for (const auto* name :
         {"StrandIvy",
          "StrandClimbingIvy",
          "StrandFern",
          "StrandWillow",
          "StrandPowerLines",
          "StrandBalloons"})
    {
        SCOPED_TRACE(name);
        const auto playback = CreateRef<VegetationPlayback>();
        std::ifstream stream(
            std::filesystem::path(ELIXIR_EDITOR_PROJECT_ROOT) / "Assets/VFX" /
            (std::string(name) + ".json")
        );
        const std::string source{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()
        };
        std::string error;
        const auto system = ParseVegetationEffect(source, &error, playback);
        ASSERT_TRUE(system) << error;
        Value asset;
        ASSERT_TRUE(Value::Parse(source, asset, error));
        std::vector<std::optional<LeafSettings>> leaves;
        std::vector<std::optional<BalloonSettings>> balloons;
        ASSERT_TRUE(ParseLeafRenderers(asset, leaves, error));
        ASSERT_TRUE(ParseBalloonRenderers(asset, balloons, error));
        VegetationRenderer tubes(context, &loader);
        LeafRenderer leafRenderer(context, &loader);
        BalloonRenderer balloonRenderer(context, &loader);
        leafRenderer.SetSettings(std::move(leaves));
        balloonRenderer.SetSettings(std::move(balloons));
        Materials::MaterialRegistry registry;
        Materials::MaterialSystem materials(
            context,
            {},
            &loader,
            context->GetRenderTarget(),
            context->GetDepthStencilRenderTarget()
        );
        CapturingManager manager(context, &loader, registry, materials);
        const auto instance = system->CreateInstance();
        ASSERT_TRUE(manager.Add(instance));
        PerspectiveCamera camera(45, 16.0f / 9, 0.01f, 100);
        camera.SetPosition({0, 0, 3});
        const auto render = [&]
        {
            context->RenderFrame(
                [&]
                {
                    materials.BeginFrame();
                    manager.BeginFrame(Timestep(0.1f));
                    manager.Render(camera, context->GetRenderTarget());
                    const auto result = materials.RenderFrame();
                    EXPECT_EQ(result.DrawCount, 0u);
                    tubes.Prepare(manager.LastFrame, camera);
                    leafRenderer.Prepare(manager.LastFrame, camera);
                    balloonRenderer.Prepare(manager.LastFrame, camera);
                    tubes.Render(
                        context->GetRenderTarget(),
                        context->GetDepthStencilRenderTarget()
                    );
                    leafRenderer.Render(
                        context->GetRenderTarget(),
                        context->GetDepthStencilRenderTarget()
                    );
                    balloonRenderer.Render(
                        context->GetRenderTarget(),
                        context->GetDepthStencilRenderTarget()
                    );
                }
            );
            ASSERT_TRUE(context->RunRenderTaskAndWait(
                []
                {
                }
            ));
        };
        for (int frame = 0; frame < 15; ++frame)
            render();
        context->WaitDeviceIdle();
        ASSERT_TRUE(manager.LastFrame);
        const auto item = manager.LastFrame->GetItems().front();
        const auto* config =
            dynamic_cast<const VegetationRenderConfig*>(item.RenderConfig.get());
        ASSERT_NE(config, nullptr);
        const auto readState = [&]() -> std::array<glm::vec4, 3>
        {
            const auto buffer = manager.LastFrame->GetResources().front().ParticleStateBuffer;
            auto info = StagingBuffer::CreateBufferInfo(buffer->GetSize(), nullptr);
            info.Usage = EBufferUsage::TransferDst;
            const Ref<StagingBuffer> readback =
                CreateRef<Vulkan::VulkanStagingBuffer>(context, info);
            const auto command = context->GetUploadCommandBuffer();
            command->Begin();
            buffer->Barrier(command, EPipelineStage::Transfer, EPipelineAccess::TransferRead);
            buffer->Copy(command, readback);
            command->End();
            command->Flush();
            const auto* data = static_cast<const glm::vec4*>(readback->Map());
            const auto base = item.Allocation.Particles.Offset + item.LocalParticleOffset;
            const auto last = base + (config->MaxPoints - 1) * config->OrderStep;
            const std::array<glm::vec4, 3> state{
                data[base * 6 + 2],
                data[last * 6],
                data[(base + config->OrderStep) * 6]
            };
            readback->Unmap();
            return state;
        };
        const auto beforeState = readState();
        const auto before = beforeState.front();
        for (const auto& value : beforeState)
            EXPECT_TRUE(
                std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z)
            );
        if (config->Constraint.EndAttachment.Enabled)
            EXPECT_EQ(glm::vec3(beforeState[1]), config->Constraint.EndAttachment.Position);
        if (config->Constraint.HasGrowth)
        {
            EXPECT_GT(before.z, 1.0f);
            EXPECT_GT(before.w, 2.0f);
        }
        playback->Paused.store(true);
        for (int frame = 0; frame < 3; ++frame)
            render();
        context->WaitDeviceIdle();
        const auto paused = readState().front();
        EXPECT_EQ(paused.z, before.z);
        EXPECT_EQ(paused.w, before.w);
        EXPECT_TRUE(manager.Remove(instance));
    }
}
