#include "ContentBrowser.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

#include <algorithm>
#include <cctype>
#include <format>
#include <system_error>

namespace
{
    constexpr auto RefreshDebounce = std::chrono::milliseconds(180);

    std::string PathKey(const std::filesystem::path& path)
    {
        return path.lexically_normal().generic_string();
    }

    bool IsHiddenPath(const std::filesystem::path& relativePath)
    {
        for (const auto& component : relativePath)
        {
            const std::string name = component.string();
            if (!name.empty() && name.front() == '.')
                return true;
        }
        return false;
    }
}

ContentBrowser::ContentBrowser(
    Rml::ElementDocument* document,
    std::filesystem::path assetRoot,
    EffectOpenCallback onEffectOpen
)
    : m_Document(document),
      m_AssetRoot(std::filesystem::absolute(std::move(assetRoot)).lexically_normal()),
      m_CurrentDirectory(m_AssetRoot),
      m_OnEffectOpen(std::move(onEffectOpen)),
      m_Watcher(EditorSupport::CreateNativeFileSystemWatcher())
{
    m_ExpandedDirectories.insert(PathKey(m_AssetRoot));

    if (m_Document)
    {
        m_Document->AddEventListener("mousedown", this, true);
        m_ListenerRegistered = true;
    }

    SetLoadingState(true);
    BeginScan();

    if (m_Watcher)
    {
        m_Watcher->Start(m_AssetRoot, [this](const EditorSupport::SFileSystemEvent&)
        {
            RequestRefresh();
        });
    }
}

ContentBrowser::~ContentBrowser()
{
    if (m_Watcher)
        m_Watcher->Stop();
    if (m_ScanFuture.valid())
        m_ScanFuture.wait();
    if (m_ListenerRegistered)
        m_Document->RemoveEventListener("mousedown", this, true);
}

void ContentBrowser::Update()
{
    if (m_ScanFuture.valid()
        && m_ScanFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        ApplyScanResult(m_ScanFuture.get());
    }

    // RmlUi is still dispatching the pointer event while ProcessEvent runs. Replacing
    // the clicked subtree there invalidates the event target, so navigation is
    // applied on the following editor update instead.
    if (m_PendingToggle)
    {
        const std::filesystem::path toggledPath = std::move(*m_PendingToggle);
        m_PendingToggle.reset();
        const std::string key = PathKey(toggledPath);
        const bool expanded = !m_ExpandedDirectories.contains(key);
        if (expanded)
            m_ExpandedDirectories.insert(key);
        else
            m_ExpandedDirectories.erase(key);

        RebuildView();
        if (Rml::Element* status = m_Document->GetElementById("status-left"))
        {
            status->SetInnerRML(
                std::string(expanded ? "Expanded folder: " : "Collapsed folder: ")
                + EscapeRml(toggledPath.filename().string())
            );
        }
    }

    if (m_PendingDirectory)
    {
        m_CurrentDirectory = std::move(*m_PendingDirectory);
        m_PendingDirectory.reset();

        const bool expandAncestors = m_PendingNavigationExpandsAncestors;
        m_PendingNavigationExpandsAncestors = true;
        if (expandAncestors)
        {
            m_ExpandedDirectories.insert(PathKey(m_AssetRoot));
            if (m_CurrentDirectory != m_AssetRoot)
            {
                for (std::filesystem::path ancestor = m_CurrentDirectory.parent_path();
                     ancestor != m_AssetRoot && !ancestor.empty();
                     ancestor = ancestor.parent_path())
                {
                    m_ExpandedDirectories.insert(PathKey(ancestor));
                }
            }
        }

        RebuildView();
        if (Rml::Element* status = m_Document->GetElementById("status-left"))
            status->SetInnerRML("Opened folder: " + EscapeRml(m_CurrentDirectory.filename().string()));
    }

    if (m_PendingEffect)
    {
        const std::filesystem::path effectPath = std::move(*m_PendingEffect);
        m_PendingEffect.reset();
        if (m_OnEffectOpen)
            m_OnEffectOpen(effectPath);
    }

    if (!m_RefreshRequested.load(std::memory_order_acquire) || m_ScanFuture.valid())
        return;

    const auto lastNotification = std::chrono::steady_clock::time_point(
        std::chrono::nanoseconds(m_LastNotificationNanoseconds.load(std::memory_order_acquire))
    );
    if (std::chrono::steady_clock::now() - lastNotification < RefreshDebounce)
        return;

    m_RefreshRequested.store(false, std::memory_order_release);
    BeginScan();
}

void ContentBrowser::ProcessEvent(Rml::Event& event)
{
    if (event.GetParameter("button", 0) != 0)
        return;

    Rml::Element* element = event.GetTargetElement();
    auto pathIt = m_ElementPaths.end();
    while (element)
    {
        pathIt = m_ElementPaths.find(element->GetId());
        if (pathIt != m_ElementPaths.end())
            break;
        if (element->GetId() == "content-browser-content")
            break;
        element = element->GetParentNode();
    }
    if (!element || pathIt == m_ElementPaths.end())
        return;

    const std::string id = element->GetId();
    const std::filesystem::path& path = pathIt->second;
    std::error_code error;
    if (std::filesystem::is_directory(path, error))
    {
        const bool isFolderRow = id == "content-folder-root" || id.starts_with("content-folder-");
        if (isFolderRow && m_DirectoriesWithChildren.contains(PathKey(path)))
            m_PendingToggle = path;

        m_PendingDirectory = path;
        m_PendingNavigationExpandsAncestors = !isFolderRow;
        return;
    }

    for (const auto& [otherId, unusedPath] : m_ElementPaths)
    {
        if (otherId.starts_with("content-item-"))
        {
            if (Rml::Element* otherElement = m_Document->GetElementById(otherId))
                otherElement->SetClass("asset-selected", otherId == id);
        }
    }

    if (Rml::Element* status = m_Document->GetElementById("status-left"))
        status->SetInnerRML("Selected asset: " + EscapeRml(path.filename().string()));

    const auto entry = std::ranges::find_if(m_Entries, [&path](const SAssetEntry& candidate)
    {
        return candidate.AbsolutePath == path;
    });
    if (entry != m_Entries.end() && entry->Kind == EAssetKind::Effect)
        m_PendingEffect = path;
}

void ContentBrowser::BeginScan()
{
    SetLoadingState(true);
    const std::filesystem::path root = m_AssetRoot;
    m_ScanFuture = std::async(std::launch::async, [root]
    {
        return Scan(root);
    });
}

void ContentBrowser::ApplyScanResult(SScanResult result)
{
    if (!result.Error.empty())
    {
        SetErrorState(result.Error);
        return;
    }

    m_Entries = std::move(result.Entries);
    std::error_code error;
    if (!std::filesystem::is_directory(m_CurrentDirectory, error))
        m_CurrentDirectory = m_AssetRoot;

    SetErrorState("");
    SetLoadingState(false);
    RebuildView();

    if (Rml::Element* status = m_Document->GetElementById("status-left"))
        status->SetInnerRML(std::format("Assets indexed: {}", m_Entries.size()));
}

void ContentBrowser::RebuildView()
{
    Rml::Element* folderTree = m_Document->GetElementById("content-browser-folders");
    Rml::Element* assetGrid = m_Document->GetElementById("content-browser-grid");
    Rml::Element* assetPath = m_Document->GetElementById("asset-path");
    Rml::Element* emptyState = m_Document->GetElementById("content-browser-empty");
    if (!folderTree || !assetGrid || !assetPath || !emptyState)
        return;

    m_ElementPaths.clear();
    Rml::String foldersMarkup;
    Rml::String breadcrumbMarkup;
    Rml::String gridMarkup;

    m_DirectoriesWithChildren.clear();
    for (const SAssetEntry& entry : m_Entries)
    {
        if (entry.Kind == EAssetKind::Directory)
            m_DirectoriesWithChildren.insert(PathKey(entry.AbsolutePath.parent_path()));
    }

    const std::string rootId = "content-folder-root";
    const bool rootSelected = m_CurrentDirectory == m_AssetRoot;
    const bool rootExpanded = m_ExpandedDirectories.contains(PathKey(m_AssetRoot));
    const bool rootHasChildren = m_DirectoriesWithChildren.contains(PathKey(m_AssetRoot));
    foldersMarkup += std::format(
        "<div id=\"{}\" class=\"folder-row{}\">"
        "<svg class=\"folder-toggle-icon{}{}\" src=\"Icons/chevron-down.svg\"></svg>"
        "<span class=\"folder-label\">Assets</span></div>",
        rootId,
        rootSelected ? " folder-selected" : "",
        rootExpanded ? " folder-toggle-expanded" : "",
        rootHasChildren ? "" : " folder-toggle-empty"
    );
    m_ElementPaths[rootId] = m_AssetRoot;

    const std::string rootBreadcrumbId = "content-breadcrumb-0";
    breadcrumbMarkup += std::format(
        "<span id=\"{}\" class=\"content-breadcrumb-segment{}\">Assets</span>",
        rootBreadcrumbId,
        rootSelected ? " content-breadcrumb-current" : ""
    );
    m_ElementPaths[rootBreadcrumbId] = m_AssetRoot;

    std::error_code breadcrumbError;
    const std::filesystem::path breadcrumbRelative = std::filesystem::relative(
        m_CurrentDirectory, m_AssetRoot, breadcrumbError
    );
    if (!breadcrumbError && !breadcrumbRelative.empty() && breadcrumbRelative != ".")
    {
        std::filesystem::path accumulatedPath = m_AssetRoot;
        std::size_t breadcrumbIndex = 1;
        for (const auto& component : breadcrumbRelative)
        {
            accumulatedPath /= component;
            const std::string id = std::format("content-breadcrumb-{}", breadcrumbIndex++);
            const bool current = accumulatedPath == m_CurrentDirectory;
            breadcrumbMarkup += "<svg class=\"content-breadcrumb-chevron\" src=\"Icons/chevron-down.svg\"></svg>";
            breadcrumbMarkup += std::format(
                "<span id=\"{}\" class=\"content-breadcrumb-segment{}\">{}</span>",
                id,
                current ? " content-breadcrumb-current" : "",
                EscapeRml(component.string())
            );
            m_ElementPaths[id] = accumulatedPath;
        }
    }

    std::size_t folderIndex = 0;
    for (const SAssetEntry& entry : m_Entries)
    {
        if (entry.Kind != EAssetKind::Directory)
            continue;

        bool visible = rootExpanded;
        for (std::filesystem::path ancestor = entry.AbsolutePath.parent_path();
             visible && ancestor != m_AssetRoot && !ancestor.empty();
             ancestor = ancestor.parent_path())
        {
            visible = m_ExpandedDirectories.contains(PathKey(ancestor));
        }
        if (!visible)
            continue;

        const std::size_t currentFolderIndex = folderIndex++;
        const std::string id = std::format("content-folder-{}", currentFolderIndex);
        const std::size_t depth = static_cast<std::size_t>(std::distance(
            entry.RelativePath.begin(), entry.RelativePath.end()
        ));
        const int padding = 4 + static_cast<int>(depth - 1) * 14;
        const bool selected = entry.AbsolutePath == m_CurrentDirectory;
        const bool hasChildren = m_DirectoriesWithChildren.contains(PathKey(entry.AbsolutePath));
        const bool expanded = m_ExpandedDirectories.contains(PathKey(entry.AbsolutePath));
        foldersMarkup += std::format(
            "<div id=\"{}\" class=\"folder-row{}\" style=\"padding-left: {}dp;\">"
            "<svg class=\"folder-toggle-icon{}{}\" src=\"Icons/chevron-down.svg\"></svg>"
            "<span class=\"folder-label\">{}</span></div>",
            id,
            selected ? " folder-selected" : "",
            padding,
            expanded ? " folder-toggle-expanded" : "",
            hasChildren ? "" : " folder-toggle-empty",
            EscapeRml(entry.Name)
        );
        m_ElementPaths[id] = entry.AbsolutePath;
    }

    std::size_t visibleIndex = 0;
    for (const SAssetEntry& entry : m_Entries)
    {
        if (entry.AbsolutePath.parent_path() != m_CurrentDirectory)
            continue;

        const std::string id = std::format("content-item-{}", visibleIndex);
        const int left = 12 + static_cast<int>(visibleIndex % 10) * 120;
        const int top = 32 + static_cast<int>(visibleIndex / 10) * 116;
        gridMarkup += std::format(
            "<div id=\"{}\" class=\"asset-card\" style=\"left: {}dp; top: {}dp;\">"
            "<div class=\"asset-preview {}\"><span class=\"asset-kind\">{}</span></div>"
            "<div class=\"asset-label\">{}</div></div>",
            id,
            left,
            top,
            KindClass(entry.Kind),
            KindLabel(entry.Kind),
            EscapeRml(entry.Name)
        );
        m_ElementPaths[id] = entry.AbsolutePath;
        ++visibleIndex;
    }

    folderTree->SetInnerRML(foldersMarkup);
    assetGrid->SetInnerRML(gridMarkup);
    assetPath->SetInnerRML(breadcrumbMarkup);
    emptyState->SetClass("content-browser-state-visible", visibleIndex == 0);
}

void ContentBrowser::RequestRefresh()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    m_LastNotificationNanoseconds.store(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count(),
        std::memory_order_release
    );
    m_RefreshRequested.store(true, std::memory_order_release);
}

void ContentBrowser::SetLoadingState(const bool loading)
{
    if (Rml::Element* content = m_Document->GetElementById("content-browser-content"))
        content->SetClass("content-browser-content-hidden", loading);
    if (Rml::Element* loadingState = m_Document->GetElementById("content-browser-loading"))
        loadingState->SetClass("content-browser-state-visible", loading);
    if (loading)
    {
        if (Rml::Element* emptyState = m_Document->GetElementById("content-browser-empty"))
            emptyState->SetClass("content-browser-state-visible", false);
        if (Rml::Element* errorState = m_Document->GetElementById("content-browser-error"))
            errorState->SetClass("content-browser-state-visible", false);
    }
}

void ContentBrowser::SetErrorState(const std::string& message)
{
    if (Rml::Element* errorState = m_Document->GetElementById("content-browser-error"))
    {
        errorState->SetInnerRML(EscapeRml(message));
        errorState->SetClass("content-browser-state-visible", !message.empty());
    }
    if (!message.empty())
    {
        SetLoadingState(false);
        if (Rml::Element* content = m_Document->GetElementById("content-browser-content"))
            content->SetClass("content-browser-content-hidden", true);
    }
}

ContentBrowser::SScanResult ContentBrowser::Scan(const std::filesystem::path& root)
{
    SScanResult result;
    std::error_code error;
    if (!std::filesystem::is_directory(root, error))
    {
        result.Error = "Asset directory is not available: " + root.string();
        return result;
    }

    const auto options = std::filesystem::directory_options::skip_permission_denied;
    std::filesystem::recursive_directory_iterator iterator(root, options, error);
    const std::filesystem::recursive_directory_iterator end;
    while (!error && iterator != end)
    {
        const std::filesystem::directory_entry& directoryEntry = *iterator;
        std::filesystem::path relative = std::filesystem::relative(directoryEntry.path(), root, error);
        if (error)
            break;

        if (IsHiddenPath(relative))
        {
            if (directoryEntry.is_directory(error))
                iterator.disable_recursion_pending();
            iterator.increment(error);
            continue;
        }

        SAssetEntry entry;
        entry.AbsolutePath = directoryEntry.path().lexically_normal();
        entry.RelativePath = std::move(relative);
        entry.Name = directoryEntry.path().filename().string();
        entry.Kind = Classify(directoryEntry);
        if (entry.Kind != EAssetKind::Directory)
            entry.Size = directoryEntry.file_size(error);
        if (error)
            error.clear();
        entry.LastWriteTime = directoryEntry.last_write_time(error);
        if (error)
            error.clear();
        result.Entries.push_back(std::move(entry));
        iterator.increment(error);
    }

    if (error)
    {
        result.Error = "Could not scan Assets: " + error.message();
        return result;
    }

    std::ranges::sort(result.Entries, [](const SAssetEntry& left, const SAssetEntry& right)
    {
        if ((left.Kind == EAssetKind::Directory) != (right.Kind == EAssetKind::Directory))
            return left.Kind == EAssetKind::Directory;
        return left.RelativePath.generic_string() < right.RelativePath.generic_string();
    });
    return result;
}

ContentBrowser::EAssetKind ContentBrowser::Classify(const std::filesystem::directory_entry& entry)
{
    std::error_code error;
    if (entry.is_directory(error))
        return EAssetKind::Directory;

    std::string extension = entry.path().extension().string();
    std::ranges::transform(extension, extension.begin(), [](const unsigned char character)
    {
        return static_cast<char>(std::tolower(character));
    });

    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg"
        || extension == ".hdr" || extension == ".dds" || extension == ".ktx")
        return EAssetKind::Texture;
    if (extension == ".mat" || extension == ".material")
        return EAssetKind::Material;
    if (extension == ".gltf" || extension == ".glb" || extension == ".obj" || extension == ".fbx")
        return EAssetKind::Mesh;
    if (extension == ".scene")
        return EAssetKind::Scene;
    if (extension == ".effect" || extension == ".vfx" || extension == ".json")
        return EAssetKind::Effect;
    if (extension == ".hlsl" || extension == ".glsl" || extension == ".spv")
        return EAssetKind::Shader;
    if (extension == ".ttf" || extension == ".otf")
        return EAssetKind::Font;
    return EAssetKind::Other;
}

std::string ContentBrowser::KindClass(const EAssetKind kind)
{
    switch (kind)
    {
        case EAssetKind::Directory: return "asset-preview-folder";
        case EAssetKind::Texture: return "asset-preview-texture";
        case EAssetKind::Material: return "asset-preview-material";
        case EAssetKind::Mesh: return "asset-preview-mesh";
        case EAssetKind::Scene: return "asset-preview-scene";
        case EAssetKind::Effect: return "asset-preview-effect";
        case EAssetKind::Shader: return "asset-preview-shader";
        case EAssetKind::Font: return "asset-preview-font";
        case EAssetKind::Other: return "asset-preview-file";
    }
    return "asset-preview-file";
}

std::string ContentBrowser::KindLabel(const EAssetKind kind)
{
    switch (kind)
    {
        case EAssetKind::Directory: return "DIR";
        case EAssetKind::Texture: return "TEX";
        case EAssetKind::Material: return "MAT";
        case EAssetKind::Mesh: return "MESH";
        case EAssetKind::Scene: return "SCENE";
        case EAssetKind::Effect: return "VFX";
        case EAssetKind::Shader: return "SHADER";
        case EAssetKind::Font: return "FONT";
        case EAssetKind::Other: return "FILE";
    }
    return "FILE";
}

std::string ContentBrowser::EscapeRml(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value)
    {
        switch (character)
        {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '\"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += character; break;
        }
    }
    return escaped;
}
