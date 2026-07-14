#pragma once

#include <Engine.h>

#include <Engine/Graphics/Model.h>
#include <Engine/Graphics/MeshRenderer.h>
#include <Engine/Graphics/PostProcessor.h>
#include <Engine/Graphics/Material/MaterialGraphEditor.h>

struct SFrameData
{
    glm::mat4 ViewProj;
};

class Dissolve final : public Elixir::Application
{
public:
    Dissolve();
    ~Dissolve() override;

    void OnGUI(Timestep frameTime) override;
    void OnRender(Timestep frameTime) override;

    void OnEvent(Event& event) override;

private:
    void DrawGeometry();
    void DrawMaterialEditor();

    WaitGroup m_WaitGroup;
    Extent2D m_DrawExtent;

    SFrameData m_FrameData;
    Ref<UniformBuffer> m_FrameConstantBuffer;

    Scope<ArcBallCameraController> m_CameraController;

    Scope<MeshRenderer> m_MeshRenderer;
    Scope<PostProcessor> m_PostProcessor;
    Ref<Model> m_Model;

    MaterialGraphEditor m_GraphEditor;
    Ref<Shader> m_PendingGraphShader;      // compiled in OnGUI, applied on the render thread
    uint32_t m_PendingGraphMaterial = 0;   // material slot the pending shader targets
};