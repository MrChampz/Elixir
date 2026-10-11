#pragma once

#include <Engine.h>

#include "UI/EditorUI.h"
#include "VFX/EditorVFXManager.h"
#include <Engine/Graphics/PostProcessor.h>
#include <Engine/Materials/MaterialSystem.h>
#include <Engine/Camera/ArcBallCameraController.h>
#include <Engine/Mesh/StaticMeshRenderer.h>

class VFXPanel;
class ViewportPanel;

/** @brief Hosts scene tools and the project VFX preview editor. */
class Editor final : public Elixir::Application
{
public:
    /** @brief Builds the Editor UI and its preview panels. */
    Editor(std::filesystem::path asset = {});
    /** @brief Releases editor panels while the engine services are still available. */
    ~Editor() override;

    /** @brief Updates the active editor panel. */
    void OnGUI(Timestep frameTime) override;
    /** @brief Renders document previews into their own targets. */
    void Render(Timestep frameTime) override;

    /** @brief Routes application events and VFX camera input. */
    void OnEvent(Event& event) override;

private:
    // Loads Sponza and sets up its camera and lighting.
    void LoadDefaultScene();
    // Routes orbit, pan and zoom input outside scene controls.
    void ProcessSceneCameraEvent(Event& event, bool inputBlocked);
    Ref<StaticMesh> m_SceneMesh;
    Scope<StaticMeshRenderer> m_SceneRenderer;
    Scope<ArcBallCameraController> m_SceneCamera;
    ViewportPanel* m_ScenePanel = nullptr;
    bool m_SceneOrbitDragging = false, m_ScenePanDragging = false;
    SStaticMeshLighting m_SceneLighting;
    Scope<EditorUI> m_EditorUI;
    // Opens or focuses an asset document without replacing other tab sessions.
    void OpenVFXAsset(const std::filesystem::path& asset);
    struct SDocument
    {
        std::filesystem::path Path;
        Ref<Image> HDRTarget, DisplayTarget, DepthTarget;
        Scope<Materials::MaterialSystem> Materials;
        Scope<PostProcessor> PostProcess;
        Scope<EditorVFXManager> Manager;
        VFXPanel* Panel;
        int Tab;
    };
    // Recreates preview attachments when the viewport dimensions change.
    bool ResizePreview(SDocument& document, const Extent3D& extent);
    std::vector<SDocument> m_Documents;
    std::vector<std::filesystem::path> m_OpenRequests;
    std::vector<int> m_CloseRequests;
};
