#pragma once

#include <Engine/Event/Event.h>

namespace Elixir
{
    class ELIXIR_API WindowResizeEvent : public Event
    {
      public:
        WindowResizeEvent(unsigned int width, unsigned int height)
            : m_Width(width), m_Height(height) {}

        [[nodiscard]] std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << m_Width << ", " << m_Height << ".";
            return ss.str();
        }

        [[nodiscard]] inline unsigned int GetWidth() const { return m_Width; }
        [[nodiscard]] inline unsigned int GetHeight() const { return m_Height; }

        EVENT_CLASS_TYPE(WindowResize)
        EVENT_CLASS_CATEGORY(EventCategoryWindow)

      private:
        unsigned int m_Width, m_Height;
    };

    class ELIXIR_API WindowCloseEvent : public Event
    {
      public:
        WindowCloseEvent() = default;

        EVENT_CLASS_TYPE(WindowClose)
        EVENT_CLASS_CATEGORY(EventCategoryWindow)
    };
}