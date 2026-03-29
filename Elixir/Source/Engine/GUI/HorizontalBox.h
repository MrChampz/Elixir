#pragma once

#include <Engine/GUI/Panel.h>

namespace Elixir::GUI
{
    class ELIXIR_API HorizontalBox final : public Panel
    {
      public:
        LayoutSlot& AddChild(const Ref<Widget>& child);

        bool IsStretching() const { return m_Stretching; }
        void SetStretching(const bool stretching) { m_Stretching = stretching; }

      protected:
        glm::vec2 ComputeDesiredSize() override;
        void ArrangeChildren(const SRect& allocatedSpace) override;

        bool m_Stretching = false;
    };
}