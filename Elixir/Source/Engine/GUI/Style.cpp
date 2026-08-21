#include "epch.h"
#include "Style.h"

namespace Elixir::GUI
{
    namespace
    {
        constexpr size_t ToIndex(const EStyleLayer layer)
        {
            return static_cast<size_t>(layer);
        }

        void ApplyOverride(SResolvedStyle& destination, const SStyleOverride& override)
        {
            if (override.BackgroundColor)
                destination.BackgroundColor = *override.BackgroundColor;

            if (override.ForegroundColor)
                destination.ForegroundColor = *override.ForegroundColor;

            if (override.BackgroundTexture)
                destination.BackgroundTexture = *override.BackgroundTexture;

            if (override.BackgroundBorders)
                destination.BackgroundBorders = *override.BackgroundBorders;

            if (override.CornerRadius)
                destination.CornerRadius = *override.CornerRadius;

            if (override.Outline)
                destination.Outline = *override.Outline;

            if (override.InsetShadow)
                destination.InsetShadow = *override.InsetShadow;

            if (override.DropShadow)
                destination.DropShadow = *override.DropShadow;
        }
    }

    const SStyleOverride& StyleSet::Get(const EStyleLayer layer) const
    {
        return m_Layers[ToIndex(layer)];
    }

    void StyleSet::Set(const EStyleLayer layer, const SStyleOverride& style)
    {
        m_Layers[ToIndex(layer)] = style;
    }

    void StyleSet::Clear(const EStyleLayer layer)
    {
        m_Layers[ToIndex(layer)] = SStyleOverride{};
    }

    SResolvedStyle StyleSet::Resolve(EInteractionState states) const
    {
        SResolvedStyle result{};

        // Normal has no earlier layer to fall back to, so it must fill every field the
        // caller needs; ApplyOverride still checks each optional; a caller that never set
        // Normal gets a default-constructed SResolvedStyle instead of an assert, since
        // StyleSet has no way to know which fields the widget actually needs.
        ApplyOverride(result, m_Layers[ToIndex(EStyleLayer::Normal)]);

        if (HasState(states, EInteractionState::Hovered))
            ApplyOverride(result, m_Layers[ToIndex(EStyleLayer::Hovered)]);

        if (HasState(states, EInteractionState::Pressed))
            ApplyOverride(result, m_Layers[ToIndex(EStyleLayer::Pressed)]);

        if (HasState(states, EInteractionState::Focused))
            ApplyOverride(result, m_Layers[ToIndex(EStyleLayer::Focused)]);

        if (HasState(states, EInteractionState::Disabled))
            ApplyOverride(result, m_Layers[ToIndex(EStyleLayer::Disabled)]);

        return result;
    }
}
