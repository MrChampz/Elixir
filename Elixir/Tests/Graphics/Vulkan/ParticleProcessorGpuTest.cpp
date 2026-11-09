#include <gtest/gtest.h>
#include "VulkanTestContext.h"
#include "../../Engine/Aether/TestInstanceRegistry.h"
#include <Engine/Aether/Simulation/Simulator.h>
#include <Engine/Aether/Simulation/ParticleProcessor.h>
#include <Engine/Graphics/CommandBuffer.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Graphics/Vulkan/VulkanBuffer.h>

using namespace Elixir;
using namespace Elixir::Aether;

namespace
{
    class MarkerRuntime final : public Simulation::ParticleProcessorRuntime
    {
      public:
        /**
         * @brief Creates immutable data retained for every recorded copy.
         * @param context Graphics context owning the upload.
         */
        explicit MarkerRuntime(const GraphicsContext* context)
        {
            const glm::vec3 marker{42, 2, 3};
            m_Upload = StagingBuffer::Create(context, sizeof(marker), &marker);
        }
        /**
         * @brief Writes a marker only into the emitter's first particle.
         * @param context Current allocation and recording command buffer.
         */
        bool Record(const Simulation::SParticleProcessingContext& context) override
        {
            if (!m_Upload || context.ParticleCount == 0)
                return false;
            context.Particles->Barrier(
                context.Command,
                EPipelineStage::Transfer,
                EPipelineAccess::TransferWrite
            );
            SBufferCopy region{
                .SrcOffset = 0,
                .DstOffset = context.ParticleOffset * Core::PARTICLE_STATE_CORE_V1_STRIDE,
                .Size = sizeof(glm::vec3),
            };
            context.Command->CopyBuffer(m_Upload, context.Particles, std::span(&region, 1));
            return true;
        }

      private:
        Ref<StagingBuffer> m_Upload;
    };

    class MarkerProcessor final : public Simulation::ParticleProcessor
    {
      public:
        /**
         * @brief Counts runtime creation without coupling processing to a render mode.
         * @param creations Counter retained by the test.
         */
        explicit MarkerProcessor(Ref<uint32_t> creations) : m_Creations(std::move(creations))
        {
        }
        /**
         * @brief Accepts any emitter with a nonempty allocation.
         * @param emitter Compiled emitter.
         */
        bool IsCompatible(const SCompiledEmitter& emitter) const override
        {
            return emitter.MaxParticles > 0;
        }
        /**
         * @brief Creates one runtime for its owning Simulator.
         * @param context Graphics context owning the upload.
         * @param loader Unused loader; the marker stage uses a transfer command.
         */
        Scope<Simulation::ParticleProcessorRuntime> CreateRuntime(
            const GraphicsContext* context,
            const ShaderLoader& loader
        ) const override
        {
            ++*m_Creations;
            return CreateScope<MarkerRuntime>(context);
        }

      private:
        Ref<uint32_t> m_Creations;
    };
}

TEST(ParticleProcessorGpuTest, SupportsAllRenderModesAndIsolatesSimulatorResources)
{
    const auto context = VulkanTestContext::Get().GetGraphicsContext();
    ShaderLoader loader(context);
    const auto creations = CreateRef<uint32_t>(0);
    const auto processor = CreateRef<MarkerProcessor>(creations);
    const auto system = CreateRef<System>("Generic processing");
    for (const auto mode :
         {Core::EParticleRenderMode::Sprite,
          Core::EParticleRenderMode::Mesh,
          Core::EParticleRenderMode::Ribbon,
          Core::EParticleRenderMode::Strand,
          Core::EParticleRenderMode::None})
    {
        auto& emitter = system->AddEmitter(std::to_string(int(mode)), 8, 0);
        emitter.SetRenderMode(mode);
        emitter.SetParticleProcessor(processor);
    }
    TestInstanceRegistry registry;
    const auto instance = registry.CreateRegisteredInstance(system);
    ASSERT_TRUE(instance);
    Aether::Rendering::FrameSubmission submission;
    ASSERT_TRUE(submission.Submit(*instance));
    for (uint32_t simulatorIndex = 0; simulatorIndex < 2; ++simulatorIndex)
    {
        Core::SResourcePoolLimits limits;
        limits.ParticleCapacity = 64;
        Simulation::Simulator simulator(context, &loader, limits);
        simulator.BeginFrame(Timestep(0.1f));
        const auto command = context->GetUploadCommandBuffer();
        command->Begin();
        const auto frame = simulator.Simulate(submission, command);
        ASSERT_TRUE(frame);
        command->End();
        command->Flush();
        EXPECT_EQ(*creations, simulatorIndex + 1);
        const auto buffer = frame->GetResources().front().ParticleStateBuffer;
        auto info = StagingBuffer::CreateBufferInfo(buffer->GetSize(), nullptr);
        info.Usage = EBufferUsage::TransferDst;
        const Ref<StagingBuffer> readback =
            CreateRef<Vulkan::VulkanStagingBuffer>(context, info);
        const auto copy = context->GetUploadCommandBuffer();
        copy->Begin();
        buffer->Barrier(copy, EPipelineStage::Transfer, EPipelineAccess::TransferRead);
        buffer->Copy(copy, readback);
        copy->End();
        copy->Flush();
        const auto* data = static_cast<const glm::vec4*>(readback->Map());
        ASSERT_EQ(frame->GetItems().size(), 5u);
        for (const auto& item : frame->GetItems())
        {
            const auto offset = item.Allocation.Particles.Offset + item.LocalParticleOffset;
            EXPECT_EQ(glm::vec3(data[offset * 6]), glm::vec3(42, 2, 3));
            EXPECT_EQ(data[(offset + 1) * 6], glm::vec4(0));
        }
        readback->Unmap();
    }
}
