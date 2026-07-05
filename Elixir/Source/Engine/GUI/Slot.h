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
        void SetWidget(const Ref<Widget>& widget)
        {
            if (m_Widget && m_Widget->m_Parent == m_Owner)
                m_Widget->m_Parent = nullptr;

            m_Widget = widget;

            if (m_Widget && m_Owner)
                m_Widget->m_Parent = m_Owner;

            InvalidateLayout();
        }

        Widget* GetOwner() const { return m_Owner; }

        /**
         * Set the widget that owns this slot. Wires the child's parent back-pointer (for
         * dirty propagation) and invalidates the owner's layout. Containers call this when a
         * slot is added.
         * @param owner the container widget that holds this slot.
         */
        void SetOwner(Widget* owner)
        {
            m_Owner = owner;
            if (m_Widget && m_Owner)
                m_Widget->m_Parent = m_Owner;
            InvalidateLayout();
        }

      protected:
        // Slot metadata (margin, alignment, anchors, ...) feeds the owner's layout, so any
        // change must invalidate the owner. No-op until the slot has an owner.
        void InvalidateLayout() const { if (m_Owner) m_Owner->MarkLayoutDirty(); }

        Ref<Widget> m_Widget = nullptr;
        Widget* m_Owner = nullptr;
    };

    class ELIXIR_API ContentSlot final : public Slot
    {
    public:
        explicit ContentSlot(const Ref<Widget>& widget) { m_Widget = widget; }

        EHorizontalAlignment GetHorizontalAlignment() const { return m_HAlignment; }
        ContentSlot& SetHorizontalAlignment(const EHorizontalAlignment alignment)
        {
            m_HAlignment = alignment;
            InvalidateLayout();
            return *this;
        }

        EVerticalAlignment GetVerticalAlignment() const { return m_VAlignment; }
        ContentSlot& SetVerticalAlignment(const EVerticalAlignment alignment)
        {
            m_VAlignment = alignment;
            InvalidateLayout();
            return *this;
        }

        SMargin GetMargin() const { return m_Margin; }
        ContentSlot& SetMargin(const SMargin& margin)
        {
            m_Margin = margin;
            InvalidateLayout();
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
            InvalidateLayout();
            return *this;
        }

        EVerticalAlignment GetVerticalAlignment() const { return m_VAlignment; }
        LayoutSlot& SetVerticalAlignment(const EVerticalAlignment alignment)
        {
            m_VAlignment = alignment;
            InvalidateLayout();
            return *this;
        }

        SMargin GetMargin() const { return m_Margin; }
        LayoutSlot& SetMargin(const SMargin& margin)
        {
            m_Margin = margin;
            InvalidateLayout();
            return *this;
        }

        glm::vec2 GetMinSize() const { return m_MinSize; }
        LayoutSlot& SetMinSize(const glm::vec2& size)
        {
            m_MinSize = size;
            InvalidateLayout();
            return *this;
        }


        glm::vec2 GetMaxSize() const { return m_MaxSize; }
        LayoutSlot& SetMaxSize(const glm::vec2& size)
        {
            m_MaxSize = size;
            InvalidateLayout();
            return *this;
        }

        float GetFillRatio() const { return m_FillRatio; }
        LayoutSlot& SetFillRatio(const float ratio)
        {
            m_FillRatio = ratio;
            InvalidateLayout();
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