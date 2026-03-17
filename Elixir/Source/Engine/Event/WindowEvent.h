#pragma once

#include <Engine/Event/Event.h>

namespace Elixir
{
    class ELIXIR_API WindowResizeEvent final : public Event
    {
      public:
        explicit WindowResizeEvent(const Extent2D& extent) : m_Extent(extent) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << GetWidth() << ", " << GetHeight() << ".";
            return ss.str();
        }

        const Extent2D& GetExtent() const { return m_Extent; }
        uint32_t GetWidth() const { return m_Extent.Width; }
        uint32_t GetHeight() const { return m_Extent.Height; }

        EVENT_CLASS_TYPE(WindowResize)
        EVENT_CLASS_CATEGORY(EventCategoryWindow)

      private:
        Extent2D m_Extent;
    };

    class ELIXIR_API FramebufferResizeEvent final : public Event
    {
    public:
        explicit FramebufferResizeEvent(const Extent2D& extent) : m_Extent(extent) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << GetName() << ": " << GetWidth() << ", " << GetHeight() << ".";
            return ss.str();
        }

        const Extent2D& GetExtent() const { return m_Extent; }
        uint32_t GetWidth() const { return m_Extent.Width; }
        uint32_t GetHeight() const { return m_Extent.Height; }

        EVENT_CLASS_TYPE(FramebufferResize)
        EVENT_CLASS_CATEGORY(EventCategoryWindow)

      private:
        Extent2D m_Extent;
    };

    class ELIXIR_API WindowCloseEvent final : public Event
    {
      public:
        WindowCloseEvent() = default;

        EVENT_CLASS_TYPE(WindowClose)
        EVENT_CLASS_CATEGORY(EventCategoryWindow)
    };
}