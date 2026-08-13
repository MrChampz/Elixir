#include "FileSystemWatcher.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <array>
#include <cstddef>
#include <thread>

namespace EditorSupport
{
    namespace
    {
        class WindowsFileSystemWatcher final : public IFileSystemWatcher
        {
        public:
            ~WindowsFileSystemWatcher() override
            {
                Stop();
            }

            bool Start(const std::filesystem::path& directory, Callback callback) override
            {
                Stop();
                m_Root = std::filesystem::absolute(directory).lexically_normal();
                m_Callback = std::move(callback);
                m_DirectoryHandle = CreateFileW(
                    m_Root.c_str(),
                    FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS,
                    nullptr
                );
                if (m_DirectoryHandle == INVALID_HANDLE_VALUE)
                    return false;

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
                    if (m_DirectoryHandle != INVALID_HANDLE_VALUE)
                    {
                        CancelIoEx(m_DirectoryHandle, nullptr);
                        CancelSynchronousIo(m_Thread.native_handle());
                    }
                    m_Thread.join();
                }
                if (m_DirectoryHandle != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(m_DirectoryHandle);
                    m_DirectoryHandle = INVALID_HANDLE_VALUE;
                }
                m_Callback = {};
            }

        private:
            void Run(const std::stop_token stopToken)
            {
                alignas(DWORD) std::array<std::byte, 64 * 1024> buffer {};
                while (!stopToken.stop_requested())
                {
                    DWORD bytesReturned = 0;
                    const BOOL succeeded = ReadDirectoryChangesW(
                        m_DirectoryHandle,
                        buffer.data(),
                        static_cast<DWORD>(buffer.size()),
                        TRUE,
                        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
                            | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE
                            | FILE_NOTIFY_CHANGE_CREATION,
                        &bytesReturned,
                        nullptr,
                        nullptr
                    );
                    if (!succeeded || bytesReturned == 0)
                        continue;

                    std::size_t offset = 0;
                    while (offset < bytesReturned)
                    {
                        const auto* nativeEvent = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer.data() + offset);
                        const std::wstring relativeName(
                            nativeEvent->FileName,
                            nativeEvent->FileNameLength / sizeof(wchar_t)
                        );
                        SFileSystemEvent event;
                        event.Path = m_Root / relativeName;
                        switch (nativeEvent->Action)
                        {
                            case FILE_ACTION_ADDED: event.Type = EFileSystemEventType::Added; break;
                            case FILE_ACTION_REMOVED: event.Type = EFileSystemEventType::Removed; break;
                            case FILE_ACTION_RENAMED_OLD_NAME:
                            case FILE_ACTION_RENAMED_NEW_NAME: event.Type = EFileSystemEventType::Renamed; break;
                            default: event.Type = EFileSystemEventType::Modified; break;
                        }
                        if (m_Callback)
                            m_Callback(event);
                        if (nativeEvent->NextEntryOffset == 0)
                            break;
                        offset += nativeEvent->NextEntryOffset;
                    }
                }
            }

            HANDLE m_DirectoryHandle = INVALID_HANDLE_VALUE;
            std::filesystem::path m_Root;
            Callback m_Callback;
            std::jthread m_Thread;
        };
    }

    std::unique_ptr<IFileSystemWatcher> CreateNativeFileSystemWatcher()
    {
        return std::make_unique<WindowsFileSystemWatcher>();
    }
}
