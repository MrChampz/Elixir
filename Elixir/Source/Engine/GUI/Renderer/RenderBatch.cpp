#include "epch.h"
#include "RenderBatch.h"

namespace Elixir::GUI
{
    void RenderBatch::Sort()
    {
        std::ranges::sort(
            m_Commands,
            [](const SDrawCommand& a, const SDrawCommand& b)
            {
                return a.ZOrder < b.ZOrder;
            }
        );
    }

    void RenderBatch::Clear()
    {
        m_Commands.clear();
    }

    void RenderBatch::AddRect(
        const SRect& rect,
        const SColor& color,
        const glm::vec4 cornerRadius,
        const glm::vec4 insetShadow,
        const glm::vec4 dropShadow,
        const SOutline outline,
        const int zOrder
    )
    {
        SDrawCommand cmd;
        cmd.Type = SDrawCommand::EType::Rect;
        cmd.Geometry = rect;
        cmd.Color = color;
        cmd.Border = cornerRadius;
        cmd.InsetShadow = insetShadow;
        cmd.DropShadow = dropShadow;
        cmd.Outline = outline;
        cmd.ZOrder = zOrder;

        m_Commands.push_back(cmd);
    }

    void RenderBatch::AddText(
        const std::string& text,
        const SRect& rect,
        const float fontSize,
        const SColor& color,
        const int zOrder
    )
    {
        SDrawCommand cmd;
        cmd.Type = SDrawCommand::EType::Text;
        cmd.Geometry = rect;
        cmd.Color = color;
        cmd.Text = text;
        cmd.FontSize = fontSize;
        cmd.ZOrder = zOrder;

        m_Commands.push_back(cmd);
    }

    void RenderBatch::AddTexture(
        const Ref<Texture2D>& texture,
        const SRect& rect,
        const SColor& tint,
        const int zOrder
    )
    {
        SDrawCommand cmd;
        cmd.Type = SDrawCommand::EType::Rect;
        cmd.Geometry = rect;
        cmd.Color = tint;
        cmd.Texture = texture;
        cmd.Border = glm::vec4{ 30.0f, 30.0f, 30.0f, 30.0f }; // TODO: Add to widget properties
        cmd.ZOrder = zOrder;

        m_Commands.push_back(cmd);
    }

    void RenderBatch::AddDebugRect(const SRect& rect, const SColor& color)
    {
        SDrawCommand cmd;
        cmd.Type = SDrawCommand::EType::DebugRect;
        cmd.Geometry = rect;
        cmd.Color = color;

        m_Commands.push_back(cmd);
    }
}