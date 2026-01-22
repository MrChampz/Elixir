#pragma once

#include <Engine/GUI/Panel.h>

namespace Elixir::GUI
{
    class ELIXIR_API HorizontalBox final : public Panel
    {
    protected:
        glm::vec2 ComputeDesiredSize() override;
        void ArrangeChildren(const SRect& allocatedSpace) override;
    };
}