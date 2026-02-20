#pragma once

#include <Engine/GUI/Widget.h>
#include <Engine/GUI/Panel.h>

namespace Elixir::GUI
{
    class ELIXIR_API InputManager
    {
        friend class Manager;
      public:
        void Update();

        void ProcessWidget(const Ref<Widget>& widget);

        void SetRoot(const Ref<Panel>& root) { m_RootWidget = root; }

      private:
        void ProcessInputRecursive(const Ref<Widget>& widget);

        Ref<Panel> m_RootWidget;
        Ref<Widget> m_PressedWidget;

        glm::vec2 m_MousePos{};
        bool m_WasMouseDown = false;
        bool m_MousePressed = false;
        bool m_MouseReleased = false;
    };
}