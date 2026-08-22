#pragma once

#include <Engine/GUI/Button.h>
#include <Engine/GUI/Checkbox.h>
#include <Engine/GUI/Icon.h>
#include <Engine/GUI/ScrollBox.h>
#include <Engine/GUI/TextField.h>

namespace EditorStyle
{
    /** @brief Stores the visual styles used by the Editor chrome. */
    struct SChromeStyles
    {
        SWidgetStyle Transparent;
        SWidgetStyle MenuBar;
        SWidgetStyle MenuBorder;
        SWidgetStyle BranchPill;
        SWidgetStyle TabBar;
        SWidgetStyle TabActiveIndicator;
        SWidgetStyle AssetBrowser;
        SWidgetStyle Popup;
        SWidgetStyle Toolbar;
        SWidgetStyle Panel;
        SWidgetStyle PanelHeaderBorder;
        SWidgetStyle PanelSection;
        SWidgetStyle InspectorField;
        SWidgetStyle Selection;
        SWidgetStyle ToolbarButtonActive;
        SWidgetStyle ToolbarButtonInactive;
        SWidgetStyle PlayButtonActive;
        SWidgetStyle PlayButtonInactive;
        SWidgetStyle PauseButtonActive;
        SWidgetStyle PauseButtonInactive;
        SWidgetStyle StatsOverlay;
        SButtonStyle PanelHeaderButton;
        SButtonStyle PanelToggleButton;
        STextFieldStyle TextField;
        STextFieldStyle TransparentTextField;
        SCheckboxStyle Checkbox;
        SScrollBarStyle ScrollBar;
        SIconStyle PrimaryIcon;
        SIconStyle SecondaryIcon;
        SColor TextPrimary;
        SColor TextSecondary;
        SColor Accent;
        SColor AxisX;
        SColor AxisY;
        SColor AxisZ;
    };

    /** @brief Get the Editor's shared chrome styles. */
    const SChromeStyles& Get();
}
