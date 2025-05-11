#pragma once

#include <Engine/Event/Event.h>

namespace Elixir
{
    class ELIXIR_API AppTickEvent : public Event
    {
      public:
        AppTickEvent() = default;

        EVENT_CLASS_TYPE(AppTick)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class ELIXIR_API AppUpdateEvent : public Event
    {
      public:
        AppUpdateEvent() = default;

        EVENT_CLASS_TYPE(AppUpdate)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class ELIXIR_API AppRenderEvent : public Event
    {
      public:
        AppRenderEvent() = default;

        EVENT_CLASS_TYPE(AppRender)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };
}