#pragma once

#include <Engine/Core/Core.h>

namespace Elixir
{
    /**
	 * Event are currently blocking, meaning when an event occurs, it
	 * immediately gets dispatched and must be dealt with right then an there.
	 * For the future, a better strategy might be to buffer events in an event
	 * bus and process them during the "event" part of the update stage.
     */

#define EVENT_CLASS_TYPE(type)                                                  \
    static EventType GetStaticType() { return EventType::type; }                \
    virtual EventType GetEventType() const override { return GetStaticType(); } \
    virtual const char* GetName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category)                                          \
    virtual int GetCategoryFlags() const override { return category; }

    enum class EventType
    {
        None,
        WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
        AppTick, AppUpdate, AppRender,
        KeyPressed, KeyReleased, KeyTyped,
        MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
    };

    enum EventCategory
    {
        None,
        EventCategoryApplication    = BIT(0),
        EventCategoryWindow         = BIT(1),
        EventCategoryInput          = BIT(2),
        EventCategoryKeyboard       = BIT(3),
        EventCategoryMouse          = BIT(4),
        EventCategoryMouseButton    = BIT(5)
    };

    class ELIXIR_API Event
    {
        friend class EventDispatcher;

      public:
        virtual ~Event() = default;
        bool Handled = false;

        [[nodiscard]] bool IsInCategory(EventCategory category) const
        {
            return GetCategoryFlags() & category;
        }

        [[nodiscard]] virtual std::string ToString() const { return GetName(); }

        [[nodiscard]] virtual EventType GetEventType() const = 0;
        [[nodiscard]] virtual const char* GetName() const = 0;
        [[nodiscard]] virtual int GetCategoryFlags() const = 0;
    };

    class ELIXIR_API EventDispatcher
    {
        template <typename T>
        using EventFn = std::function<bool(T&)>;

      public:
        explicit EventDispatcher(Event& event) : m_Event(event) {}

        template <typename T>
        bool Dispatch(EventFn<T> fn)
        {
            if (m_Event.GetEventType() == T::GetStaticType())
            {
                m_Event.Handled = fn(*(T*)&m_Event);
                return true;
            }

            return false;
        }

      private:
        Event& m_Event;
    };

    inline std::ostream& operator<<(std::ostream& os, const Event& event)
    {
        return os << event.ToString();
    }
}