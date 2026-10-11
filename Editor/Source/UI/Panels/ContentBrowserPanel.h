#pragma once
#include "../EditorPanel.h"
#include <Engine/GUI/ScrollBox.h>
#include <filesystem>
#include <functional>

/** @brief Browses project assets in a sliding drawer shared by all editor tabs. */
class ContentBrowserPanel final : public EditorPanel
{
public:
    /** @brief Creates a project browser with a VFX document opening callback. */
    ContentBrowserPanel(std::filesystem::path root, std::function<void(const std::filesystem::path&)> open)
        : m_ProjectRoot(std::move(root)), m_Open(std::move(open)) {}
    /** @brief Returns the panel name. */
    const char* GetName() const override { return "Content Browser"; }
    /** @brief Builds the folder tree, asset grid, search, and drawer surface. */
    Ref<GUI::Widget> Build() override;
    /** @brief Animates the drawer and applies deferred navigation and search changes. */
    void OnUpdate(Timestep frameTime) override;
    /** @brief Opens or closes the drawer with the shared panel slide and fade. */
    void SetOpen(bool open);
    /** @brief Returns the drawer's requested open state. */
    bool IsOpen() const { return m_IsOpen; }
    /** @brief Returns whether the visible drawer covers a point in GUI coordinates. */
    bool Contains(const glm::vec2& point) const;
private:
    // Rebuilds the current directory's asset grid and folder tree.
    void Refresh();
    // Selects a project asset directory for the next refresh.
    void Navigate(const std::filesystem::path& directory);
    // Adds visible folder rows recursively without following symbolic links.
    void AddFolders(const std::filesystem::path& directory, int depth);

    std::filesystem::path m_ProjectRoot, m_Directory, m_SelectedAsset;
    std::function<void(const std::filesystem::path&)> m_Open;
    Ref<GUI::Canvas> m_Root;
    Ref<GUI::Overlay> m_Surface;
    Scope<GUI::WidgetAnimation> m_Animation;
    GUI::CanvasSlot* m_SurfaceSlot = nullptr;
    Ref<GUI::VerticalBox> m_Assets, m_Folders;
    Ref<GUI::ScrollBox> m_AssetScroll;
    Ref<GUI::TextBlock> m_Status, m_Path;
    std::string m_Filter;
    bool m_Refresh = true, m_IsOpen = false, m_OnlyVFX = false;
    float m_DrawerHeight = 320;
    int m_Columns = 1;
};
