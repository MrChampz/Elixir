#pragma once

#include <Engine.h>

#include <Engine/Graphics/Model.h>
#include <Engine/Graphics/MeshRenderer.h>
#include <Engine/Graphics/PostProcessor.h>
#include <Engine/Graphics/Material/MaterialAsset.h>
#include <Engine/Graphics/Material/MaterialGraphEditor.h>
#include <Engine/Graphics/Material/MaterialShaderMap.h>

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

    // Async graph compilation: a worker resolves shared SPIR-V through the material
    // shader map, then builds renderer-local Vulkan shader/pipeline resources;
    // MeshRenderer::Render installs the ready variant (no main/render stall).
    bool m_Compiling = false;
    bool m_RecompileQueued = false;        // a change arrived mid-compile; redo after
    std::mutex m_CompileMutex;
    bool m_CompileReady = false;
    std::mutex m_GraphBuildMutex;
    std::mutex m_GraphGenerationMutex;
    std::unordered_map<uint32_t, uint64_t> m_GraphGenerations;

    // Material Editor: the instance name being authored, refreshed when the panel's
    // selected slot changes.
    char m_InstanceName[128] = "";
    int m_InstanceNameSlot = -1;

    // Where a named material/instance asset lives. The two are named independently:
    // one parent material can back several instances.
    static std::filesystem::path MaterialAssetPath(const std::string& name);
    static std::filesystem::path InstanceAssetPath(const std::string& name);

    void StartGraphCompile();
    void QueueGraphCompile(uint32_t slot, const Ref<MaterialInstance>& instance, bool notifyEditor);
    void SaveMaterialAsset();
    void OpenMaterialAsset();
    void SaveInstanceAsset(uint32_t slot);
    void LoadMaterialAssets();
    bool LoadMaterialAsset(uint32_t slot, const std::filesystem::path& instancePath);

};
