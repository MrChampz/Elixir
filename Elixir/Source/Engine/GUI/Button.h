#pragma once

#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    class ELIXIR_API Button : public Widget
    {
      public:
        Button();

      private:
        void GenerateDrawCommands(RenderBatch& batch, int zOrder) override;

        SColor m_NormalColor{0.3f, 0.3f, 0.8f, 1.0f};
    };
}