#include "epch.h"
#include "Panel.h"

namespace Elixir::GUI
{
    void Panel::GenerateDrawCommands(RenderBatch& batch, const int zOrder)
    {
        for (const auto& slot : m_Slots)
        {
            if (slot.IsVisible())
            {
                slot.Content->GenerateDrawCommands(batch, zOrder + 1);
            }
        }

        if (m_Background.A > 0.0f)
        {
            batch.AddRect(m_Geometry, m_Background, zOrder);
        }
    }

    SSlot& Panel::AddChild(const Ref<Widget>& child, SSlot slot)
    {
        slot.Content = child;
        m_Slots.emplace_back(slot);
        return m_Slots.back();
    }

    SRect Panel::AlignChild(
        const glm::vec2& childSize,
        const SRect& availableSpace,
        const EHorizontalAlignment hAlignment,
        const EVerticalAlignment vAlignment,
        const SMargin& margin
    )
    {
        SRect result = ApplyMargin(availableSpace, margin);
        result = AlignHorizontally(childSize, result, hAlignment);
        result = AlignVertically(childSize, result, vAlignment);

        return result;
    }

    SRect Panel::ApplyMargin(const SRect& availableSpace, const SMargin& margin)
    {
        SRect result;
        result.Position.x = availableSpace.Position.x + margin.Left;
        result.Position.y = availableSpace.Position.y + margin.Top;
        result.Size.x = availableSpace.Size.x - margin.GetTotalHorizontal();
        result.Size.y = availableSpace.Size.y - margin.GetTotalVertical();

        return result;
    }

    SRect Panel::AlignHorizontally(
        const glm::vec2& childSize,
        const SRect& availableSpace,
        const EHorizontalAlignment alignment
    )
    {
        SRect result = availableSpace;

        switch (alignment)
        {
            case EHorizontalAlignment::Left:
                result.Position.x = availableSpace.Position.x;
                result.Size.x = childSize.x;
                break;
            case EHorizontalAlignment::Center:
                result.Position.x = availableSpace.Position.x + (availableSpace.Size.x - childSize.x) * 0.5f;
                result.Size.x = childSize.x;
                break;
            case EHorizontalAlignment::Right:
                result.Position.x = availableSpace.Position.x + availableSpace.Size.x - childSize.x;
                result.Size.x = childSize.x;
                break;
            case EHorizontalAlignment::Stretch:
                result.Position.x = availableSpace.Position.x;
                result.Size.x = availableSpace.Size.x;
                break;
        }

        return result;
    }

    SRect Panel::AlignVertically(
        const glm::vec2& childSize,
        const SRect& availableSpace,
        const EVerticalAlignment alignment
    )
    {
        SRect result = availableSpace;

        switch (alignment)
        {
            case EVerticalAlignment::Top:
                result.Position.y = availableSpace.Position.y;
                result.Size.y = childSize.y;
                break;
            case EVerticalAlignment::Center:
                result.Position.y = availableSpace.Position.y + (availableSpace.Size.y - childSize.y) * 0.5f;
                result.Size.y = childSize.y;
                break;
            case EVerticalAlignment::Bottom:
                result.Position.y = availableSpace.Position.y + availableSpace.Size.y - childSize.y;
                result.Size.y = childSize.y;
                break;
            case EVerticalAlignment::Stretch:
                result.Position.y = availableSpace.Position.y;
                result.Size.y = availableSpace.Size.y;
                break;
        }

        return result;
    }
}