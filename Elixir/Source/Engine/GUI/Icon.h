#pragma once

#include <Engine/Icon/Icon.h>
#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    /** @brief Describes one icon tint. */
    struct SIconAppearance
    {
        SColor Foreground{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    /** @brief Stores icon tints for interaction states. */
    struct SIconStyle final : SStyle, TStateStyles<SIconAppearance>{};

    /**
     * @brief Draws one tinted icon asset.
     *
     * Icon is self-hit-test-invisible and preserves its intrinsic aspect ratio. Use it inside
     * an interactive widget when the icon is a visual label for another control.
     */
    class ELIXIR_API Icon final : public Widget
    {
      public:
        /** @brief Construct an empty icon with the default icon style. */
        Icon();

        /**
         * @brief Set the asset this widget draws.
         * @param icon Imported icon asset, or nullptr to draw nothing.
         */
        void SetIcon(const Ref<::Elixir::Icon>& icon);

        /** @brief Get the asset this widget draws. */
        const Ref<::Elixir::Icon>& GetIcon() const { return m_Icon; }

        /**
         * @brief Set the logical size this icon requests from layout.
         * @param size Requested dimensions in GUI units.
         */
        void SetSize(glm::vec2 size);

        /** @brief Get the logical size this icon requests from layout. */
        glm::vec2 GetSize() const { return m_Size; }

        /**
         * @brief Replace this icon's complete style.
         * @param style Style to copy into this icon.
         */
        void SetStyle(const SIconStyle& style);

        /**
         * @brief Set the tint for one interaction layer.
         * @param layer Interaction layer to change.
         * @param color New tint.
         */
        void SetColor(EStyleLayer layer, const SColor& color);

      protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override;
        void BuildDrawCommands(RenderBatch& batch, int zOrder) override;

      private:
        Ref<::Elixir::Icon> m_Icon;
        SIconStyle m_Style;
        glm::vec2 m_Size{ 16.0f, 16.0f };
    };
}
