#pragma once

#include <Engine.h>

#include "UI/EditorUI.h"
#include "VFX/EditorVFXManager.h"
#include <Engine/Graphics/PostProcessor.h>
#include <Engine/Materials/MaterialSystem.h>
#include <Engine/Camera/ArcBallCameraController.h>
#include <Engine/Mesh/StaticMeshRenderer.h>

class VFXPanel;
class MeshPanel;
class ViewportPanel;
class StrandsPanel;

/** @brief Hosts scene tools and tabbed VFX and mesh asset viewers. */
class Editor final : public Elixir::Application
{
public:
    /** @brief Builds the Editor UI and its preview panels. */
    Editor(std::filesystem::path asset = {}, bool openStrands = false);
    /** @brief Releases editor panels while the engine services are still available. */
    ~Editor() override;

    /** @brief Updates the active editor panel. */
    void OnGUI(Timestep frameTime) override;
    /** @brief Renders document previews into their own targets. */
    void Render(Timestep frameTime) override;

    /** @brief Routes application events and VFX camera input. */
    void OnEvent(Event& event) override;

private:
    // Creates the persistent strand playground and its scene resources.
    void LoadStrandScene();
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
    // Chooses a document viewer from the asset format.
    void OpenAsset(const std::filesystem::path& asset);
    // Opens or focuses a mesh document with isolated preview resources.
    void OpenMeshAsset(const std::filesystem::path& asset);
    // Returns whether no asset document tab is active.
    bool IsSceneActive() const;
    // Opens or focuses a VFX document without replacing other tab sessions.
    void OpenVFXAsset(const std::filesystem::path& asset);
    struct SPreview
    {
        Ref<Image> HDRTarget, DisplayTarget, DepthTarget;
        Scope<Materials::MaterialSystem> Materials;
        Scope<PostProcessor> PostProcess;
    };
    struct SDocument : SPreview
    {
        std::filesystem::path Path;
        Scope<EditorVFXManager> Manager;
        VFXPanel* Panel;
        int Tab;
    };
    // Recreates preview attachments when the viewport dimensions change.
    bool ResizePreview(SPreview& document, const Extent3D& extent);
    struct SMeshDocument : SPreview
    {
        std::filesystem::path Path;
        Ref<StaticMesh> Mesh;
        Scope<StaticMeshRenderer> Renderer;
        MeshPanel* Panel = nullptr;
        int Tab = -1;
    };
    struct SStrandScene : SPreview
    {
        Scope<EditorVFXManager> Manager;
        Ref<StaticMesh> Mesh;
        Scope<StaticMeshRenderer> Renderer;
        Ref<StaticMesh> Wall;
        Ref<StaticMesh> PowerPoles;
        StrandsPanel* Panel = nullptr;
    };
    SStrandScene m_StrandScene;
    std::vector<SMeshDocument> m_MeshDocuments;
    std::vector<SDocument> m_Documents;
    std::vector<std::filesystem::path> m_OpenRequests;
    std::vector<int> m_CloseRequests;
};
