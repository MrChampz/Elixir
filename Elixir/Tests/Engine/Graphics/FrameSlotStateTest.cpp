#include <gtest/gtest.h>

#include <Engine/Graphics/FrameSlotPendingState.h>
#include <Engine/Graphics/FrameSlotState.h>
#include <Engine/GUI/Renderer/FrameGeometrySlots.h>

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
        bool EnqueueRenderTask(std::function<void()> task) const override { task(); return true; }
        bool RunRenderTaskAndWait(std::function<void()> task) const override { task(); return true; }
        bool IsRenderThread() const override { return true; }
        bool IsFrameRecording() const override { return false; }
        void WaitForSubmittedFrames() const override {}
        void SetClearColor(const glm::vec4&) override {}
        void Resize(Extent2D) override {}
        Ref<CommandBuffer> GetSecondaryCommandBuffer() const override { return nullptr; }
        Ref<CommandBuffer> GetUploadCommandBuffer() const override { return nullptr; }
        void EnqueueSecondaryCommandBuffer(const Ref<CommandBuffer>&) const override {}
        Extent3D GetSwapchainExtent() const override { return {}; }

    private:
        void ClearImage(const Ref<Image>&) override {}
        void CreateRenderTargets() override {}
    };

    TEST(FrameSlotStateTest, SelectsValuesForTheCurrentFrameSlot)
    {
        FrameSlotStateTestContext context;
        FrameSlotState<uint32_t> state(context);

        context.SetFrameNumber(0);
        state.GetCurrent() = 3;

        context.SetFrameNumber(1);
        state.GetCurrent() = 8;

        context.SetFrameNumber(2);
        EXPECT_EQ(state.GetCurrent(), 3u);

        context.SetFrameNumber(3);
        EXPECT_EQ(state.GetCurrent(), 8u);
    }

    TEST(FrameSlotPendingStateTest, AppliesEachRevisionOncePerFrameSlot)
    {
        FrameSlotStateTestContext context;
        FrameSlotPendingState<uint32_t, std::string, uint32_t> state(context);
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
        EXPECT_EQ(state.GetCurrent(), 3u);

        context.SetFrameNumber(1);
        state.ApplyPendingState(apply);
        EXPECT_EQ(state.GetCurrent(), 3u);
        EXPECT_EQ(applyCount, 2u);

        EXPECT_TRUE(state.Set("value", 8));
        state.ApplyPendingState(apply);
        EXPECT_EQ(state.GetCurrent(), 8u);

        context.SetFrameNumber(2);
        state.ApplyPendingState(apply);
        EXPECT_EQ(state.GetCurrent(), 8u);
        EXPECT_EQ(applyCount, 4u);
    }

    TEST(FrameSlotPendingStateTest, DoesNotApplyAnUnchangedValue)
    {
        FrameSlotStateTestContext context;
        FrameSlotPendingState<uint32_t, std::string, uint32_t> state(context);
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

    TEST(FrameGeometrySlotsTest, UploadsChangedGeometryWithoutOverwritingOtherSlots)
    {
        FrameSlotStateTestContext context;
        GUI::FrameGeometrySlots<std::vector<int>> slots(context);
        std::vector<int> geometry{1, 2, 3};
        int uploads = 0;
        const auto upload = [&](std::vector<int>& buffer)
        {
            buffer = geometry;
            ++uploads;
            return true;
        };

        context.SetFrameNumber(0);
        auto* first = slots.UpdateCurrent(upload);
        ASSERT_NE(first, nullptr);
        context.SetFrameNumber(1);
        auto* second = slots.UpdateCurrent(upload);
        ASSERT_NE(second, nullptr);
        EXPECT_NE(first, second);
        EXPECT_EQ(*first, *second);

        // A visibility change changes both the ranges and the geometry size.
        geometry = {4};
        slots.MarkChanged();
        slots.UpdateCurrent(upload);
        EXPECT_EQ(*first, (std::vector<int>{1, 2, 3}));
        EXPECT_EQ(*second, (std::vector<int>{4}));

        // A static UI still uploads the latest revision when another slot is reused.
        context.SetFrameNumber(2);
        slots.UpdateCurrent(upload);
        EXPECT_EQ(*first, (std::vector<int>{4}));
        slots.UpdateCurrent(upload);
        EXPECT_EQ(uploads, 4);

        geometry.clear();
        slots.MarkChanged();
        slots.UpdateCurrent(upload);
        EXPECT_TRUE(first->empty());
        EXPECT_FALSE(second->empty());
        context.SetFrameNumber(3);
        slots.UpdateCurrent(upload);
        EXPECT_TRUE(second->empty());
    }

    TEST(FrameGeometrySlotsTest, RetriesFailedUploadBeforeMarkingTheSlotCurrent)
    {
        FrameSlotStateTestContext context;
        GUI::FrameGeometrySlots<int> slots(context);
        EXPECT_EQ(slots.UpdateCurrent([](int&) { return false; }), nullptr);
        const auto* value = slots.UpdateCurrent([](int& buffer) { buffer = 42; return true; });
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(*value, 42);
    }
}
