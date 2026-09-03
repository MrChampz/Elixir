#include <gtest/gtest.h>

#include <Engine/Graphics/FrameSlotState.h>

using namespace Elixir;

TEST(FrameSlotStateTest, TracksDirtyStateIndependentlyPerSlot)
{
    FrameSlotState<uint32_t, 2> state;

    EXPECT_TRUE(state.IsDirty(0));
    EXPECT_TRUE(state.IsDirty(1));

    state.SetActiveFrameSlot(0);
    state.MarkActiveClean();

    EXPECT_FALSE(state.IsDirty(0));
    EXPECT_TRUE(state.IsDirty(1));

    state.SetActiveFrameSlot(1);
    state.MarkActiveClean();
    state.MarkDirty(0);

    EXPECT_TRUE(state.IsDirty(0));
    EXPECT_FALSE(state.IsActiveDirty());
}

TEST(FrameSlotStateTest, KeepsValuesInTheirAssignedSlots)
{
    FrameSlotState<uint32_t, 2> state;

    state.SetActiveFrameSlot(0);
    state.GetActive() = 17;

    state.SetActiveFrameSlot(1);
    state.GetActive() = 29;

    EXPECT_EQ(state.Get(0), 17u);
    EXPECT_EQ(state.GetActive(), 29u);
}

TEST(FrameSlotStateTest, MarksEverySlotDirtyAfterGlobalInvalidation)
{
    FrameSlotState<uint32_t, 3> state;

    state.MarkClean(0);
    state.MarkClean(1);
    state.MarkClean(2);
    state.MarkDirty();

    EXPECT_TRUE(state.IsDirty(0));
    EXPECT_TRUE(state.IsDirty(1));
    EXPECT_TRUE(state.IsDirty(2));
}
