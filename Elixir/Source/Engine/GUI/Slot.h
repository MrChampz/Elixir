#pragma once

#include <Engine/Core/Core.h>
#include <Engine/GUI/Types.h>
#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    struct SSlot
    {
        Ref<Widget> Content = nullptr;

        // Only used for Canvas layout
        SAnchor Anchor = SAnchor::TopLeft();
        SConstraint Constraint;

        EHorizontalAlignment HAlignment = EHorizontalAlignment::Center;
        EVerticalAlignment VAlignment = EVerticalAlignment::Center;

        // Only used for Box layouts
        SMargin Margin;

        glm::vec2 MinSize{0, 0};
        glm::vec2 MaxSize{FLT_MAX, FLT_MAX};

        // For proportional layouts (like Flexbox flex property)
        // Work only with Stretch alignment
        float FillRatio = 1.0f;

        float Opacity = 1.0f;

        EVisibility Visibility = EVisibility::Visible;

        SSlot() = default;
        explicit SSlot(const Ref<Widget>& content) : Content(content) {}

        SSlot& SetHorizontalAlignment(const EHorizontalAlignment alignment)
        {
            HAlignment = alignment;
            return *this;
        }

        SSlot& SetVerticalAlignment(const EVerticalAlignment alignment)
        {
            VAlignment = alignment;
            return *this;
        }

        SSlot& SetMargin(const SMargin& margin)
        {
            Margin = margin;
            return *this;
        }

        SSlot& SetMinSize(const glm::vec2& size)
        {
            MinSize = size;
            return *this;
        }

        SSlot& SetMaxSize(const glm::vec2& size)
        {
            MaxSize = size;
            return *this;
        }

        SSlot& SetFillRatio(const float ratio)
        {
            FillRatio = ratio;
            return *this;
        }

        SSlot& SetOpacity(const float opacity)
        {
            Opacity = opacity;
            return *this;
        }

        SSlot& SetAnchor(const SAnchor& anchor)
        {
            Anchor = anchor;
            return *this;
        }

        SSlot& SetPosition(const glm::vec2& pos)
        {
            Constraint.Position = pos;
            return *this;
        }

        SSlot& SetSize(const glm::vec2& size)
        {
            Constraint.Size = size;
            return *this;
        }

        SSlot& SetOffsets(
            const float left,
            const float top,
            const float right,
            const float bottom
        )
        {
            Constraint.Offsets = { left, top, right, bottom };
            return *this;
        }

        SSlot& SetAlignment(const glm::vec2& alignment)
        {
            Constraint.Alignment = alignment;
            return *this;
        }

        SSlot& SetZOrder(const int zOrder)
        {
            Constraint.ZOrder = zOrder;
            return *this;
        }

        bool IsVisible() const
        {
            return Visibility == EVisibility::Visible && Opacity > 0.0f;
        }
    };
}