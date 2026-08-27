#include <gtest/gtest.h>
using namespace testing;

#include <Engine/GUI/Checkbox.h>
#include <Engine/Event/MouseEvent.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    // Checkbox's own promoted surface: HandleMouseDown and HandleClick are protected
    // overrides with no public equivalent, so this test double promotes them the same way
    // ScrollBoxTest.cpp/ForEachChildTest.cpp promote other protected members.
    class TestCheckbox final : public Checkbox
    {
      public:
        using Checkbox::HandleMouseDown;
        using Checkbox::HandleClick;
    };
}

TEST(CheckboxTest, DefaultsToUnchecked)
{
    const auto checkbox = CreateRef<Checkbox>();
    EXPECT_FALSE(checkbox->GetChecked());
}

TEST(CheckboxTest, ClickTogglesAndFiresCallbackExactlyOnce)
{
    const auto checkbox = CreateRef<TestCheckbox>();

    int callCount = 0;
    bool lastValue = false;
    checkbox->OnCheckedChanged([&](const bool checked)
    {
        ++callCount;
        lastValue = checked;
    });

    // Manager only calls HandleClick() after a HandleMouseDown it accepted is followed by a
    // matching HandleMouseUp on the same widget (see Manager::ProcessMouseRelease) - calling
    // it directly here exercises exactly that contract without needing a full Manager/event
    // round trip.
    checkbox->HandleClick();

    EXPECT_TRUE(checkbox->GetChecked());
    EXPECT_EQ(callCount, 1);
    EXPECT_TRUE(lastValue);
}

TEST(CheckboxTest, SecondClickTogglesBackAndFiresAgain)
{
    const auto checkbox = CreateRef<TestCheckbox>();

    int callCount = 0;
    checkbox->OnCheckedChanged([&](bool) { ++callCount; });

    checkbox->HandleClick();
    checkbox->HandleClick();

    EXPECT_FALSE(checkbox->GetChecked());
    EXPECT_EQ(callCount, 2);
}

TEST(CheckboxTest, SetCheckedProgrammaticallyDoesNotFireCallback)
{
    const auto checkbox = CreateRef<Checkbox>();

    int callCount = 0;
    checkbox->OnCheckedChanged([&](bool) { ++callCount; });

    checkbox->SetChecked(true);

    EXPECT_TRUE(checkbox->GetChecked());
    EXPECT_EQ(callCount, 0)
        << "SetChecked is the programmatic sync path - firing the callback here would let "
           "external state that syncs INTO this checkbox echo straight back out again";
}

TEST(CheckboxTest, SetCheckedToSameValueIsANoOp)
{
    const auto checkbox = CreateRef<Checkbox>();

    const uint64_t before = Widget::CurrentDirtyEpoch();
    checkbox->SetChecked(false); // already false
    EXPECT_EQ(Widget::CurrentDirtyEpoch(), before);
}

TEST(CheckboxTest, ToggleAdvancesDirtyEpoch)
{
    const auto checkbox = CreateRef<TestCheckbox>();

    const uint64_t before = Widget::CurrentDirtyEpoch();
    checkbox->HandleClick(); // toggles false -> true, must MarkRenderDirty()
    EXPECT_GT(Widget::CurrentDirtyEpoch(), before);
}

TEST(CheckboxTest, DisabledCheckboxIgnoresMouseDown)
{
    const auto checkbox = CreateRef<TestCheckbox>();
    checkbox->SetEnabled(false);

    const MouseButtonPressedEvent event(0, glm::vec2{ 0.0f, 0.0f });
    const SInputReply reply = checkbox->HandleMouseDown(event);

    EXPECT_FALSE(reply.EventHandled)
        << "a disabled checkbox must never become Manager::m_PressedWidget, or HandleClick "
           "would still run for it on the matching mouse-up";
}

TEST(CheckboxTest, DisabledCheckboxClickDoesNotToggleOrFireCallback)
{
    const auto checkbox = CreateRef<TestCheckbox>();
    checkbox->SetEnabled(false);

    int callCount = 0;
    checkbox->OnCheckedChanged([&](bool) { ++callCount; });

    // Exercises HandleClick()'s own guard directly (see Checkbox.cpp) - covers the case where
    // SetEnabled(false) runs after Manager already latched this widget as m_PressedWidget from
    // an earlier mouse-down, so HandleMouseDown's own gate above never gets a say.
    checkbox->HandleClick();

    EXPECT_FALSE(checkbox->GetChecked());
    EXPECT_EQ(callCount, 0);
}
