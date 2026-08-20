#pragma once

#include <Engine/GUI/Manager.h>
using namespace Elixir;
using namespace Elixir::GUI;

namespace
{
    class TestGUIManager : public Manager
    {
    public:
        using Manager::AssembleFrame;
        using Manager::NeedsRebuild;
        using Manager::MarkRebuilt;

        // Focus surface: SetFocusedWidget/ProcessMousePress/HandleKeyPressed are private
        // (Tab/Shift+Tab/Escape are only reachable through HandleKeyPressed; a real mouse
        // press would need InputManager's static polling state, which ProcessMousePress lets
        // a test skip by taking the hit path directly). Promoted the same way
        // AssembleFrame/NeedsRebuild/MarkRebuilt already are above.
        using Manager::SetFocusedWidget;
        using Manager::ProcessMousePress;
        using Manager::HandleKeyPressed;
    };
}