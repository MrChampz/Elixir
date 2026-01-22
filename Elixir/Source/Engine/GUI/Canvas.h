#pragma once

#include <Engine/GUI/Panel.h>

namespace Elixir::GUI
{
    class ELIXIR_API Canvas final : public Panel
    {
      public:
        Canvas();

        glm::vec2 ComputeDesiredSize() override;
        void ArrangeChildren(const SRect& allocatedSpace) override;

      private:
        SRect ComputeChildGeometry(const SSlot& slot, const glm::vec2& canvasSize) const;
    };
}