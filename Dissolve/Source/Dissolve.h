#pragma once

#include <Engine.h>

struct SFrameData
{
    glm::mat4 ViewProj;
};

class Dissolve final : public Elixir::Application
{
public:
    Dissolve();
    ~Dissolve() override;

    void OnGUI(Elixir::Timestep frameTime) override;
    void OnRender(Elixir::Timestep frameTime) override;

    void OnEvent(Elixir::Event& event) override;

private:
    void DrawGeometry();

    Elixir::WaitGroup m_WaitGroup;
    Elixir::Extent2D m_DrawExtent;

    SFrameData m_FrameData;
    Elixir::Ref<Elixir::UniformBuffer> m_FrameConstantBuffer;

    Elixir::Scope<Elixir::ArcBallCameraController> m_CameraController;
};
