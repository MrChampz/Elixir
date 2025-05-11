#pragma once

#include <Engine/Core/Application.h>

using namespace Elixir;

int main()
{
    Log::Init();

    auto app = CreateApplication();
    app->Run();

    delete app;
}