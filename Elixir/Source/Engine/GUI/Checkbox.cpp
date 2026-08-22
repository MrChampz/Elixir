#include "epch.h"
#include "Checkbox.h"

#include <Engine/Core/Platform.h>

namespace Elixir::GUI
{
    const SAppearance& SCheckboxStyle::Resolve(
        const bool checked,
        const EInteractionState states
    ) const
    {
        if (!checked)
            return TStateStyles::Resolve(states);

        if (states & EInteractionState::Disabled && CheckedDisabled) return *CheckedDisabled;
        if (states & EInteractionState::Pressed && CheckedPressed) return *CheckedPressed;
        if (states & EInteractionState::Focused && CheckedFocused) return *CheckedFocused;
        if (states & EInteractionState::Hovered && CheckedHovered) return *CheckedHovered;
        return Checked;
    }

    Checkbox::Checkbox()
      : m_Style(GetDefaultStyles().GetWidgetStyle<SCheckboxStyle>())
    {
    }

    void Checkbox::SetStyle(const SCheckboxStyle& style)
    {
        m_Style = style;
        MarkLayoutDirty();
    }

    void Checkbox::SetChecked(const bool checked)
    {
        if (m_Checked == checked) return;
        m_Checked = checked;
        MarkRenderDirty();
    }

    void Checkbox::SetSize(const glm::vec2& size)
    {
        if (m_Size == size) return;
        m_Size = size;
        MarkLayoutDirty();
    }

    SColor Checkbox::GetCheckedColor() const
    {
        return m_Style.Checked.Background.Color;
    }

    void Checkbox::SetCheckedColor(const SColor& color)
    {
        m_Style.Checked.Background.Color = color;
        if (m_Style.CheckedHovered) m_Style.CheckedHovered->Background.Color = color;
        if (m_Style.CheckedPressed) m_Style.CheckedPressed->Background.Color = color;
        if (m_Style.CheckedFocused) m_Style.CheckedFocused->Background.Color = color;
        if (m_Style.CheckedDisabled) m_Style.CheckedDisabled->Background.Color = color;
        MarkRenderDirty();
    }

    glm::vec2 Checkbox::ComputeDesiredSize(const glm::vec2& availableSize)
    {
        // Never ask for more than the parent actually offered - same rule Canvas follows.
        return glm::min(m_Size, availableSize);
    }

    void Checkbox::BuildDrawCommands(RenderBatch& batch, const int zOrder)
    {
        const auto& appearance = GetResolvedAppearance();
        batch.AddBrush(appearance.Background, m_Geometry, zOrder);
    }

    const SAppearance& Checkbox::GetResolvedAppearance() const
    {
        return m_Style.Resolve(m_Checked, GetInteractionState());
    }

    SBrush& Checkbox::GetMutableBackgroundBrush(const EStyleLayer layer)
    {
        return m_Style.Get(layer).Background;
    }

    void Checkbox::HandleMouseEnter()
    {
        Widget::HandleMouseEnter();
        if (IsEnabled())
            Platform::Get().SetCursorShape(ECursorShape::Hand);
    }

    void Checkbox::HandleMouseLeave()
    {
        Widget::HandleMouseLeave();

        // Mirrors HandleMouseEnter's own IsEnabled() gate: Platform's "previous cursor" is a
        // single global slot (Platform::SetCursorShape overwrites it on every call), not a
        // per-widget stack. If Enter never called SetCursorShape for this widget (disabled),
        // Leave popping it anyway would restore whatever unrelated shape happened to be the
        // global previous one - not this widget's own.
        if (IsEnabled())
            Platform::Get().SetPreviousCursorShape();
    }

    SInputReply Checkbox::HandleMouseDown(const MouseButtonPressedEvent& event)
    {
        if (!IsEnabled()) return SInputReply::Unhandled();

        m_Pressed = true;
        MarkRenderDirty();
        if (m_OnMouseDownCallback) m_OnMouseDownCallback();
        return SInputReply::HandledAndCaptured();
    }

    void Checkbox::HandleClick()
    {
        // Belt-and-braces: HandleMouseDown already refuses the press while disabled, so
        // Manager never sets this widget as m_PressedWidget in the common case - but
        // SetEnabled(false) can still run in between a real mouse-down and mouse-up on this
        // same widget (Manager latches m_PressedWidget at press time), so this guard is what
        // actually prevents a toggle from that interleaving, not the one above.
        if (!IsEnabled()) return;

        m_Checked = !m_Checked;
        MarkRenderDirty();
        if (m_OnCheckedChangedCallback) m_OnCheckedChangedCallback(m_Checked);

        // Still runs the base OnClick callback too, in case a caller wants both.
        Widget::HandleClick();
    }
}
