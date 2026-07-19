#pragma once

#include <Engine/Core/Core.h>
#include <Engine/GUI/Definitions.h>

namespace Elixir::GUI
{
    class Widget;

    class ELIXIR_API Slot
    {
      public:
        virtual ~Slot() = default;

        bool IsVisible() const;

        Ref<Widget> GetWidget() const { return m_Widget; }

      protected:
        /**
         * Slot metadata (margin, alignment, min/max, fill, ...) feeds the OWNER's layout, so
         * a change must invalidate the owner. We reach it through the child's parent link.
         */
        void InvalidateOwnerLayout() const;

        Ref<Widget> m_Widget = nullptr;
    };

    class ELIXIR_API ContentSlot final : public Slot
    {
    public:
        explicit ContentSlot(const Ref<Widget>& widget) { m_Widget = widget; }

        EHorizontalAlignment GetHorizontalAlignment() const { return m_HAlignment; }
        ContentSlot& SetHorizontalAlignment(EHorizontalAlignment alignment);

        EVerticalAlignment GetVerticalAlignment() const { return m_VAlignment; }
        ContentSlot& SetVerticalAlignment(EVerticalAlignment alignment);

        SMargin GetMargin() const { return m_Margin; }
        ContentSlot& SetMargin(const SMargin& margin);

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
        LayoutSlot& SetHorizontalAlignment(EHorizontalAlignment alignment);

        EVerticalAlignment GetVerticalAlignment() const { return m_VAlignment; }
        LayoutSlot& SetVerticalAlignment(EVerticalAlignment alignment);

        SMargin GetMargin() const { return m_Margin; }
        LayoutSlot& SetMargin(const SMargin& margin);

        glm::vec2 GetMinSize() const { return m_MinSize; }
        LayoutSlot& SetMinSize(const glm::vec2& size);


        glm::vec2 GetMaxSize() const { return m_MaxSize; }
        LayoutSlot& SetMaxSize(const glm::vec2& size);

        float GetFillRatio() const { return m_FillRatio; }
        LayoutSlot& SetFillRatio(float ratio);

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