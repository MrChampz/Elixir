#pragma once

#include <Engine.h>

#include <Engine/Graphics/Model.h>
#include <Engine/Graphics/MeshRenderer.h>
#include <Engine/Graphics/PostProcessor.h>
#include <Engine/Graphics/Material/MaterialGraphEditor.h>
#include <Engine/Graphics/Material/MaterialCompiler.h>

#include <mutex>
#include <optional>

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
    Ref<Shader> m_PendingGraphShader;      // loaded shader, applied on the render thread
    uint32_t m_PendingGraphMaterial = 0;   // material slot the pending shader targets

    // Async graph compilation: DXC runs on a worker thread; the Vulkan shader load
    // and pipeline swap happen on the main/render thread when it's ready.
    bool m_Compiling = false;
    bool m_RecompileQueued = false;        // a change arrived mid-compile; redo after
    MaterialGraph m_CompileGraph;          // snapshot handed to the worker
    uint32_t m_CompileSlot = 0;
    Elixir::EMaterialBlendMode m_CompileBlend = Elixir::EMaterialBlendMode::Opaque;
    Elixir::EMaterialBlendMode m_PendingGraphBlend = Elixir::EMaterialBlendMode::Opaque;
    std::mutex m_CompileMutex;
    bool m_CompileReady = false;
    std::optional<MaterialCompiler::SCompiled> m_CompiledResult;

    void StartGraphCompile();

    // Live exposed-parameter values, snapshotted on the main thread (OnGUI) and
    // pushed to the applied slot's shader on the render thread (OnRender).
    int m_AppliedParamSlot = -1;
    glm::vec4 m_GraphParams[8] = {};
    int m_GraphParamCount = 0;
};