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
            m_FakeSize = desired;
        }

        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return m_FakeSize; }

        // MarkLayoutDirty is protected on Widget; promote it so tests can simulate a
        // widget dirtying itself without weakening the production API.
        using Widget::MarkLayoutDirty;

    protected:
        // LayoutChildren is invoked by the non-virtual ArrangeChildren template method only
        // when a re-arrangement is actually needed (dirty, or the allocated space changed),
        // so counting here records exactly how often real layout work happened.
        void LayoutChildren(const SRect& allocatedSpace) override
        {
            ++ArrangeCount;
        }

    private:
        // m_DesiredSize no longer exists on Widget — Measure() now owns the desired-size
        // cache (m_CachedDesiredSize) exclusively, so a test double that wants a fixed fake
        // size keeps its own copy instead.
        glm::vec2 m_FakeSize{};
    };

    // Minimal single-child container to exercise ContentWidget lifecycle
    // (SetContent / ClearContent) without Button's font-system dependency.
    class TestContentWidget final : public ContentWidget
    {
    public:
        glm::vec2 ComputeDesiredSize(const glm::vec2&) override { return { 10.0f, 10.0f }; }
    };

    // ArrangeChildren is the non-virtual template method on Widget; call it directly.
    void Arrange(const Ref<Widget>& widget, const SRect& space)
    {
        widget->ArrangeChildren(space);
    }
}