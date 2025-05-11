#pragma once

#include <Engine/Event/Event.h>

namespace Elixir
{
    class ELIXIR_API MouseEvent : public Event
    {
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

      protected:
        MouseEvent() = default;
    };

    class ELIXIR_API MouseMovedEvent final : public MouseEvent
    {
      public:
        MouseMovedEvent(const float x, const float y) : m_MouseX(x), m_MouseY(y) {}

        [[nodiscard]] std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << GetX() << ", " << GetY() << ".";
            return ss.str();
        }

        [[nodiscard]] float GetX() const { return m_MouseX; }
        [[nodiscard]] float GetY() const { return m_MouseY; }
        [[nodiscard]] std::pair<float, float> GetPosition() const { return { m_MouseX, m_MouseY }; }

        EVENT_CLASS_TYPE(MouseMoved)

      private:
        float m_MouseX, m_MouseY;
    };

    class ELIXIR_API MouseScrolledEvent final : public MouseEvent
    {
      public:
        MouseScrolledEvent(const float offsetX, const float offsetY)
            : m_OffsetX(offsetX), m_OffsetY(offsetY) {}

        [[nodiscard]] std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << GetOffsetX() << ", " << GetOffsetY() << ".";
            return ss.str();
        }

        [[nodiscard]] float GetOffsetX() const { return m_OffsetX; }
        [[nodiscard]] float GetOffsetY() const { return m_OffsetY; }

        EVENT_CLASS_TYPE(MouseScrolled)

      private:
        float m_OffsetX, m_OffsetY;
    };

    class ELIXIR_API MouseButtonEvent : public MouseEvent
    {
      public:
        [[nodiscard]] std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << GetMouseButton() << ".";
            return ss.str();
        }

        [[nodiscard]] int GetMouseButton() const { return m_Button; }

      protected:
        explicit MouseButtonEvent(const int button) : m_Button(button) {}

        int m_Button;
    };

    class ELIXIR_API MouseButtonPressedEvent final : public MouseButtonEvent
    {
      public:
        explicit MouseButtonPressedEvent(const int button) : MouseButtonEvent(button) {}

        EVENT_CLASS_TYPE(MouseButtonPressed)
    };

    class ELIXIR_API MouseButtonReleasedEvent final : public MouseButtonEvent
    {
      public:
        explicit MouseButtonReleasedEvent(const int button) : MouseButtonEvent(button) {}

        EVENT_CLASS_TYPE(MouseButtonReleased)
    };
}