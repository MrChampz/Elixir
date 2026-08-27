#include "epch.h"
#include "Style.h"

#include <Engine/GUI/Button.h>
#include <Engine/GUI/Checkbox.h>
#include <Engine/GUI/Icon.h>
#include <Engine/GUI/ScrollBox.h>
#include <Engine/GUI/TextField.h>
#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    namespace
    {
        SAppearance MakeCheckboxAppearance(const SColor& color, const SOutline& outline = {})
        {
            SAppearance appearance;
            appearance.Background.Color = color;
            appearance.Background.CornerRadius = glm::vec4{ 3.0f };
            appearance.Background.Outline = outline;
            return appearance;
        }

        StyleSet CreateDefaultStyles()
        {
            StyleSet styles;

            styles.SetWidgetStyle(SWidgetStyle{});

            SScrollBarStyle scrollBar;
            scrollBar.Normal.Track.Color = { 0.0f, 0.0f, 0.0f, 0.15f };
            scrollBar.Normal.Thumb.Color = { 1.0f, 1.0f, 1.0f, 0.35f };
            scrollBar.Normal.Thumb.CornerRadius = glm::vec4{ 4.0f };
            scrollBar.Hovered = scrollBar.Normal;
            scrollBar.Hovered->Thumb.Color.A = 0.55f;
            scrollBar.Pressed = scrollBar.Hovered;
            scrollBar.Focused = scrollBar.Normal;
            scrollBar.Disabled = scrollBar.Normal;
            scrollBar.Disabled->Thumb.Color.A = 0.2f;
            styles.SetWidgetStyle(std::move(scrollBar));

            SIconStyle icon;
            icon.Normal.Foreground = { 0.875f, 0.882f, 0.898f, 1.0f };
            icon.Hovered = icon.Normal;
            icon.Hovered->Foreground = { 1.0f, 1.0f, 1.0f, 1.0f };
            icon.Pressed = icon.Hovered;
            icon.Focused = icon.Normal;
            icon.Disabled = icon.Normal;
            icon.Disabled->Foreground.A = 0.5f;
            styles.SetWidgetStyle(std::move(icon));

            SButtonStyle button;
            button.Normal.Background.Color = { 0.0941f, 0.0941f, 0.1059f, 1.0f };
            button.Normal.Background.CornerRadius = glm::vec4{ 4.0f };
            button.Normal.Background.Borders = glm::vec4{ 30.0f };
            button.Normal.Background.Outline = { { 0.1529f, 0.1529f, 0.1647f, 1.0f }, 0.5f };
            button.Normal.Foreground = { 0.8941f, 0.8941f, 0.9059f, 1.0f };
            button.Hovered = button.Normal;
            button.Hovered->Background.Color = { 0.1529f, 0.1529f, 0.1647f, 1.0f };
            button.Focused = button.Normal;
            button.Focused->Background.Outline = { { 0.6314f, 0.6314f, 0.6667f, 1.0f }, 1.0f };
            button.Pressed = button.Hovered;
            button.Pressed->Background.Outline = button.Focused->Background.Outline;
            button.Disabled = button.Normal;
            button.Disabled->Background.Color.A = 0.5f;
            button.Disabled->Foreground.A = 0.5f;
            styles.SetWidgetStyle(std::move(button));

            STextFieldStyle textField;
            textField.Normal.Background.Color = { 0.0941f, 0.0941f, 0.1059f, 1.0f };
            textField.Normal.Background.CornerRadius = glm::vec4{ 4.0f };
            textField.Normal.Background.Borders = glm::vec4{ 30.0f };
            textField.Normal.Background.Outline = { { 0.1529f, 0.1529f, 0.1647f, 1.0f }, 0.5f };
            textField.Normal.Foreground = { 0.8941f, 0.8941f, 0.9059f, 1.0f };
            textField.Hovered = textField.Normal;
            textField.Focused = textField.Normal;
            textField.Focused->Background.Outline = { { 0.6314f, 0.6314f, 0.6667f, 1.0f }, 1.0f };
            textField.Pressed = textField.Focused;
            textField.Disabled = textField.Normal;
            textField.Disabled->Background.Color.A = 0.5f;
            textField.Disabled->Foreground.A = 0.5f;
            styles.SetWidgetStyle(std::move(textField));

            const SOutline uncheckedOutline{ { 0.224f, 0.231f, 0.251f, 1.0f }, 1.0f };
            SCheckboxStyle checkbox;
            checkbox.Normal = MakeCheckboxAppearance({ 0.094f, 0.098f, 0.106f, 1.0f }, uncheckedOutline);
            checkbox.Hovered = MakeCheckboxAppearance({ 0.145f, 0.149f, 0.161f, 1.0f }, uncheckedOutline);
            checkbox.Pressed = checkbox.Hovered;
            checkbox.Focused = checkbox.Normal;
            checkbox.Disabled = checkbox.Normal;
            checkbox.Disabled->Background.Color.A = 0.5f;
            checkbox.Checked = MakeCheckboxAppearance({ 0.208f, 0.455f, 0.941f, 1.0f });
            checkbox.CheckedHovered = checkbox.Checked;
            checkbox.CheckedHovered->Background.Color = { 0.271f, 0.510f, 0.980f, 1.0f };
            checkbox.CheckedPressed = checkbox.CheckedHovered;
            checkbox.CheckedFocused = checkbox.Checked;
            checkbox.CheckedDisabled = checkbox.Checked;
            checkbox.CheckedDisabled->Background.Color.A = 0.5f;
            styles.SetWidgetStyle(std::move(checkbox));

            return styles;
        }
    }

    StyleSet& GetDefaultStyles()
    {
        static StyleSet styles = CreateDefaultStyles();
        return styles;
    }
}
