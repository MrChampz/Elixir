#pragma once

#include <Engine/Core/Core.h>
#include <Engine/GUI/Definitions.h>
#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    class ELIXIR_API Slot
    {
      public:
        virtual ~Slot() = default;

        bool IsVisible() const
        {
            return m_Widget && m_Widget->IsVisible();
        }

        Ref<Widget> GetWidget() const { return m_Widget; }
        void SetWidget(const Ref<Widget>& widget) { m_Widget = widget; }

      protected:
        Ref<Widget> m_Widget = nullptr;
    };

    class ELIXIR_API ContentSlot final : public Slot
    {
    public:
        explicit ContentSlot(const Ref<Widget>& widget) { m_Widget = widget; }

        EHorizontalAlignment GetHorizontalAlignment() const { return m_HAlignment; }
        ContentSlot& SetHorizontalAlignment(const EHorizontalAlignment alignment)
        {
            m_HAlignment = alignment;
            return *this;
        }

        EVerticalAlignment GetVerticalAlignment() const { return m_VAlignment; }
        ContentSlot& SetVerticalAlignment(const EVerticalAlignment alignment)
        {
            m_VAlignment = alignment;
            return *this;
        }

        SMargin GetMargin() const { return m_Margin; }
        ContentSlot& SetMargin(const SMargin& margin)
        {
            m_Margin = margin;
            return *this;
        }

    private:
        EHorizontalAlignment m_HAlignment = EHorizontalAlignment::Center;
        EVerticalAlignment m_VAlignment = EVerticalAlignment::Center;
        SMargin m_Margin;
    };

    class ELIXIR_API LayoutSlot final : public Slot
    {
      public:
        explicit LayoutSlot(const Ref<Widget>& widget) { m_Widget = widget; }

        EHorizontalAlignment GetHorizontalAlignment() const { return m_HAlignment; }
        LayoutSlot& SetHorizontalAlignment(const EHorizontalAlignment alignment)
        {
            m_HAlignment = alignment;
            return *this;
        }

        EVerticalAlignment GetVerticalAlignment() const { return m_VAlignment; }
        LayoutSlot& SetVerticalAlignment(const EVerticalAlignment alignment)
        {
            m_VAlignment = alignment;
            return *this;
        }

        SMargin GetMargin() const { return m_Margin; }
        LayoutSlot& SetMargin(const SMargin& margin)
        {
            m_Margin = margin;
            return *this;
        }

        glm::vec2 GetMinSize() const { return m_MinSize; }
        LayoutSlot& SetMinSize(const glm::vec2& size)
        {
            m_MinSize = size;
            return *this;
        }


        glm::vec2 GetMaxSize() const { return m_MaxSize; }
        LayoutSlot& SetMaxSize(const glm::vec2& size)
        {
            m_MaxSize = size;
            return *this;
        }

        float GetFillRatio() const { return m_FillRatio; }
        LayoutSlot& SetFillRatio(const float ratio)
        {
            m_FillRatio = ratio;
            return *this;
        }

    private:
        EHorizontalAlignment m_HAlignment = EHorizontalAlignment::Center;
        EVerticalAlignment m_VAlignment = EVerticalAlignment::Center;

        SMargin m_Margin;

        glm::vec2 m_MinSize{0, 0};
        glm::vec2 m_MaxSize{FLT_MAX, FLT_MAX};

        // For proportional layouts (like Flexbox flex property)
        // Work only with Stretch alignment
        float m_FillRatio = 1.0f;
    };
}