#pragma once

#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    /**
     * @brief Complete visual style for Checkbox.
     *
     * Checked appearances belong to Checkbox because checked is component data, not a
     * generic interaction state. The checkbox renderer chooses between checked and unchecked
     * appearances before applying the normal interaction-state priority.
     */
    struct SCheckboxStyle final : SStyle, TStateStyles<SAppearance>
    {
        SAppearance Checked;
        std::optional<SAppearance> CheckedHovered;
        std::optional<SAppearance> CheckedPressed;
        std::optional<SAppearance> CheckedFocused;
        std::optional<SAppearance> CheckedDisabled;

        /**
        * @brief Select the appearance for checked state and active interaction states.
        * @param checked Whether the checkbox is checked.
        * @param states Interaction states active on the checkbox.
        * @return The selected complete appearance.
        */
        ELIXIR_API const SAppearance& Resolve(bool checked, EInteractionState states) const;
    };

    /**
     * @brief A small toggle square with checked and unchecked appearances.
     *
     * Checkbox selects checked data from SCheckboxStyle itself. This keeps checked out of the
     * generic widget state model and leaves the component free to define its own rendering.
     */
    class ELIXIR_API Checkbox : public Widget
    {
      public:
        /** @brief Construct a checkbox with a copy of the current default checkbox style. */
        Checkbox();

        /**
         * @brief Replace this checkbox's complete style.
         * @param style Style to copy.
         */
        void SetStyle(const SCheckboxStyle& style);

        bool GetChecked() const { return m_Checked; }

        /**
         * Set the checked state programmatically. Deliberately does NOT invoke
         * OnCheckedChanged - that callback fires only from user clicks (HandleClick).
         * If SetChecked also fired it, any code that syncs this widget FROM an external
         * model (e.g. a callback wired the other way) would immediately echo its own
         * write back into that model.
         * @param checked the new checked state.
         */
        void SetChecked(bool checked);

        /**
         * Register a callback invoked when the user toggles this checkbox by clicking it.
         * Never invoked by SetChecked - see its doc comment.
         * @param callback receives the new checked state.
         */
        void OnCheckedChanged(const std::function<void(bool)>& callback) { m_OnCheckedChangedCallback = callback; }

        const glm::vec2& GetSize() const { return m_Size; }

        /**
         * Set the size this Checkbox asks for, capped to whatever the parent actually
         * offers - same convention Canvas::SetSize uses.
         * @param size the desired size.
         */
        void SetSize(const glm::vec2& size);

        /**
         * @brief Get the normal checked background color.
         * @return Current checked color.
         */
        SColor GetCheckedColor() const;

        /**
         * @brief Set one color for every checked appearance.
         *
         * This compatibility method changes the complete checkbox style. New code should set
         * Checked, CheckedHovered and the other checked appearances directly in SCheckboxStyle.
         * @param color Background color for checked appearances.
         */
        void SetCheckedColor(const SColor& color);

      protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override;
        void BuildDrawCommands(RenderBatch& batch, int zOrder) override;

        void HandleMouseEnter() override;
        void HandleMouseLeave() override;

        // Same override Button uses, and for the same reason: a Checkbox must win the
        // mouse-down bubble even with no OnClick/OnMouseDown/OnMouseUp callback registered,
        // because it drives its own state from HandleClick() directly rather than through
        // those callbacks - Widget::HandleMouseDown's default gate would otherwise return
        // Unhandled() for it.
        SInputReply HandleMouseDown(const MouseButtonPressedEvent& event) override;

        void HandleClick() override;

        const SAppearance& GetResolvedAppearance() const override;
        SBrush& GetMutableBackgroundBrush(EStyleLayer layer) override;

      private:
        bool m_Checked = false;

        // Configured size; ComputeDesiredSize never returns more than this on either axis
        // (capped to availableSize). 13x13 matches the ad-hoc ViewportPanel::MakeCheckbox
        // helper this widget replaces, kept as the default so migrating call sites look
        // identical without an explicit SetSize.
        glm::vec2 m_Size{ 13.0f, 13.0f };

        SCheckboxStyle m_Style;

        std::function<void(bool)> m_OnCheckedChangedCallback;
    };
}
