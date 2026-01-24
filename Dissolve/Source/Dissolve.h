#pragma once

#include <Engine.h>

class Dissolve final : public Elixir::Application
{
public:
    Dissolve();
    ~Dissolve() override;

    void OnGUI(Timestep frameTime) override;
    void OnRender(Timestep frameTime) override;

private:
    void DrawGeometry();

    WaitGroup m_WaitGroup;
    Extent2D m_DrawExtent;
};