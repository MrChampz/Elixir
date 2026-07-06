#pragma once

#include <Engine/GUI/Widget.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    // Minimal leaf widget that records how many times it actually re-arranged
    // (i.e. how often ArrangeChildren did work instead of short-circuiting).
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

        void ArrangeChildren(const SRect& allocatedSpace) override
        {
            if (!m_LayoutDirty && m_LastArrangedSpace == allocatedSpace)
                return;

            ++ArrangeCount;
            m_Geometry = allocatedSpace;
            m_LastArrangedSpace = allocatedSpace;
            m_LayoutDirty = false;
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

    // Arrange via a Widget reference so the public base overload is used
    // (containers re-declare ArrangeChildren as protected).
    void Arrange(const Ref<Widget>& widget, const SRect& space)
    {
        widget->ArrangeChildren(space);
    }
}