#pragma once

#include <Engine.h>
#include <Engine/GUI/WidgetAnimation.h>
#include "EditorStyles.h"

// Base class for a pluggable editor UI panel (Hierarchy, Inspector, Viewport, ...).
// A panel owns a single root widget that EditorUI docks into the editor's content area.
class EditorPanel
{
public:
    virtual ~EditorPanel() = default;

    virtual const char* GetName() const = 0;

    // Build (or rebuild) this panel's widget tree and return its root widget.
    virtual Ref<GUI::Widget> Build() = 0;

    // Called once per frame, after the base Application has ticked its own GUI.
    virtual void OnUpdate(Timestep frameTime) {}

    /** @brief Notifies the panel when its tab becomes active or inactive. */
    virtual void OnActivated(bool active) {}

    /** @brief Hides floating panels temporarily and restores their previous visibility. */
    void SetOverlaysHidden(bool hidden)
    {
        if (m_OverlaysHidden == hidden) return;
        m_OverlaysHidden = hidden;
        OnOverlaysHidden(hidden);
        for (auto& overlay : m_Overlays)
        {
            if (hidden && !overlay.Animation->IsPlaying())
            {
                overlay.Visibility = overlay.Widget->GetVisibility();
            }
            if (overlay.Visibility != GUI::EVisibility::Collapsed)
                AnimateOverlay(overlay.Widget, *overlay.Animation,
                    hidden ? GUI::EVisibility::Collapsed : overlay.Visibility, overlay.Offset);
        }
    }

    /** @brief Advances floating-panel transitions, including those in inactive tabs. */
    virtual void UpdateOverlayAnimations(Timestep frameTime)
    {
        for (auto& overlay : m_Overlays) overlay.Animation->Update(frameTime);
    }

protected:
    // Registers a floating surface that the content drawer can temporarily hide.
    void RegisterOverlay(const Ref<GUI::Widget>& widget, const glm::vec2& offset = {0, -12})
    { m_Overlays.push_back({widget, widget->GetVisibility(), offset, CreateScope<GUI::WidgetAnimation>(widget)}); }

    // Returns whether floating surfaces are temporarily hidden.
    bool AreOverlaysHidden() const { return m_OverlaysHidden; }

    // Lets panels with close controls preserve their requested open states.
    virtual void OnOverlaysHidden(bool hidden) {}

    // Slides and fades a surface from its current state using the shared panel timing.
    static void AnimateOverlay(const Ref<GUI::Widget>& widget, GUI::WidgetAnimation& animation,
        GUI::EVisibility visibility, const glm::vec2& offset)
    {
        const bool open = visibility != GUI::EVisibility::Collapsed;
        const bool collapsed = widget->GetVisibility() == GUI::EVisibility::Collapsed;
        const float opacity = collapsed ? 0.0f : widget->GetOpacity();
        const glm::vec2 origin = collapsed ? offset : widget->GetRenderOffset();
        const auto& styles = EditorStyle::Get();
        widget->SetVisibility(GUI::EVisibility::HitTestInvisible);
        animation.ClearTracks();
        AnimationCurve<float> fade;
        fade.AddKey({.Time = 0, .Value = opacity, .Interpolation = EKeyframeInterpolation::EaseOut});
        fade.AddKey({.Time = styles.PanelTransitionDelay, .Value = opacity, .Interpolation = EKeyframeInterpolation::EaseOut});
        fade.AddKey({.Time = styles.PanelTransitionDelay + styles.PanelTransitionDuration, .Value = open ? 1.0f : 0.0f});
        animation.AddTrack(fade, [](GUI::Widget& target, float value) { target.SetOpacity(value); });
        AnimationCurve<glm::vec2> slide;
        slide.AddKey({.Time = 0, .Value = origin, .Interpolation = EKeyframeInterpolation::EaseOut});
        slide.AddKey({.Time = styles.PanelTransitionDelay, .Value = origin, .Interpolation = EKeyframeInterpolation::EaseOut});
        slide.AddKey({.Time = styles.PanelTransitionDelay + styles.PanelTransitionDuration, .Value = open ? glm::vec2{} : offset});
        animation.AddTrack(slide, [](GUI::Widget& target, const glm::vec2& value) { target.SetRenderOffset(value); });
        animation.OnFinished([widget, visibility] { widget->SetVisibility(visibility); });
        animation.Play();
    }

private:
    struct SOverlay
    {
        Ref<GUI::Widget> Widget;
        GUI::EVisibility Visibility;
        glm::vec2 Offset;
        Scope<GUI::WidgetAnimation> Animation;
    };
    std::vector<SOverlay> m_Overlays;
    bool m_OverlaysHidden = false;
};
