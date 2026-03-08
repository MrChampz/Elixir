#pragma once

#include <Engine/Core/Platform.h>

namespace Elixir::GLFW
{
    class GLFWPlatform final : public Platform
    {
      public:
        explicit GLFWPlatform(const Window* window);

        void SetPreviousCursorShape() override;

        ECursorShape GetCursorShape() override { return m_Cursor; }
        void SetCursorShape(ECursorShape shape) override;

        std::string GetClipboardText() override;
        void SetClipboardText(const std::string& text) override;

      private:
        ECursorShape m_Cursor = ECursorShape::Arrow;
        ECursorShape m_PrevCursor = m_Cursor;

        const Window* m_Window;
    };
}