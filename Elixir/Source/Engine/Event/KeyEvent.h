#pragma once

#include <Engine/Event/Event.h>

namespace Elixir
{
    class ELIXIR_API KeyEvent : public Event
    {
      public:
        [[nodiscard]] std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << GetKeyCode() << ".";
            return ss.str();
        }

        [[nodiscard]] inline int GetKeyCode() const { return m_KeyCode; }

        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)

      protected:
        explicit KeyEvent(int keyCode) : m_KeyCode(keyCode) {}

        int m_KeyCode;
    };

    class ELIXIR_API KeyPressedEvent : public KeyEvent
    {
      public:
        KeyPressedEvent(
            const int keyCode,
            const int repeatCount,
            const bool ctrl,
            const bool alt,
            const bool shift
        ) : KeyEvent(keyCode),
            m_RepeatCount(repeatCount),
            m_CtrlPressed(ctrl),
            m_AltPressed(alt),
            m_ShiftPressed(shift) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << GetKeyCode();

            ss << " [";
            ss << "Ctrl = " << m_CtrlPressed;
            ss << ", Alt = " << m_AltPressed;
            ss << ", Shift = " << m_ShiftPressed;
            ss << "]";

            if (m_RepeatCount > 1)
                ss << " (" << GetRepeatCount() << " times).";
            else
                ss << ".";

            return ss.str();
        }

        int GetRepeatCount() const { return m_RepeatCount; }
        bool IsCtrlPressed() const { return m_CtrlPressed; }
        bool IsAltPressed() const { return m_AltPressed; }
        bool IsShiftPressed() const { return m_ShiftPressed; }

        EVENT_CLASS_TYPE(KeyPressed)

      private:
        int m_RepeatCount;
        bool m_CtrlPressed;
        bool m_AltPressed;
        bool m_ShiftPressed;
    };

    class ELIXIR_API KeyReleasedEvent : public KeyEvent
    {
      public:
        explicit KeyReleasedEvent(int keyCode) : KeyEvent(keyCode) {}

        EVENT_CLASS_TYPE(KeyReleased)
    };

    class ELIXIR_API KeyTypedEvent : public KeyEvent
    {
      public:
        explicit KeyTypedEvent(int keyCode) : KeyEvent(keyCode) {}

        EVENT_CLASS_TYPE(KeyTyped)
    };
}