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

    void RenderBatch::AddRect(const SRect& rect, const SColor& color, const glm::vec4 cornerRadius, const int zOrder)
    {
        SDrawCommand cmd;
        cmd.Type = SDrawCommand::EType::Rect;
        cmd.Geometry = rect;
        cmd.Color = color;
        cmd.CornerRadius = cornerRadius;
        cmd.ZOrder = zOrder;

        m_Commands.push_back(cmd);
    }

    void RenderBatch::AddText(
        const std::string& text,
        const glm::vec2& position,
        const float fontSize,
        const SColor& color,
        const int zOrder
    )
    {
        SDrawCommand cmd;
        cmd.Type = SDrawCommand::EType::Text;
        cmd.Geometry = { position, {0, 0} };
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
        cmd.ZOrder = zOrder;

        m_Commands.push_back(cmd);
    }
}