#pragma once

#include "../EditorPanel.h"

#include <Engine/Aether/Manager.h>
#include <Engine/Camera/ArcBallCameraController.h>

#include <filesystem>
#include <functional>
#include <mutex>
#include <map>

/** @brief Browses project effects and previews one Aether system at a time. */
class VFXPanel final : public EditorPanel
{
public:
    /** @brief Creates a preview using the application's Aether manager and HDR target. */
    VFXPanel(Aether::Manager& manager, GraphicsContext& context, std::filesystem::path projectRoot, float aspectRatio,
        std::function<void(const std::string&)> statusChanged);
    /** @brief Detaches the preview instance before the application destroys Aether. */
    ~VFXPanel() override;

    /** @brief Returns the panel's display name. */
    const char* GetName() const override { return "VFX"; }
    /** @brief Builds the asset list, preview controls, and emitter inspector. */
    Ref<GUI::Widget> Build() override;
    /** @brief Applies asset selection and refresh requests on the application thread. */
    void OnUpdate(Timestep frameTime) override;
    /** @brief Suspends the preview while another editor tab is active. */
    void OnActivated(bool active) override;
    /** @brief Simulates and draws the preview on the rendering thread. */
    void Render(Timestep frameTime, const Ref<Image>& sceneTarget);
    /** @brief Routes camera gestures that start within the preview area. */
    void ProcessEvent(Event& event, bool popupOpen);

private:
    // Rebuilds the effect list from the project root.
    void RefreshAssets();
    // Loads a replacement before detaching the previous preview.
    void OpenAsset(const std::filesystem::path& path, bool reload = false);
    // Creates a fresh runtime instance of the selected authored system.
    bool RestartPreview();
    // Rebuilds the read-only list of authored emitters.
    void UpdateInspector();
    // Repaints the selected asset row.
    void UpdateSelection();
    // Updates the playback control and status message.
    void UpdateStatus(const std::string& message);

    Aether::Manager& m_Manager;
    GraphicsContext& m_Context;
    std::filesystem::path m_ProjectRoot;
    std::function<void(const std::string&)> m_StatusChanged;
    Ref<GUI::Canvas> m_PreviewInput;
    Ref<GUI::VerticalBox> m_AssetList;
    Ref<GUI::VerticalBox> m_EmitterList;
    Ref<GUI::TextBlock> m_AssetCount;
    Ref<GUI::TextBlock> m_EffectName;
    Ref<GUI::TextBlock> m_Status;
    Ref<GUI::Button> m_Playback;
    Ref<GUI::Button> m_Restart;
    Ref<GUI::Button> m_Reload;

    struct SAssetRow
    {
        std::filesystem::path Path;
        Ref<GUI::Widget> Widget;
    };
    std::vector<SAssetRow> m_AssetRows;
    std::filesystem::path m_SelectedPath;
    std::filesystem::path m_PendingPath;
    bool m_RefreshRequested = false;
    bool m_RestartRequested = false;
    bool m_ReloadRequested = false;
    bool m_OrbitDragging = false;
    bool m_PanDragging = false;

    // GUI callbacks and the rendering thread share the camera and preview state.
    std::mutex m_PreviewMutex;
    ArcBallCameraController m_Camera;
    Ref<Aether::System> m_System;
    // Keep one authored system per asset so ordinary selection reuses its GPU compilation.
    std::map<std::filesystem::path, Ref<Aether::System>> m_LoadedSystems;
    Ref<Aether::SystemInstance> m_Instance;
    bool m_Active = false;
    bool m_Playing = true;
};
