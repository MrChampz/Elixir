#pragma once

#include <Engine.h>

class Dissolve final : public Application
{
public:
    Dissolve();
    ~Dissolve() override;

    void OnGUI(Timestep frameTime) override;
    void OnRender(Timestep frameTime) override;

private:
    void DrawGeometry();

    Scope<Executor> m_Executor;
    WaitGroup m_WaitGroup;

    Extent2D m_DrawExtent;
};