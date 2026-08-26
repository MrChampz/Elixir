#pragma once

#include <Engine/GUI/Panel.h>

namespace Elixir::GUI
{
    class ELIXIR_API Overlay final : public TPanel<LayoutSlot>
    {
    protected:
        glm::vec2 ComputeDesiredSize(const glm::vec2& availableSize) override;
        void LayoutChildren(const SRect& allocatedSpace) override;
    };
}