#include "epch.h"
#include "Platform.h"

namespace Elixir
{
    Scope<Platform> Platform::s_Platform = nullptr;

    void Platform::Initialize(const Window* window)
    {
        if (!s_Platform)
        {
            s_Platform = Create(window);
            EE_CORE_INFO("Platform layer initialized.")
        }
    }

    void Platform::Shutdown()
    {
        s_Platform.reset();
        EE_CORE_INFO("Platform layer shutdown.")
    }

    Platform& Platform::Get()
    {
        EE_CORE_ASSERT(s_Platform, "Platform layer not initialized!")
        return *s_Platform;
    }

    void Platform::SetDefaultCursorShape()
    {
        SetCursorShape(DEFAULT_CURSOR_SHAPE);
    }
}