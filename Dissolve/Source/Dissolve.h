#pragma once

#include <Engine.h>

#include <Engine/Graphics/Model.h>
#include <Engine/Graphics/MeshRenderer.h>
#include <Engine/Graphics/PostProcessor.h>
#include <Engine/Graphics/Material/MaterialAsset.h>
#include <Engine/Graphics/Material/MaterialGraphEditor.h>
#include <Engine/Graphics/Material/MaterialCompiler.h>

#include <filesystem>
#include <mutex>
#include <optional>
#include <unordered_map>

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

    // Async graph compilation: a worker thread does DXC + the Vulkan shader/pipeline
    // build; MeshRenderer::Render installs the ready variant (no main/render stall).
    bool m_Compiling = false;
    bool m_RecompileQueued = false;        // a change arrived mid-compile; redo after
    std::mutex m_CompileMutex;
    bool m_CompileReady = false;
    std::mutex m_GraphBuildMutex;
    std::mutex m_GraphGenerationMutex;
    std::unordered_map<uint32_t, uint64_t> m_GraphGenerations;

    void StartGraphCompile();
    void QueueGraphCompile(uint32_t slot, const Ref<MaterialInstance>& instance, bool notifyEditor);
    void SaveMaterialAssets();
    void LoadMaterialAssets();
    bool LoadMaterialAsset(uint32_t slot, const std::filesystem::path& instancePath, bool loadEditor = false);

};
