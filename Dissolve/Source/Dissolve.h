#pragma once

#include <Engine.h>

class Dissolve final : public Elixir::Application
{
public:
    Dissolve();
    ~Dissolve() override = default;

    void OnGUI(Elixir::Timestep frameTime) override;

private:
    Elixir::Scope<Elixir::Executor> m_Executor;
};