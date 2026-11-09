#include <gtest/gtest.h>
#include "TestInstanceRegistry.h"
#include <Engine/Aether/Simulation/ParticleProcessor.h>

using namespace Elixir;
using namespace Elixir::Aether;

namespace
{
    class TestProcessor final : public Simulation::ParticleProcessor
    {
      public:
        /**
         * @brief Accepts a bounded allocation independently of its rendering policy.
         * @param emitter Compiled emitter to validate.
         */
        bool IsCompatible(const SCompiledEmitter& emitter) const override
        {
            return emitter.MaxParticles == 8;
        }
        /**
         * @brief Leaves GPU allocation to the separate integration test.
         * @param context Unused graphics context.
         * @param loader Unused shader loader.
         */
        Scope<Simulation::ParticleProcessorRuntime> CreateRuntime(
            const GraphicsContext* context,
            const ShaderLoader& loader
        ) const override
        {
            return nullptr;
        }
    };
}

TEST(ParticleProcessorTest, SupportsEveryEmitterRenderModeAndImmutableSnapshots)
{
    const auto processor = CreateRef<TestProcessor>();
    for (const auto mode :
         {Core::EParticleRenderMode::Sprite,
          Core::EParticleRenderMode::Mesh,
          Core::EParticleRenderMode::Ribbon,
          Core::EParticleRenderMode::Strand,
          Core::EParticleRenderMode::None})
    {
        const auto system = CreateRef<System>("Application extension");
        auto& emitter = system->AddEmitter("Emitter", 8, 1);
        emitter.SetRenderMode(mode);
        emitter.SetParticleProcessor(processor);
        TestInstanceRegistry registry;
        const auto instance = registry.CreateRegisteredInstance(system);
        ASSERT_TRUE(instance);
        Aether::Rendering::FrameSubmission submission;
        ASSERT_TRUE(submission.Submit(*instance));
        const auto& compiled = submission.GetRenderProxies().front()->GetCompiledSystem();
        EXPECT_EQ(compiled.Emitters.front().Processor, processor);
        EXPECT_EQ(compiled.Emitters.front().RenderMode, mode);
        emitter.SetParticleProcessor(nullptr);
        EXPECT_EQ(compiled.Emitters.front().Processor, processor);
        EXPECT_EQ(emitter.GetParticleProcessor(), nullptr);
    }
}

TEST(ParticleProcessorTest, RejectsIncompatibleEmitterWithoutPublishingAnInstance)
{
    const auto system = CreateRef<System>("Invalid application extension");
    auto& emitter = system->AddEmitter("Emitter", 7, 1);
    emitter.SetParticleProcessor(CreateRef<TestProcessor>());
    TestInstanceRegistry registry;
    EXPECT_FALSE(registry.CreateRegisteredInstance(system));
}
