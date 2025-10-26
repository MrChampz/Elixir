#pragma once

#include <Engine.h>

class Dissolve final : public Application
{
public:
    Dissolve();
    ~Dissolve() override = default;

    void OnGUI(Timestep frameTime) override;
    void OnRender(Timestep frameTime) override;

private:
    void DrawGeometry(const Ref<CommandBuffer>& cmd);

    Scope<Executor> m_Executor;
};