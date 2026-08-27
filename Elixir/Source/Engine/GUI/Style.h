#pragma once

#include <Engine/GUI/Definitions.h>
#include <Engine/Graphics/Texture.h>
#include <Engine/Logging/Log.h>

#include <optional>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace Elixir::GUI
{
    /**
     * @brief States supplied by Widget while it handles input.
     *
     * More than one state can be active at once. A style resolves them in this order:
     * Disabled, Pressed, Focused, Hovered, then Normal.
     */
    enum class EInteractionState : uint8_t
    {
        None     = 0,
        Hovered  = 1 << 0,
        Pressed  = 1 << 1,
        Focused  = 1 << 2,
        Disabled = 1 << 3,
    };

    GENERATE_ENUM_CLASS_OPERATORS(EInteractionState)

    /**
     * @brief Legacy names for a single interactive appearance.
     *
     * Use a component style and SetStyle for new code. This enum remains while existing
     * callers move from individual setters to complete, typed styles.
     */
    enum class EStyleLayer : uint8_t
    {
        Normal,
        Hovered,
        Pressed,
        Focused,
        Disabled,
        Count,
    };

    /**
     * @brief Describes how to draw one rectangular surface.
     *
     * A brush can be a solid surface or a tinted nine-patch texture. Its radius, outline,
     * inset shadow and drop shadow apply to solid surfaces. RenderBatch chooses the suitable
     * command from Texture.
     */
    struct SBrush
    {
        SColor Color{};
        Ref<Texture2D> Texture;
        glm::vec4 Borders{};
        glm::vec4 CornerRadius{};
        SOutline Outline{};
        glm::vec4 InsetShadow{};
        glm::vec4 DropShadow{};

    };

    /**
     * @brief Base visual data shared by component appearances.
     *
     * A base widget draws only Background. Components extend this type when they draw more
     * data, such as Button's foreground color.
     */
    struct SAppearance
    {
        SBrush Background;

    };

    /**
     * @brief Stores the appearances a component uses for interaction states.
     *
     * The struct stores complete appearances, not field-level patches. Resolve returns one
     * appearance and never merges fields from separate states.
     *
     * @tparam TAppearance Appearance type owned by the component style.
     */
    template<typename TAppearance>
    struct TStateStyles
    {
        TAppearance Normal;
        std::optional<TAppearance> Hovered;
        std::optional<TAppearance> Pressed;
        std::optional<TAppearance> Focused;
        std::optional<TAppearance> Disabled;

        /**
         * @brief Select the appearance for active interaction states.
         *
         * Disabled wins over Pressed, Focused and Hovered. An absent state uses Normal.
         *
         * @param states Interaction states active on the component.
         * @return The selected complete appearance.
         */
        const TAppearance& Resolve(const EInteractionState states) const
        {
            if (states & EInteractionState::Disabled && Disabled) return *Disabled;
            if (states & EInteractionState::Pressed && Pressed) return *Pressed;
            if (states & EInteractionState::Focused && Focused) return *Focused;
            if (states & EInteractionState::Hovered && Hovered) return *Hovered;
            return Normal;
        }

        /**
         * @brief Get one appearance by its legacy layer name.
         * @param layer Appearance to access.
         * @return The requested complete appearance.
         */
        TAppearance& Get(const EStyleLayer layer)
        {
            switch (layer)
            {
            case EStyleLayer::Hovered: return GetOrCreate(Hovered);
            case EStyleLayer::Pressed: return GetOrCreate(Pressed);
            case EStyleLayer::Focused: return GetOrCreate(Focused);
            case EStyleLayer::Disabled: return GetOrCreate(Disabled);
            default: return Normal;
            }
        }

        /**
         * @brief Get one appearance by its legacy layer name.
         * @param layer Appearance to access.
         * @return The requested complete appearance.
         */
        const TAppearance& Get(const EStyleLayer layer) const
        {
            switch (layer)
            {
            case EStyleLayer::Hovered: return Hovered ? *Hovered : Normal;
            case EStyleLayer::Pressed: return Pressed ? *Pressed : Normal;
            case EStyleLayer::Focused: return Focused ? *Focused : Normal;
            case EStyleLayer::Disabled: return Disabled ? *Disabled : Normal;
            default: return Normal;
            }
        }

    private:
        TAppearance& GetOrCreate(std::optional<TAppearance>& appearance)
        {
            if (!appearance)
                appearance = Normal;

            return *appearance;
        }
    };

    /** @brief Base class required for a style stored by StyleSet. */
    struct SStyle
    {
        virtual ~SStyle() = default;
    };

    /**
     * @brief Owns the complete styles used as the application's defaults.
     *
     * A StyleSet is a typed registry. Widgets copy their registered style when constructed, so
     * replacing an entry affects subsequently constructed widgets. SetStyle replaces a widget's
     * local copy explicitly.
     */
    class ELIXIR_API StyleSet
    {
    public:
        StyleSet() = default;
        StyleSet(const StyleSet&) = delete;
        StyleSet& operator=(const StyleSet&) = delete;
        StyleSet(StyleSet&&) = default;
        StyleSet& operator=(StyleSet&&) = default;

        /**
         * @brief Store the default style for one component type.
         *
         * @tparam TStyle Concrete style type, derived from SStyle.
         * @param style Complete style to store.
         */
        template<typename TStyle>
        void SetWidgetStyle(TStyle style)
        {
            static_assert(std::is_base_of_v<SStyle, TStyle>);
            m_Styles[std::type_index(typeid(TStyle))] = CreateScope<TStyle>(std::move(style));
        }

        /**
         * @brief Get the style registered for one component type.
         * @tparam TStyle Concrete style type to retrieve.
         * @return The registered complete style, or an empty fallback when it is not registered.
         */
        template<typename TStyle>
        const TStyle& GetWidgetStyle() const
        {
            static_assert(std::is_base_of_v<SStyle, TStyle>);
            const auto it = m_Styles.find(std::type_index(typeid(TStyle)));
            if (it == m_Styles.end())
            {
                EE_CORE_ERROR("StyleSet has no style for component type {}", typeid(TStyle).name())
                static const TStyle fallback{};
                return fallback;
            }
            return static_cast<const TStyle&>(*it->second);
        }

    private:
        std::unordered_map<std::type_index, Scope<SStyle>> m_Styles;
    };

    /**
     * @brief Get the application's built-in default styles.
     *
     * The returned registry supplies styles for subsequently constructed widgets. Applications
     * can replace registered styles to update their default look without selecting a named theme.
     * Existing widgets keep their copied styles until SetStyle replaces them.
     *
     * @return The shared default style registry.
     */
    ELIXIR_API StyleSet& GetDefaultStyles();
}
