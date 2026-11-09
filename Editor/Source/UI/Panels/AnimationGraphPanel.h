#pragma once

#include "../EditorPanel.h"
#include "../../Animation/AnimationGraphDocument.h"
#include <Engine/GUI/TextField.h>

class AnimationGraphCanvas;

/** Authors pose sources, montage actions, and state graphs with runtime previews. */
class AnimationGraphPanel final : public EditorPanel
{
  public:
    /** @brief Create a graph workspace with built-in sample animation sources. */
    AnimationGraphPanel();
    /**
     * @brief Open an authored machine with application-provided animation sources.
     * @param document State definitions, parameters, and canvas positions to edit.
     * @param sources Named runtime nodes used by source selection and persistence.
     */
    AnimationGraphPanel(
        AnimationGraphDocument document,
        AnimationGraphDocument::Sources sources
    );
    /** @brief Get the workspace title. */
    const char* GetName() const override { return "Animation Graph"; }
    /** @brief Build the graph canvas, toolbar, and property inspector. */
    Ref<GUI::Widget> Build() override;
    /**
     * @brief Apply deferred UI changes and advance the compiled preview.
     * @param frameTime Elapsed frame time in seconds.
     */
    void OnUpdate(Timestep frameTime) override;

  private:
    /** @brief Rebuild the inspector for the current selection. */
    void RefreshInspector();
    /** @brief Record an edit and invalidate the runtime preview. */
    void Changed();
    /** @brief Compile the current document and create an independent preview. */
    void Compile();
    /**
     * @brief Save or load the document at the path entered in the toolbar.
     * @param save True to save; false to load.
     */
    void FileAction(bool save);
    /**
     * @brief Apply a document from edit history.
     * @param redo True to redo; false to undo.
     */
    void History(bool redo);
    /** @brief Build properties for the selected resource or graph output. */
    void BuildAssetInspector();
    /** @brief Recompile the editable library and refresh named state sources. */
    void RefreshSources();
    /**
     * @brief Add an editable resource with a unique name.
     * @param kind Resource category.
     * @param twoDimensional Create a two-axis blend space when true.
     */
    void AddAsset(EAnimationAssetKind kind, bool twoDimensional = false);
    /**
     * @brief Rename a selected source and its graph references.
     * @param name Unique replacement resource name.
     */
    void RenameAsset(const std::string& name);
    /** @brief Delete the selected resource when no authored connection references it. */
    void DeleteAsset();
    /**
     * @brief Play an authored montage over the continuing graph preview.
     * @param index Montage index in the editable library.
     */
    void PlayMontage(size_t index);

    AnimationGraphDocument m_Document;
    AnimationGraphDocument::Sources m_Sources;
    std::vector<AnimationGraphDocument> m_Undo;
    std::vector<AnimationGraphDocument> m_Redo;
    AnimationGraphDocument m_LastEdit;
    Ref<AnimationGraphCanvas> m_Canvas;
    Ref<GUI::VerticalBox> m_Inspector;
    Ref<GUI::TextBlock> m_Status;
    Ref<GUI::TextField> m_Path;
    Ref<GUI::TextBlock> m_PlaybackStatus;
    Ref<Animation> m_Preview;
    int m_State = 0;
    int m_Transition = -1;
    int m_ConnectFrom = -1;
    bool m_Refresh = true;
    bool m_Playing = false;
    bool m_Dirty = false;
    bool m_LibraryView = true;
    EAnimationAssetKind m_AssetKind = EAnimationAssetKind::Sequence;
    int m_Asset = -1;
};
