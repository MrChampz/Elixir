#pragma once

#include <Engine/GUI/Panel.h>

namespace Elixir::GUI
{
    class ELIXIR_API Overlay final : public Panel
    {
      public:
        LayoutSlot& AddChild(const Ref<Widget>& child);
      protected:
        glm::vec2 ComputeDesiredSize() override;
        void ArrangeChildren(const SRect& allocatedSpace) override;
    };
}