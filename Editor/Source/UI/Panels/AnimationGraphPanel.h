#pragma once

#include "../EditorPanel.h"
#include "../../Animation/AnimationGraphDocument.h"
#include <Engine/GUI/TextField.h>

class AnimationGraphCanvas;
class AnimationPosePreview;

/** Authors an animation graph through asset reference nodes and runtime previews. */
class AnimationGraphPanel final : public EditorPanel
{
  public:
    /** @brief Creates a graph workspace with built-in sample animation sources. */
    AnimationGraphPanel();
    /**
     * @brief Opens an authored machine with application-provided animation sources.
     * @param document State definitions, parameters, and canvas positions to edit.
     * @param sources Named runtime nodes used by source selection and persistence.
     */
    AnimationGraphPanel(
        AnimationGraphDocument document,
        AnimationGraphDocument::Sources sources
    );
    /** @brief Gets the document tab title. */
    const char* GetName() const override { return "Animation Graph"; }
    /** @brief Builds the full workspace and floating preview, tools, and properties. */
    Ref<GUI::Widget> Build() override;
    /**
     * @brief Applies deferred UI changes and advances the compiled preview.
     * @param frameTime Elapsed time in seconds.
     */
    void OnUpdate(Timestep frameTime) override;

  private:
    /** @brief Rebuilds properties of the selected graph reference. */
    void RefreshInspector();
    /** @brief Rebuilds document settings and graph parameter defaults. */
    void BuildDefaultsInspector();
    /** @brief Rebuilds temporary runtime inputs without editing authored defaults. */
    void RefreshPreviewInputs();
    /**
     * @brief Adds a typed graph parameter.
     * @param boolean True creates a bool; false creates a float.
     */
    void AddParameter(bool boolean);
    /**
     * @brief Pauses the preview and seeks its source.
     * @param seconds Target source time in seconds.
     */
    void SeekPreview(float seconds);
    /** @brief Records an edit and invalidates the runtime preview. */
    void Changed();
    /** @brief Compiles the document and creates an independent preview. */
    void Compile();
    /**
     * @brief Saves or loads the document at its configured path.
     * @param save True to save; false to load.
     */
    void FileAction(bool save);
    /**
     * @brief Applies a document from edit history.
     * @param redo True to redo; false to undo.
     */
    void History(bool redo);
    /** @brief Recompiles the library and refreshes named state sources. */
    void RefreshSources();
    AnimationGraphDocument m_Document;
    AnimationGraphDocument::Sources m_Sources;
    std::vector<AnimationGraphDocument> m_Undo;
    std::vector<AnimationGraphDocument> m_Redo;
    AnimationGraphDocument m_LastEdit;
    Ref<AnimationGraphCanvas> m_Canvas;
    Ref<GUI::VerticalBox> m_Inspector, m_PreviewInputs;
    Ref<AnimationPosePreview> m_PosePreview;
    Ref<GUI::TextBlock> m_DocumentTitle, m_PoseStatus, m_Status, m_PlaybackStatus;
    Ref<GUI::Button> m_PlayButton, m_DetailsTab, m_DefaultsTab;
    Ref<GUI::TextField> m_Path;
    Ref<Animation> m_Preview;
    EAnimationAssetKind m_AssetKind = EAnimationAssetKind::Sequence;
    int m_Asset = -1;
    bool m_ShowDefaults = false, m_MachineSelected = false;
    bool m_Refresh = true, m_Playing = false, m_Dirty = false;
};
