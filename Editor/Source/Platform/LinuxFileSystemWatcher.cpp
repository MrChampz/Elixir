#include "FileSystemWatcher.h"

#include <sys/inotify.h>
#include <poll.h>
#include <unistd.h>

#include <array>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace EditorSupport
{
    namespace
    {
        class InotifyFileSystemWatcher final : public IFileSystemWatcher
        {
        public:
            ~InotifyFileSystemWatcher() override
            {
                Stop();
            }

            bool Start(const std::filesystem::path& directory, Callback callback) override
            {
                Stop();
                m_FileDescriptor = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
                if (m_FileDescriptor < 0)
                    return false;

                m_Callback = std::move(callback);
                m_Root = std::filesystem::absolute(directory).lexically_normal();
                AddDirectoryTree(m_Root);
                m_Thread = std::jthread([this](const std::stop_token stopToken)
                {
                    Run(stopToken);
                });
                return true;
            }

            void Stop() override
            {
                if (m_Thread.joinable())
                {
                    m_Thread.request_stop();
                    m_Thread.join();
                }
                if (m_FileDescriptor >= 0)
                {
                    close(m_FileDescriptor);
                    m_FileDescriptor = -1;
                }
                m_WatchedDirectories.clear();
                m_Callback = {};
            }

        private:
            static constexpr std::uint32_t WatchMask = IN_CREATE | IN_DELETE | IN_MODIFY
                | IN_CLOSE_WRITE | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB
                | IN_DELETE_SELF | IN_MOVE_SELF;

            void AddDirectoryTree(const std::filesystem::path& directory)
            {
                AddDirectory(directory);
                std::error_code error;
                for (std::filesystem::recursive_directory_iterator iterator(
                         directory,
                         std::filesystem::directory_options::skip_permission_denied,
                         error
                     ), end;
                     !error && iterator != end;
                     iterator.increment(error))
                {
                    if (iterator->is_directory(error))
                        AddDirectory(iterator->path());
                    if (error)
                        error.clear();
                }
            }

            void AddDirectory(const std::filesystem::path& directory)
            {
                const int descriptor = inotify_add_watch(m_FileDescriptor, directory.c_str(), WatchMask);
                if (descriptor >= 0)
                    m_WatchedDirectories[descriptor] = directory;
            }

            void Run(const std::stop_token stopToken)
            {
                alignas(inotify_event) std::array<char, 64 * 1024> buffer {};
                while (!stopToken.stop_requested())
                {
                    pollfd pollDescriptor { m_FileDescriptor, POLLIN, 0 };
                    const int pollResult = poll(&pollDescriptor, 1, 200);
                    if (pollResult <= 0 || !(pollDescriptor.revents & POLLIN))
                        continue;

                    const ssize_t byteCount = read(m_FileDescriptor, buffer.data(), buffer.size());
                    if (byteCount <= 0)
                        continue;

                    std::size_t offset = 0;
                    while (offset < static_cast<std::size_t>(byteCount))
                    {
                        const auto* nativeEvent = reinterpret_cast<const inotify_event*>(buffer.data() + offset);
                        SFileSystemEvent event;
                        const auto directoryIt = m_WatchedDirectories.find(nativeEvent->wd);
                        if (directoryIt != m_WatchedDirectories.end())
                        {
                            event.Path = directoryIt->second;
                            if (nativeEvent->len > 0)
                                event.Path /= nativeEvent->name;
                        }

                        if (nativeEvent->mask & IN_Q_OVERFLOW)
                            event.Type = EFileSystemEventType::Overflow;
                        else if (nativeEvent->mask & (IN_CREATE | IN_MOVED_TO))
                            event.Type = EFileSystemEventType::Added;
                        else if (nativeEvent->mask & (IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF))
                            event.Type = EFileSystemEventType::Removed;
                        else if (nativeEvent->mask & IN_MOVE_SELF)
                            event.Type = EFileSystemEventType::Renamed;
                        else
                            event.Type = EFileSystemEventType::Modified;

                        if ((nativeEvent->mask & IN_ISDIR) && (nativeEvent->mask & (IN_CREATE | IN_MOVED_TO)))
                            AddDirectoryTree(event.Path);
                        if (m_Callback)
                            m_Callback(event);
                        offset += sizeof(inotify_event) + nativeEvent->len;
                    }
                }
            }

            int m_FileDescriptor = -1;
            std::filesystem::path m_Root;
            std::unordered_map<int, std::filesystem::path> m_WatchedDirectories;
            Callback m_Callback;
            std::jthread m_Thread;
        };
    }

    std::unique_ptr<IFileSystemWatcher> CreateNativeFileSystemWatcher()
    {
        return std::make_unique<InotifyFileSystemWatcher>();
    }
}
