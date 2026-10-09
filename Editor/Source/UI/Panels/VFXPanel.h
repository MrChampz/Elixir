#pragma once

#include "../EditorPanel.h"
#include "../../VFX/VFXDocument.h"
#include <Engine/Aether/Manager.h>
#include <Engine/Camera/ArcBallCameraController.h>
#include <Engine/GUI/ScrollBox.h>
#include <map>
#include <mutex>
#include <functional>

/** @brief Authors Aether effects using emitter stacks, editable properties, and a live preview. */
class VFXPanel final : public EditorPanel
{
public:
    /** @brief Creates an authoring workspace with an independent preview session. */
    VFXPanel(Aether::Manager& manager, GraphicsContext& context, std::filesystem::path projectRoot,
        float aspectRatio, std::function<void(const std::string&)> statusChanged);
    /** @brief Detaches the preview before the application destroys Aether. */
    ~VFXPanel() override;
    /** @brief Returns the workspace tab name. */
    const char* GetName() const override { return "VFX"; }
    /** @brief Builds the workbench, preview, inspector, and playback timeline. */
    Ref<GUI::Widget> Build() override;
    /** @brief Applies deferred UI edits and compilation requests on the application thread. */
    void OnUpdate(Timestep frameTime) override;
    /** @brief Suspends preview simulation when another tab becomes active. */
    void OnActivated(bool active) override;
    /** @brief Simulates the preview on the rendering thread. */
    void Render(Timestep frameTime, const Ref<Image>& sceneTarget);
    /** @brief Displays the document's tone-mapped preview image. */
    void SetPreviewImage(const Ref<Image>& image);
    /** @brief Returns physical pixel dimensions, or zero width and height before layout. */
    Extent3D GetPreviewExtent() const;
    /** @brief Returns whether this document is currently visible. */
    bool IsActive() const { return m_Active; }
    /** @brief Routes camera input and authoring shortcuts within the active workspace. */
    void ProcessEvent(Event& event, bool popupOpen);
    /** @brief Opens the asset in this document window on the next UI update. */
    void Open(const std::filesystem::path& path) { m_PendingPath = path; }
    /** @brief Returns the document title, including its unsaved marker. */
    std::string GetTitle() const
    {
        return m_Document ? m_Document->Root().String("name") + (m_Document->Dirty() ? " *" : "") : "VFX";
    }

private:
    // Opens a document, optionally discarding its unsaved definition after confirmation.
    void OpenAsset(const std::filesystem::path& path, bool reload = false);
    // Validates and compiles the edited definition while preserving the system identity.
    bool CompilePreview();
    // Activates a replacement runtime instance before removing the current one.
    bool RestartPreview();
    // Rebuilds selectable emitter and module cards.
    void UpdateOverview();
    // Rebuilds properties for the current system, emitter, or module selection.
    void UpdateInspector();
    // Builds editable leaf properties recursively, preserving nested asset fields.
    void AddProperties(const Ref<GUI::VerticalBox>& column, const EditorVFX::Value& value,
        const EditorVFX::Path& path, const std::string& label = "");
    // Commits pending field text as one validated undo transaction.
    bool ApplyDrafts();
    // Schedules authoring changes without destroying widgets during input dispatch.
    void QueueEdit(std::function<void(EditorVFX::Document&)> edit);
    // Updates dirty markers, command availability, and the preview status.
    void UpdateStatus(const std::string& message);
    // Creates a unique emitter name within the current definition.
    std::string UniqueEmitterName(const std::string& base) const;

    Aether::Manager& m_Manager;
    GraphicsContext& m_Context;
    std::filesystem::path m_ProjectRoot;
    std::function<void(const std::string&)> m_StatusChanged;
    Ref<GUI::Widget> m_PreviewInput, m_PreviewSurface, m_InspectorSurface, m_TimelineSurface;
    Ref<GUI::ScrollBox> m_Graph;
    Ref<GUI::Widget> m_WorkbenchToolbar;
    bool m_GraphPanning = false;
    glm::vec2 m_GraphLastPosition{};
    Ref<GUI::VerticalBox> m_Inspector;
    Ref<GUI::HorizontalBox> m_Overview;
    Ref<GUI::VerticalBox> m_Tracks;
    Ref<GUI::TextBlock> m_Status, m_Clock;
    Ref<GUI::Button> m_Playback;
    Ref<GUI::Widget> m_TimeBar, m_Grid;

    std::map<std::filesystem::path, EditorVFX::Document> m_Documents;
    std::map<std::filesystem::path, Ref<Aether::System>> m_LoadedSystems;
    std::map<std::filesystem::path, std::string> m_AppliedDefinitions;
    EditorVFX::Document* m_Document = nullptr;
    std::filesystem::path m_SelectedPath, m_PendingPath;
    std::vector<EditorVFX::Edit> m_Drafts;
    std::function<void(EditorVFX::Document&)> m_EditCommand;
    bool m_SaveRequested = false;
    bool m_RestartRequested = false, m_ReloadRequested = false, m_CompileRequested = false;
    bool m_RebuildSelection = false, m_ConfirmReload = false, m_DiscardReloadRequested = false;
    bool m_AutoCompile = true, m_NeedsCompile = false;
    float m_EditDelay = 0.0f;
    int m_SelectedEmitter = -1, m_SelectedModule = -1;
    std::string m_SelectedPhase;
    std::string m_PalettePhase;
    float m_GraphZoom = 0.85f;
    bool m_OrbitDragging = false, m_PanDragging = false;

    // Render and application threads share only the camera and preview playback state.
    std::mutex m_PreviewMutex;
    ArcBallCameraController m_Camera;
    GUI::SRect m_PreviewRect;
    Ref<Aether::System> m_System;
    Ref<Aether::SystemInstance> m_Instance;
    bool m_Active = false, m_Playing = true, m_Loop = true, m_StepRequested = false;
    bool m_LoopRestart = false;
    float m_Playhead = 0.0f, m_PlaybackSpeed = 1.0f;
    float m_LoopDuration = 5.0f;
};
