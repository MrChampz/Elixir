#pragma once

#include <Engine/GUI/Widget.h>

namespace Elixir::GUI
{
    class ELIXIR_API Button : public Widget
    {
      public:
        Button(const std::string& text = "");

        glm::vec2 ComputeDesiredSize() override;
        void GenerateDrawCommands(RenderBatch& batch, int zOrder) override;

      private:
        std::string m_Text;

        SColor m_NormalColor{0.3f, 0.3f, 0.8f, 1.0f};
        SColor m_TextColor{1.0f, 1.0f, 1.0f, 1.0f};
    };
}