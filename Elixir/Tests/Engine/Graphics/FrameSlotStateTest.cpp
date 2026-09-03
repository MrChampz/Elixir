#include <gtest/gtest.h>

#include <Engine/Graphics/FrameSlotState.h>

namespace Elixir
{
    class FrameSlotStateTestContext final : public GraphicsContext
    {
    public:
        FrameSlotStateTestContext()
            : GraphicsContext(EGraphicsAPI::Vulkan, nullptr) {}

        void SetFrameNumber(const uint32_t frameNumber) { m_FrameNumber = frameNumber; }

        void Init() override {}
        void Shutdown() override {}
        void ProcessEvent(Event&) override {}
        void RenderFrame(std::function<void()>) override {}
        void DrainRenderQueue() override {}
        void SetClearColor(const glm::vec4&) override {}
        void Clear() override {}
        void Resize(Extent2D) override {}
        Ref<CommandBuffer> GetSecondaryCommandBuffer() const override { return nullptr; }
        Ref<CommandBuffer> GetUploadCommandBuffer() const override { return nullptr; }
        void EnqueueSecondaryCommandBuffer(const Ref<CommandBuffer>&) const override {}
        Extent3D GetSwapchainExtent() const override { return {}; }

    private:
        void CreateRenderTargets() override {}
    };

    TEST(FrameSlotStateTest, AppliesEachRevisionOncePerFrameSlot)
    {
        FrameSlotStateTestContext context;
        FrameSlotState<uint32_t, std::string, uint32_t> state(context);
        uint32_t applyCount = 0;

        const auto apply = [&applyCount](uint32_t& resource, const auto changes)
        {
            ASSERT_EQ(changes.size(), 1u);
            resource = changes.front().Value;
            ++applyCount;
        };

        EXPECT_TRUE(state.Set("value", 3));

        context.SetFrameNumber(0);
        state.ApplyPendingState(apply);
        state.ApplyPendingState(apply);
        EXPECT_EQ(state.GetCurrentFrameResource(), 3u);

        context.SetFrameNumber(1);
        state.ApplyPendingState(apply);
        EXPECT_EQ(state.GetCurrentFrameResource(), 3u);
        EXPECT_EQ(applyCount, 2u);

        EXPECT_TRUE(state.Set("value", 8));
        state.ApplyPendingState(apply);
        EXPECT_EQ(state.GetCurrentFrameResource(), 8u);

        context.SetFrameNumber(2);
        state.ApplyPendingState(apply);
        EXPECT_EQ(state.GetCurrentFrameResource(), 8u);
        EXPECT_EQ(applyCount, 4u);
    }

    TEST(FrameSlotStateTest, DoesNotApplyAnUnchangedValue)
    {
        FrameSlotStateTestContext context;
        FrameSlotState<uint32_t, std::string, uint32_t> state(context);
        uint32_t applyCount = 0;

        EXPECT_TRUE(state.Set("value", 3));
        EXPECT_FALSE(state.Set("value", 3));

        state.ApplyPendingState([&applyCount](uint32_t&, const auto changes)
        {
            EXPECT_EQ(changes.size(), 1u);
            ++applyCount;
        });

        state.ApplyPendingState([&applyCount](uint32_t&, const auto)
        {
            ++applyCount;
        });

        EXPECT_EQ(applyCount, 1u);
    }
}
