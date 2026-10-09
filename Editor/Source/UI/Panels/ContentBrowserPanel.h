#pragma once
#include "../EditorPanel.h"
#include <filesystem>
#include <functional>

/** @brief Opens project effect assets from the main editor viewport. */
class ContentBrowserPanel final : public EditorPanel
{
public:
    /** @brief Creates a project browser with a document-window opening callback. */
    ContentBrowserPanel(std::filesystem::path root, std::function<void(const std::filesystem::path&)> open)
        : m_ProjectRoot(std::move(root)), m_Open(std::move(open)) {}
    /** @brief Returns the panel name. */
    const char* GetName() const override { return "Content Browser"; }
    /** @brief Builds the floating asset browser on the main viewport. */
    Ref<GUI::Widget> Build() override;
    /** @brief Refreshes the asset list after search and refresh actions. */
    void OnUpdate(Timestep frameTime) override;
private:
    // Updates the visible effect cards from the project asset directory.
    void Refresh();
    std::filesystem::path m_ProjectRoot;
    std::function<void(const std::filesystem::path&)> m_Open;
    Ref<GUI::HorizontalBox> m_Assets;
    Ref<GUI::TextBlock> m_Status;
    std::string m_Filter;
    bool m_Refresh = true;
};
