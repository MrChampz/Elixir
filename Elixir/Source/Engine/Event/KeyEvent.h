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
        KeyPressedEvent(int keyCode, int repeatCount)
            : KeyEvent(keyCode), m_RepeatCount(repeatCount) {}

        [[nodiscard]] std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << GetKeyCode();

            if (m_RepeatCount > 1)
                ss << " (" << GetRepeatCount() << " times).";
            else
                ss << ".";

            return ss.str();
        }

        [[nodiscard]] inline int GetRepeatCount() const { return m_RepeatCount; }

        EVENT_CLASS_TYPE(KeyPressed)

      private:
        int m_RepeatCount;
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