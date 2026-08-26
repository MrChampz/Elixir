#include "EditorStyles.h"

namespace
{
    const SColor Surface{ 0.118f, 0.122f, 0.133f, 1.0f };
    const SColor SurfaceSunken{ 0.094f, 0.098f, 0.106f, 1.0f };
    const SColor Border{ 0.224f, 0.231f, 0.251f, 1.0f };
    const SColor PanelBorder{ 0.25f, 0.26f, 0.29f, 1.0f };
    const SColor Panel{ 0.071f, 0.075f, 0.082f, 0.9f };
    const SColor Section{ 0.169f, 0.176f, 0.188f, 1.0f };

    SWidgetStyle MakeWidgetStyle(
        const SColor color,
        const SOutline outline = {},
        const float radius = 0.0f
    )
    {
        SWidgetStyle style;
        style.Normal.Background.Color = color;
        style.Normal.Background.Outline = outline;
        style.Normal.Background.CornerRadius = glm::vec4(radius);
        return style;
    }

    SButtonStyle MakeButtonStyle(const SWidgetStyle& widgetStyle)
    {
        SButtonStyle style;
        style.Normal.Background = widgetStyle.Normal.Background;
        style.Hovered = style.Normal;
        style.Pressed = style.Normal;
        style.Focused = style.Normal;
        style.Disabled = style.Normal;
        return style;
    }

    EditorStyle::SChromeStyles Create()
    {
        EditorStyle::SChromeStyles styles;
        styles.TextPrimary = { 0.875f, 0.882f, 0.898f, 1.0f };
        styles.TextSecondary = { 0.616f, 0.627f, 0.659f, 1.0f };
        styles.Accent = { 0.208f, 0.455f, 0.941f, 1.0f };
        styles.AxisX = { 0.86f, 0.23f, 0.29f, 1.0f };
        styles.AxisY = { 0.37f, 0.68f, 0.40f, 1.0f };
        styles.AxisZ = { 0.33f, 0.54f, 0.97f, 1.0f };

        styles.MenuBar = MakeWidgetStyle(Surface);
        styles.MenuBorder = MakeWidgetStyle(Border);
        styles.BranchPill = MakeWidgetStyle(SurfaceSunken, { Border, 1.0f }, 4.0f);
        styles.TabBar = MakeWidgetStyle(Surface);
        styles.TabActiveIndicator = MakeWidgetStyle(styles.Accent);
        styles.AssetBrowser = MakeWidgetStyle(Surface);
        styles.Popup = MakeWidgetStyle({ 0.16f, 0.16f, 0.19f, 1.0f }, { Border, 1.0f }, 4.0f);
        styles.Toolbar = MakeWidgetStyle({ Surface.R, Surface.G, Surface.B, 0.78f }, { PanelBorder, 1.0f }, 8.0f);
        styles.Panel = MakeWidgetStyle(Panel, { PanelBorder, 1.0f }, 8.0f);
        styles.PanelHeaderBorder = MakeWidgetStyle(PanelBorder);
        styles.PanelSection = MakeWidgetStyle(Section);
        styles.InspectorField = MakeWidgetStyle(SurfaceSunken, { Border, 1.0f }, 4.0f);
        styles.Selection = MakeWidgetStyle({ styles.Accent.R, styles.Accent.G, styles.Accent.B, 0.28f }, {}, 4.0f);
        styles.ToolbarButtonActive = MakeWidgetStyle(styles.Accent, {}, 4.0f);
        styles.ToolbarButtonInactive = MakeWidgetStyle({}, {}, 4.0f);
        styles.PlayButtonActive = MakeWidgetStyle({ 0.29f, 0.61f, 0.33f, 1.0f }, {}, 4.0f);
        styles.PlayButtonInactive = MakeWidgetStyle({}, {}, 4.0f);
        styles.PauseButtonActive = MakeWidgetStyle({ 0.35f, 0.36f, 0.40f, 1.0f }, {}, 4.0f);
        styles.PauseButtonInactive = MakeWidgetStyle({}, {}, 4.0f);
        styles.StatsOverlay = MakeWidgetStyle({ Surface.R, Surface.G, Surface.B, 0.55f }, { PanelBorder, 1.0f }, 5.0f);

        styles.PanelHeaderButton = MakeButtonStyle(styles.Transparent);
        styles.PanelToggleButton = MakeButtonStyle(styles.Panel);

        styles.TextField.Normal.Background = styles.InspectorField.Normal.Background;
        styles.TextField.Normal.Foreground = styles.TextPrimary;
        styles.TextField.Hovered = styles.TextField.Normal;
        styles.TextField.Focused = styles.TextField.Normal;
        styles.TextField.Pressed = styles.TextField.Focused;
        styles.TextField.Disabled = styles.TextField.Normal;
        styles.TextField.Disabled->Background.Color.A = 0.5f;
        styles.TextField.Disabled->Foreground.A = 0.5f;
        styles.TransparentTextField = styles.TextField;
        styles.TransparentTextField.Normal.Background = {};
        styles.TransparentTextField.Hovered = styles.TransparentTextField.Normal;
        styles.TransparentTextField.Focused = styles.TransparentTextField.Normal;
        styles.TransparentTextField.Pressed = styles.TransparentTextField.Normal;
        styles.TransparentTextField.Disabled = styles.TransparentTextField.Normal;

        styles.Checkbox.Normal.Background = styles.InspectorField.Normal.Background;
        styles.Checkbox.Normal.Background.CornerRadius = glm::vec4(3.0f);
        styles.Checkbox.Hovered = styles.Checkbox.Normal;
        styles.Checkbox.Pressed = styles.Checkbox.Hovered;
        styles.Checkbox.Focused = styles.Checkbox.Normal;
        styles.Checkbox.Disabled = styles.Checkbox.Normal;
        styles.Checkbox.Checked.Background.Color = styles.Accent;
        styles.Checkbox.Checked.Background.CornerRadius = glm::vec4(3.0f);
        styles.Checkbox.CheckedHovered = styles.Checkbox.Checked;
        styles.Checkbox.CheckedPressed = styles.Checkbox.Checked;
        styles.Checkbox.CheckedFocused = styles.Checkbox.Checked;
        styles.Checkbox.CheckedDisabled = styles.Checkbox.Checked;
        styles.Checkbox.CheckedDisabled->Background.Color.A = 0.5f;

        styles.ScrollBar.Thickness = 11.0f;
        styles.ScrollBar.MinimumThumbLength = 18.0f;
        styles.ScrollBar.Normal.Track.Color = {};
        styles.ScrollBar.Normal.Thumb.Color = { 0.38f, 0.40f, 0.44f, 0.85f };
        styles.ScrollBar.Normal.Thumb.CornerRadius = glm::vec4(6.0f);
        styles.ScrollBar.Normal.Thumb.Outline = { Panel, 2.0f };
        styles.ScrollBar.Hovered = styles.ScrollBar.Normal;
        styles.ScrollBar.Hovered->Thumb.Color = { 0.50f, 0.52f, 0.56f, 0.95f };
        styles.ScrollBar.Pressed = styles.ScrollBar.Hovered;
        styles.ScrollBar.Focused = styles.ScrollBar.Normal;
        styles.ScrollBar.Disabled = styles.ScrollBar.Normal;
        styles.ScrollBar.Disabled->Thumb.Color.A = 0.35f;

        styles.PrimaryIcon.Normal.Foreground = styles.TextPrimary;
        styles.PrimaryIcon.Hovered = styles.PrimaryIcon.Normal;
        styles.PrimaryIcon.Pressed = styles.PrimaryIcon.Normal;
        styles.PrimaryIcon.Focused = styles.PrimaryIcon.Normal;
        styles.PrimaryIcon.Disabled = styles.PrimaryIcon.Normal;
        styles.PrimaryIcon.Disabled->Foreground.A = 0.5f;
        styles.SecondaryIcon = styles.PrimaryIcon;
        styles.SecondaryIcon.Normal.Foreground = styles.TextSecondary;
        styles.SecondaryIcon.Hovered = styles.SecondaryIcon.Normal;
        styles.SecondaryIcon.Pressed = styles.SecondaryIcon.Normal;
        styles.SecondaryIcon.Focused = styles.SecondaryIcon.Normal;
        styles.SecondaryIcon.Disabled = styles.SecondaryIcon.Normal;
        styles.SecondaryIcon.Disabled->Foreground.A = 0.5f;

        return styles;
    }
}

namespace EditorStyle
{
    const SChromeStyles& Get()
    {
        static const SChromeStyles styles = Create();
        return styles;
    }
}
