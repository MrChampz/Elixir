#include "epch.h"
#include "RenderBatch.h"

namespace Elixir::GUI
{
    namespace
    {
        SColor ApplyOpacity(SColor color, const float opacity)
        {
            color.A *= opacity;
            return color;
        }

        void ApplyPresentation(
            SDrawCommand& command,
            const int zOffset,
            const SRect& clipRect,
            const glm::vec2& offset,
            const float opacity
        )
        {
            command.Geometry.Position += offset;
            command.ZOrder += zOffset;
            command.Color = ApplyOpacity(command.Color, opacity);
            command.Outline.Color = ApplyOpacity(command.Outline.Color, opacity);
            command.InsetShadow.w *= opacity;
            command.DropShadow.w *= opacity;

            if (command.ScissorRect.IsValid())
            {
                command.ScissorRect.Position += offset;
                if (clipRect.IsValid())
                    command.ScissorRect = SRect::Intersect(command.ScissorRect, clipRect);
            }
            else if (clipRect.IsValid())
            {
                command.ScissorRect = clipRect;
            }
        }

    }

    void RenderBatch::Append(
        const RenderBatch& other,
        const int zOffset,
        const SRect& clipRect,
        const glm::vec2& offset,
        const float opacity
    )
    {
        const auto sourceCount = other.m_Commands.size();
        const auto maximum = m_Commands.max_size();

        if (sourceCount > maximum - m_Commands.size())
        {
            EE_CORE_ERROR("GUI draw commands exceed the supported batch size.")
            return;
        }

        const auto required = m_Commands.size() + sourceCount;
        const auto capacity = m_Commands.capacity();

        if (required > capacity)
        {
            const auto grown = capacity <= maximum / 2
                ? capacity * 2
                : maximum;
            m_Commands.reserve(std::max(required, grown));
        }

        // Capture the original count so appending a batch to itself remains safe.
        for (size_t i = 0; i < sourceCount; ++i)
        {
            m_Commands.push_back(other.m_Commands[i]);
            auto& cmd = m_Commands.back();
            ApplyPresentation(cmd, zOffset, clipRect, offset, opacity);
        }
    }

    void RenderBatch::Sort()
    {
        std::ranges::stable_sort(
            m_Commands,
            [](const SDrawCommand& a, const SDrawCommand& b)
            {
                const bool aIsDebug = a.Type == EDrawCommandType::DebugRect;
                const bool bIsDebug = b.Type == EDrawCommandType::DebugRect;
                if (aIsDebug != bIsDebug)
                    return !aIsDebug;

                if (a.ZOrder != b.ZOrder)
                    return a.ZOrder < b.ZOrder;

                return a.Type < b.Type;
            }
        );

        BuildRuns();
    }

    void RenderBatch::Clear()
    {
        m_Commands.clear();
        m_Runs.clear();
    }

    int RenderBatch::LayerSpan() const
    {
        int maxZ = -1;
        for (const auto& command : m_Commands)
        {
            if (command.Type != EDrawCommandType::DebugRect)
                maxZ = std::max(maxZ, command.ZOrder);
        }
        return maxZ + 1;
    }

    void RenderBatch::AddBrush(
        const SBrush& brush,
        const SRect& rect,
        const int zOrder,
        const SRect& scissorRect
    )
    {
        if (brush.Texture)
        {
            AddTexture(brush.Texture, rect, brush.Borders, brush.Color, zOrder, scissorRect);
            return;
        }

        AddRect(
            rect,
            brush.Color,
            brush.CornerRadius,
            brush.InsetShadow,
            brush.DropShadow,
            brush.Outline,
            zOrder,
            scissorRect
        );
    }

    void RenderBatch::AddRect(
        const SRect& rect,
        const SColor& color,
        const glm::vec4 cornerRadius,
        const glm::vec4 insetShadow,
        const glm::vec4 dropShadow,
        const SOutline outline,
        const int zOrder,
        const SRect& scissorRect
    )
    {
        SDrawCommand cmd;
        cmd.Type = EDrawCommandType::Rect;
        cmd.Geometry = rect;
        cmd.Color = color;
        cmd.Border = cornerRadius;
        cmd.InsetShadow = insetShadow;
        cmd.DropShadow = dropShadow;
        cmd.Outline = outline;
        cmd.ZOrder = zOrder;
        cmd.ScissorRect = scissorRect;

        m_Commands.push_back(cmd);
    }

    void RenderBatch::AddText(
        const std::string& text,
        const SRect& rect,
        const Ref<Font>& font,
        const float fontSize,
        const SColor& color,
        const int zOrder,
        const SRect& scissorRect
    )
    {
        SDrawCommand cmd;
        cmd.Type = EDrawCommandType::Text;
        cmd.Geometry = rect;
        cmd.Color = color;
        cmd.Text = text;
        cmd.Font = font;
        cmd.FontSize = fontSize;
        cmd.ZOrder = zOrder;
        cmd.ScissorRect = scissorRect;

        m_Commands.push_back(cmd);
    }

    void RenderBatch::AddTexture(
        const Ref<Texture2D>& texture,
        const SRect& rect,
        const glm::vec4& borders,
        const SColor& tint,
        const int zOrder,
        const SRect& scissorRect
    )
    {
        SDrawCommand cmd;
        cmd.Type = EDrawCommandType::Rect;
        cmd.Geometry = rect;
        cmd.Color = tint;
        cmd.Texture = texture;
        cmd.Border = borders;
        cmd.TextureMapping = ETextureMapping::NineSlice;
        cmd.ZOrder = zOrder;
        cmd.ScissorRect = scissorRect;

        m_Commands.push_back(cmd);
    }

    void RenderBatch::AddIcon(
        const Ref<Texture2D>& texture,
        const SRect& rect,
        const SColor& color,
        const int zOrder,
        const SRect& scissorRect
    )
    {
        if (!texture) return;

        SDrawCommand cmd;
        cmd.Type = EDrawCommandType::Rect;
        cmd.Geometry = rect;
        cmd.Color = color;
        cmd.Texture = texture;
        cmd.TextureMapping = ETextureMapping::Stretch;
        cmd.ZOrder = zOrder;
        cmd.ScissorRect = scissorRect;

        m_Commands.push_back(cmd);
    }

    void RenderBatch::AddDebugRect(const SRect& rect, const SColor& color)
    {
        SDrawCommand cmd;
        cmd.Type = EDrawCommandType::DebugRect;
        cmd.Geometry = rect;
        cmd.Color = color;

        m_Commands.push_back(cmd);
    }

    void RenderBatch::BuildRuns()
    {
        m_Runs.clear();

        uint32_t i = 0;
        while (i < m_Commands.size())
        {
            const auto type = m_Commands[i].Type;
            uint32_t count = 1;

            while (i + count < m_Commands.size() && m_Commands[i + count].Type == type)
                ++count;

            m_Runs.push_back({ type, i, count });
            i += count;
        }
    }
}
