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

    void RenderBatch::AddRect(const SRect& rect, const SColor& color, const int zOrder)
    {
        SDrawCommand cmd;
        cmd.Type = SDrawCommand::EType::Rect;
        cmd.Geometry = rect;
        cmd.Color = color;
        cmd.ZOrder = zOrder;

        m_Commands.push_back(cmd);
    }
}