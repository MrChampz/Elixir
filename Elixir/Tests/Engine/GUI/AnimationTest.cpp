#include <gtest/gtest.h>

#include <Engine/GUI/WidgetAnimation.h>
#include <Engine/GUI/Util/Interpolation.h>

using namespace Elixir;
using namespace Elixir::GUI;
using namespace Elixir::GUI::Util;

namespace
{
    class AnimatedLeaf final : public Widget
    {
      public:
        using Widget::CollectDrawCommands;

      protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return { 20.0f, 20.0f }; }

        void BuildDrawCommands(RenderBatch& batch, const int zOrder) override
        {
            batch.AddBrush(GetResolvedAppearance().Background, GetGeometry(), zOrder);
        }
    };

    class AnimatedContainer final : public ContentWidget
    {
      public:
        using Widget::CollectDrawCommands;

      protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override
        {
            return HasContent() ? GetContentSlot()->GetWidget()->Measure(availableSize) : glm::vec2{};
        }

        void LayoutChildren(const SRect& allocatedSpace) override
        {
            if (HasContent())
                GetContentSlot()->GetWidget()->ArrangeChildren(allocatedSpace);
        }
    };
}

TEST(AnimationTest, CurveSamplesLinearKeyframes)
{
    AnimationCurve<float> curve;
    curve.AddKey({ .Time = 0.0f, .Value = 0.0f });
    curve.AddKey({ .Time = 0.05f, .Value = 0.0f });
    curve.AddKey({ .Time = 0.15f, .Value = 1.0f });

    EXPECT_FLOAT_EQ(curve.Sample(0.025f), 0.0f);
    EXPECT_FLOAT_EQ(curve.Sample(0.10f), 0.5f);
    EXPECT_FLOAT_EQ(curve.Sample(1.0f), 1.0f);
}

TEST(AnimationTest, AnimatorAppliesCurveValues)
{
    AnimationCurve<SOutline> curve([](const SOutline& from, const SOutline& to, const float amount)
    {
        return Interpolate(from, to, amount);
    });
    curve.AddKey({ .Time = 0.0f, .Value = { { 0.0f, 0.0f, 0.0f, 0.0f }, 0.0f } });
    curve.AddKey({ .Time = 0.1f, .Value = { { 1.0f, 0.5f, 0.25f, 1.0f }, 2.0f } });

    Animator animator;
    SOutline value{};
    animator.Bind<SOutline>(curve, [&value](const SOutline& next) { value = next; });
    animator.Update(Timestep(0.05f));

    EXPECT_EQ(value.Color, SColor(0.5f, 0.25f, 0.125f, 0.5f));
    EXPECT_FLOAT_EQ(value.Thickness, 1.0f);
}

TEST(AnimationTest, AnimatorCompletionCanBindAnotherAnimation)
{
    AnimationCurve<float> curve;
    curve.AddKey({ .Time = 0.0f, .Value = 0.0f });
    curve.AddKey({ .Time = 0.1f, .Value = 1.0f });

    Animator animator;
    int completed = 0;
    animator.Bind<float>(curve, [](const float&) {}, [&]
    {
        ++completed;
        animator.Bind<float>(curve, [](const float&) {}, [&] { ++completed; });
    });

    animator.Update(Timestep(0.1f));

    EXPECT_EQ(completed, 1);
    EXPECT_TRUE(animator.IsAnimating());

    animator.Update(Timestep(0.1f));

    EXPECT_EQ(completed, 2);
    EXPECT_FALSE(animator.IsAnimating());
}

TEST(AnimationTest, AnimatorCompletionCanStopAnotherAnimation)
{
    AnimationCurve<float> curve;
    curve.AddKey({ .Time = 0.0f, .Value = 0.0f });
    curve.AddKey({ .Time = 0.1f, .Value = 1.0f });

    Animator animator;
    int completed = 0;
    Animator::AnimationId second = 0;
    animator.Bind<float>(curve, [](const float&) {}, [&]
    {
        ++completed;
        animator.Stop(second);
    });
    second = animator.Bind<float>(curve, [](const float&) {}, [&] { ++completed; });

    animator.Update(Timestep(0.1f));

    EXPECT_EQ(completed, 1);
    EXPECT_FALSE(animator.IsAnimating());
}

TEST(AnimationTest, AnimatorCompletionCanStopAllAnimations)
{
    AnimationCurve<float> curve;
    curve.AddKey({ .Time = 0.0f, .Value = 0.0f });
    curve.AddKey({ .Time = 0.1f, .Value = 1.0f });

    Animator animator;
    int completed = 0;
    animator.Bind<float>(curve, [](const float&) {}, [&]
    {
        ++completed;
        animator.StopAll();
    });
    animator.Bind<float>(curve, [](const float&) {}, [&] { ++completed; });

    animator.Update(Timestep(0.1f));

    EXPECT_EQ(completed, 1);
    EXPECT_FALSE(animator.IsAnimating());
}

TEST(AnimationTest, AnimatorIgnoresNestedUpdates)
{
    AnimationCurve<float> curve;
    curve.AddKey({ .Time = 0.0f, .Value = 0.0f });
    curve.AddKey({ .Time = 0.1f, .Value = 1.0f });

    Animator animator;
    float outerValue = 0.0f;
    float nestedValue = 0.0f;
    bool requestedNestedUpdate = false;
    animator.Bind<float>(curve, [&](const float value)
    {
        outerValue = value;
        if (requestedNestedUpdate || value == 0.0f) return;

        requestedNestedUpdate = true;
        animator.Bind<float>(curve, [&](const float nested) { nestedValue = nested; });
        animator.Update(Timestep(0.1f));
    });

    animator.Update(Timestep(0.05f));

    EXPECT_FLOAT_EQ(outerValue, 0.5f);
    EXPECT_FLOAT_EQ(nestedValue, 0.0f);

    animator.Update(Timestep(0.05f));

    EXPECT_FLOAT_EQ(nestedValue, 0.5f);
}

TEST(AnimationTest, WidgetAnimationUpdatesWidgetPropertiesOutsideWidget)
{
    const auto widget = CreateRef<AnimatedLeaf>();
    WidgetAnimation animation(widget);

    AnimationCurve<float> opacity;
    opacity.AddKey({ .Time = 0.0f, .Value = 1.0f });
    opacity.AddKey({ .Time = 0.1f, .Value = 0.0f });
    animation.AddTrack<float>(opacity, [](Widget& target, const float value) { target.SetOpacity(value); });

    AnimationCurve<glm::vec2> offset;
    offset.AddKey({ .Time = 0.0f, .Value = {} });
    offset.AddKey({ .Time = 0.1f, .Value = { 12.0f, 0.0f } });
    animation.AddTrack<glm::vec2>(offset, [](Widget& target, const glm::vec2& value) { target.SetRenderOffset(value); });

    animation.Play();
    widget->Update(Timestep(0.05f));
    EXPECT_FLOAT_EQ(widget->GetOpacity(), 1.0f);

    animation.Update(Timestep(0.05f));
    EXPECT_FLOAT_EQ(widget->GetOpacity(), 0.5f);
    EXPECT_EQ(widget->GetRenderOffset(), glm::vec2(6.0f, 0.0f));
}

TEST(AnimationTest, WidgetAnimationCanAnimateStyleProperties)
{
    const auto widget = CreateRef<AnimatedLeaf>();
    WidgetAnimation animation(widget);

    AnimationCurve<SOutline> outline([](const SOutline& from, const SOutline& to, const float amount)
    {
        return Interpolate(from, to, amount);
    });
    outline.AddKey({ .Time = 0.0f, .Value = { { 0.0f, 0.0f, 0.0f, 1.0f }, 0.0f } });
    outline.AddKey({ .Time = 0.1f, .Value = { { 1.0f, 1.0f, 1.0f, 1.0f }, 2.0f } });
    animation.AddTrack<SOutline>(outline, [](Widget& target, const SOutline& value)
    {
        target.SetOutline(EStyleLayer::Normal, value);
    });

    animation.Play();
    animation.Update(Timestep(0.05f));

    EXPECT_FLOAT_EQ(widget->GetStyle().Normal.Background.Outline.Thickness, 1.0f);
    EXPECT_EQ(widget->GetStyle().Normal.Background.Outline.Color, SColor(0.5f, 0.5f, 0.5f, 1.0f));
}

TEST(AnimationTest, FinishedAnimationCanApplyTheFinalVisibility)
{
    const auto widget = CreateRef<AnimatedLeaf>();
    WidgetAnimation animation(widget);
    AnimationCurve<float> opacity;
    opacity.AddKey({ .Time = 0.0f, .Value = 1.0f });
    opacity.AddKey({ .Time = 0.1f, .Value = 0.0f });
    animation.AddTrack<float>(opacity, [](Widget& target, const float value) { target.SetOpacity(value); });
    animation.OnFinished([widget] { widget->SetVisibility(EVisibility::Collapsed); });

    widget->SetVisibility(EVisibility::HitTestInvisible);
    animation.Play();
    animation.Update(Timestep(0.1f));

    EXPECT_EQ(widget->GetVisibility(), EVisibility::Collapsed);
}

TEST(AnimationTest, ParentOpacityAppliesToChildCommands)
{
    const auto root = CreateRef<AnimatedContainer>();
    const auto child = CreateRef<AnimatedLeaf>();
    SWidgetStyle style;
    style.Normal.Background.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    child->SetStyle(style);
    root->SetContent(child);
    root->ArrangeChildren({ {}, { 100.0f, 100.0f } });
    root->SetOpacity(0.5f);

    RenderBatch batch;
    int zOrder = 0;
    bool rebuilt = false;
    root->CollectDrawCommands(batch, zOrder, rebuilt, {{ -1, -1 }, { -1, -1 }});

    ASSERT_EQ(batch.GetCommands().size(), 1u);
    EXPECT_FLOAT_EQ(batch.GetCommands().front().Color.A, 0.5f);
}
