#pragma once

#include <filesystem>
#include <functional>
#include <memory>

namespace EditorSupport
{
    enum class EFileSystemEventType
    {
        Added,
        Removed,
        Modified,
        Renamed,
        Overflow
    };

    struct SFileSystemEvent
    {
        EFileSystemEventType Type = EFileSystemEventType::Modified;
        std::filesystem::path Path;
    };

    class IFileSystemWatcher
    {
    public:
        using Callback = std::function<void(const SFileSystemEvent&)>;

        virtual ~IFileSystemWatcher() = default;
        virtual bool Start(const std::filesystem::path& directory, Callback callback) = 0;
        virtual void Stop() = 0;
    };

    std::unique_ptr<IFileSystemWatcher> CreateNativeFileSystemWatcher();
}
