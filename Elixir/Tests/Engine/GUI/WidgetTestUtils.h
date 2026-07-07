#pragma once

#include <Engine/GUI/Widget.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    // Minimal leaf widget that records how many times it actually re-arranged
    // (i.e. how often LayoutChildren ran instead of ArrangeChildren short-circuiting).
    class CountingWidget final : public Widget
    {
    public:
        int ArrangeCount = 0;

        explicit CountingWidget(const glm::vec2& desired = { 10.0f, 10.0f })
        {
            m_DesiredSize = desired;
        }

        glm::vec2 ComputeDesiredSize() override { return m_DesiredSize; }

        void GenerateDrawCommands(RenderBatch& batch, int zOrder) override {}

        // LayoutChildren is invoked by the non-virtual ArrangeChildren template method only
        // when a re-arrangement is actually needed (dirty, or the allocated space changed),
        // so counting here records exactly how often real layout work happened.
        void LayoutChildren(const SRect& allocatedSpace) override
        {
            ++ArrangeCount;
        }
    };

    // Minimal single-child container to exercise ContentWidget lifecycle
    // (SetContent / ClearContent) without Button's font-system dependency.
    class TestContentWidget final : public ContentWidget
    {
    public:
        glm::vec2 ComputeDesiredSize() override { return { 10.0f, 10.0f }; }

        void GenerateDrawCommands(RenderBatch& batch, int zOrder) override {}
    };

    // ArrangeChildren is the non-virtual template method on Widget; call it directly.
    void Arrange(const Ref<Widget>& widget, const SRect& space)
    {
        widget->ArrangeChildren(space);
    }
}