#pragma once

#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    class Manager;

    enum class ELayoutMode : uint8_t
    {
        Horizontal, Vertical, Overlay
    };

    class ELIXIR_API Panel final : public Widget
    {
        friend class Manager;
      public:
        explicit Panel(const ELayoutMode mode = ELayoutMode::Vertical) : m_LayoutMode(mode) {}

        void GenerateDrawCommands(RenderBatch& batch, int zOrder) override;
        glm::vec2 ComputeDesiredSize() override;

        void SetPadding(const float padding) { m_Padding = padding; }
        void SetBackground(const SColor& color) { m_Background = color; }

      protected:
        void ArrangeChildren() const;

      private:
        ELayoutMode m_LayoutMode;
        float m_Padding = 5.0f;
        SColor m_Background;
    };
}