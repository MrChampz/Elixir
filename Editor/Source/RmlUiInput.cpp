#include "RmlUiInput.h"

#include <Engine/Input/InputCodes.h>

Rml::Input::KeyIdentifier ConvertToRmlUiKey(const int keyCode)
{
    using namespace Rml::Input;

    if (keyCode >= EE_KEY_0 && keyCode <= EE_KEY_9)
        return static_cast<KeyIdentifier>(KI_0 + keyCode - EE_KEY_0);

    if (keyCode >= EE_KEY_A && keyCode <= EE_KEY_Z)
        return static_cast<KeyIdentifier>(KI_A + keyCode - EE_KEY_A);

    if (keyCode >= EE_KEY_KP_0 && keyCode <= EE_KEY_KP_9)
        return static_cast<KeyIdentifier>(KI_NUMPAD0 + keyCode - EE_KEY_KP_0);

    if (keyCode >= EE_KEY_F1 && keyCode <= EE_KEY_F24)
        return static_cast<KeyIdentifier>(KI_F1 + keyCode - EE_KEY_F1);

    switch (keyCode)
    {
        case EE_KEY_SPACE: return KI_SPACE;
        case EE_KEY_APOSTROPHE: return KI_OEM_7;
        case EE_KEY_COMMA: return KI_OEM_COMMA;
        case EE_KEY_MINUS: return KI_OEM_MINUS;
        case EE_KEY_PERIOD: return KI_OEM_PERIOD;
        case EE_KEY_SLASH: return KI_OEM_2;
        case EE_KEY_SEMICOLON: return KI_OEM_1;
        case EE_KEY_EQUAL: return KI_OEM_PLUS;
        case EE_KEY_LEFT_BRACKET: return KI_OEM_4;
        case EE_KEY_BACKSLASH: return KI_OEM_5;
        case EE_KEY_RIGHT_BRACKET: return KI_OEM_6;
        case EE_KEY_GRAVE_ACCENT: return KI_OEM_3;
        case EE_KEY_ESCAPE: return KI_ESCAPE;
        case EE_KEY_ENTER: return KI_RETURN;
        case EE_KEY_TAB: return KI_TAB;
        case EE_KEY_BACKSPACE: return KI_BACK;
        case EE_KEY_INSERT: return KI_INSERT;
        case EE_KEY_DELETE: return KI_DELETE;
        case EE_KEY_RIGHT: return KI_RIGHT;
        case EE_KEY_LEFT: return KI_LEFT;
        case EE_KEY_DOWN: return KI_DOWN;
        case EE_KEY_UP: return KI_UP;
        case EE_KEY_PAGE_UP: return KI_PRIOR;
        case EE_KEY_PAGE_DOWN: return KI_NEXT;
        case EE_KEY_HOME: return KI_HOME;
        case EE_KEY_END: return KI_END;
        case EE_KEY_CAPS_LOCK: return KI_CAPITAL;
        case EE_KEY_SCROLL_LOCK: return KI_SCROLL;
        case EE_KEY_NUM_LOCK: return KI_NUMLOCK;
        case EE_KEY_PRINT_SCREEN: return KI_SNAPSHOT;
        case EE_KEY_PAUSE: return KI_PAUSE;
        case EE_KEY_KP_DECIMAL: return KI_DECIMAL;
        case EE_KEY_KP_DIVIDE: return KI_DIVIDE;
        case EE_KEY_KP_MULTIPLY: return KI_MULTIPLY;
        case EE_KEY_KP_SUBTRACT: return KI_SUBTRACT;
        case EE_KEY_KP_ADD: return KI_ADD;
        case EE_KEY_KP_ENTER: return KI_NUMPADENTER;
        case EE_KEY_KP_EQUAL: return KI_OEM_NEC_EQUAL;
        case EE_KEY_LEFT_SHIFT: return KI_LSHIFT;
        case EE_KEY_LEFT_CONTROL: return KI_LCONTROL;
        case EE_KEY_LEFT_ALT: return KI_LMENU;
        case EE_KEY_LEFT_SUPER: return KI_LMETA;
        case EE_KEY_RIGHT_SHIFT: return KI_RSHIFT;
        case EE_KEY_RIGHT_CONTROL: return KI_RCONTROL;
        case EE_KEY_RIGHT_ALT: return KI_RMENU;
        case EE_KEY_RIGHT_SUPER: return KI_RMETA;
        case EE_KEY_MENU: return KI_APPS;
        default: return KI_UNKNOWN;
    }
}
