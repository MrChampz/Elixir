#pragma once

#include "FileSystemWatcher.h"

#include <RmlUi/Core/EventListener.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Rml
{
    class ElementDocument;
}

class ContentBrowser final : public Rml::EventListener
{
public:
    using EffectOpenCallback = std::function<void(const std::filesystem::path&)>;

    ContentBrowser(
        Rml::ElementDocument* document,
        std::filesystem::path assetRoot,
        EffectOpenCallback onEffectOpen = {}
    );
    ~ContentBrowser() override;

    void Update();
    void ProcessEvent(Rml::Event& event) override;

private:
    enum class EAssetKind
    {
        Directory,
        Texture,
        Material,
        Mesh,
        Scene,
        Effect,
        Shader,
        Font,
        Other
    };

    struct SAssetEntry
    {
        std::filesystem::path AbsolutePath;
        std::filesystem::path RelativePath;
        std::string Name;
        EAssetKind Kind = EAssetKind::Other;
        std::uintmax_t Size = 0;
        std::filesystem::file_time_type LastWriteTime {};
    };

    struct SScanResult
    {
        std::vector<SAssetEntry> Entries;
        std::string Error;
    };

    void BeginScan();
    void ApplyScanResult(SScanResult result);
    void RebuildView();
    void RequestRefresh();
    void SetLoadingState(bool loading);
    void SetErrorState(const std::string& message);
    static SScanResult Scan(const std::filesystem::path& root);
    static EAssetKind Classify(const std::filesystem::directory_entry& entry);
    static std::string KindClass(EAssetKind kind);
    static std::string KindLabel(EAssetKind kind);
    static std::string EscapeRml(const std::string& value);

    Rml::ElementDocument* m_Document = nullptr;
    std::filesystem::path m_AssetRoot;
    std::filesystem::path m_CurrentDirectory;
    std::optional<std::filesystem::path> m_PendingDirectory;
    std::optional<std::filesystem::path> m_PendingToggle;
    std::optional<std::filesystem::path> m_PendingEffect;
    bool m_PendingNavigationExpandsAncestors = true;
    std::unordered_set<std::string> m_ExpandedDirectories;
    std::unordered_set<std::string> m_DirectoriesWithChildren;
    std::vector<SAssetEntry> m_Entries;
    std::unordered_map<std::string, std::filesystem::path> m_ElementPaths;
    EffectOpenCallback m_OnEffectOpen;
    std::unique_ptr<EditorSupport::IFileSystemWatcher> m_Watcher;
    std::future<SScanResult> m_ScanFuture;
    std::atomic<bool> m_RefreshRequested = false;
    std::atomic<std::int64_t> m_LastNotificationNanoseconds = 0;
    bool m_ListenerRegistered = false;
};
