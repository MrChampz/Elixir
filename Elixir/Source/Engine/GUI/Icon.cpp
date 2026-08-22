#include "epch.h"
#include "Icon.h"

namespace Elixir::GUI
{
    Icon::Icon()
      : m_Style(GetDefaultStyles().GetWidgetStyle<SIconStyle>())
    {
        SetVisibility(EVisibility::SelfHitTestInvisible);
    }

    void Icon::SetIcon(const Ref<::Elixir::Icon>& icon)
    {
        if (m_Icon == icon) return;
        m_Icon = icon;
        MarkLayoutDirty();
        MarkRenderDirty();
    }

    void Icon::SetSize(const glm::vec2 size)
    {
        if (m_Size == size) return;
        m_Size = size;
        MarkLayoutDirty();
    }

    void Icon::SetStyle(const SIconStyle& style)
    {
        m_Style = style;
        MarkRenderDirty();
    }

    void Icon::SetColor(const EStyleLayer layer, const SColor& color)
    {
        m_Style.Get(layer).Foreground = color;
        MarkRenderDirty();
    }

    glm::vec2 Icon::ComputeDesiredSize(const glm::vec2& availableSize)
    {
        return glm::min(m_Size, availableSize);
    }

    void Icon::BuildDrawCommands(RenderBatch& batch, const int zOrder)
    {
        if (!m_Icon) return;

        const auto iconSize = m_Icon->GetMetrics().Size;
        if (iconSize.x <= 0.0f || iconSize.y <= 0.0f || m_Geometry.Size.x <= 0.0f ||
            m_Geometry.Size.y <= 0.0f)
            return;

        const float scale = glm::min(
            m_Geometry.Size.x / iconSize.x,
            m_Geometry.Size.y / iconSize.y
        );
        const glm::vec2 drawSize = iconSize * scale;
        const SRect drawRect{
            m_Geometry.Position + (m_Geometry.Size - drawSize) * 0.5f,
            drawSize
        };

        const auto texture = m_Icon->GetTexture(drawSize);
        if (!texture) return;

        batch.AddIcon(
            texture,
            drawRect,
            m_Style.Resolve(GetInteractionState()).Foreground,
            zOrder
        );
    }
}
