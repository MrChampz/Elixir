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
    };
}