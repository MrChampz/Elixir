#pragma once

#include <Engine/GUI/Definitions.h>
#include <Engine/Graphics/Texture.h>

namespace Elixir::GUI
{
    /**
     * @brief One editable style layer in a StyleSet.
     *
     * Not a mask: each value names a single layer a caller can set, clear or read. The
     * precedence order these compose in (see StyleSet::Resolve) is Normal < Hovered <
     * Pressed < Focused < Disabled, left to right in this same declaration order - Disabled
     * still wins even over a widget that happens to still be focused while disabled (nothing
     * clears focus just because a widget was disabled).
     */
    enum class EStyleLayer : uint8_t
    {
        Normal,
        Hovered,
        Pressed,
        Focused,
        Disabled,
        Count
    };

    /**
     * @brief Snapshot of which interaction states are active on a widget this frame.
     *
     * A mask, unlike EStyleLayer: Hovered and Pressed can both be set at once. Built fresh
     * every time a widget resolves its style; never stored across frames.
     */
    enum class EInteractionState : uint8_t
    {
        None        = 0,
        Hovered     = 1 << 0,
        Pressed     = 1 << 1,
        Focused     = 1 << 2,
        Disabled    = 1 << 3,
    };

    GENERATE_ENUM_CLASS_OPERATORS(EInteractionState)

    constexpr bool HasState(const EInteractionState states, const EInteractionState state)
    {
        return (states & state) != 0;
    }

    /**
     * @brief Visual properties one layer declares. Every field is optional: an unset field
     * means "inherit whatever the previous active layer resolved to", not "use a zero value".
     *
     * BackgroundTexture uses this same convention with one addition: setting it to a non-null
     * but empty Ref (Ref<Texture2D>{}) explicitly clears a texture inherited from an earlier
     * layer, instead of leaving it unset (which would keep inheriting it).
     */
    struct SStyleOverride
    {
        std::optional<SColor>           BackgroundColor;
        std::optional<SColor>           ForegroundColor;
        std::optional<Ref<Texture2D>>   BackgroundTexture;
        std::optional<glm::vec4>        BackgroundBorders;
        std::optional<glm::vec4>        CornerRadius;
        std::optional<SOutline>         Outline;
        std::optional<glm::vec4>        InsetShadow;
        std::optional<glm::vec4>        DropShadow;
    };

    /**
     * @brief Style ready to draw with: every field has a concrete value, none are optional.
     * This is what BuildDrawCommands consumes - it never inspects SStyleOverride or the
     * interaction state directly.
     */
    struct SResolvedStyle
    {
        SColor           BackgroundColor;
        SColor           ForegroundColor;
        Ref<Texture2D>   BackgroundTexture;
        glm::vec4        BackgroundBorders;
        glm::vec4        CornerRadius;
        SOutline         Outline;
        glm::vec4        InsetShadow;
        glm::vec4        DropShadow;
    };

    /**
     * @brief Holds one SStyleOverride per EStyleLayer and composes them into a
     * SResolvedStyle for a given interaction state.
     *
     * Owns no widget state (hover/press/enabled live on Widget) and triggers no
     * invalidation - callers decide when a resolve is needed and whether to cache it.
     */
    class ELIXIR_API StyleSet
    {
    public:
        /**
         * Read the override currently stored for a layer.
         * @param layer Layer to read.
         * @return The layer's override, as last set (or empty, if never set/cleared).
         */
        const SStyleOverride& Get(EStyleLayer layer) const;

        /**
         * Replace the whole override stored for a layer.
         * @param layer Layer to replace.
         * @param style New override for that layer.
         */
        void Set(EStyleLayer layer, const SStyleOverride& style);

        /**
         * Remove every field a layer declares, so later resolves fall back to earlier layers
         * for all of them again.
         * @param layer Layer to clear.
         */
        void Clear(EStyleLayer layer);

        /**
         * @brief Compose the active layers into one concrete style.
         *
         * Starts from Normal and applies every other active layer on top of it, in
         * Normal -> Hovered -> Pressed -> Focused -> Disabled order; for each field, the last active
         * layer that declares it wins. Normal must declare every field the caller needs -
         * it is the only layer with no earlier layer to fall back to.
         *
         * @param states Interaction states active this frame.
         * @return The composed, ready-to-draw style.
         */
        SResolvedStyle Resolve(EInteractionState states) const;

    private:
        std::array<SStyleOverride, (size_t)EStyleLayer::Count> m_Layers;
    };
}