#include "FileSystemWatcher.h"

#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>

#include <mutex>

namespace EditorSupport
{
    namespace
    {
        class FSEventsFileSystemWatcher final : public IFileSystemWatcher
        {
        public:
            ~FSEventsFileSystemWatcher() override
            {
                Stop();
            }

            bool Start(const std::filesystem::path& directory, Callback callback) override
            {
                Stop();
                m_Directory = std::filesystem::absolute(directory).lexically_normal();
                m_Callback = std::move(callback);

                CFStringRef directoryString = CFStringCreateWithFileSystemRepresentation(
                    kCFAllocatorDefault,
                    m_Directory.c_str()
                );
                if (!directoryString)
                    return false;

                const void* values[] = { directoryString };
                CFArrayRef paths = CFArrayCreate(
                    kCFAllocatorDefault,
                    values,
                    1,
                    &kCFTypeArrayCallBacks
                );
                CFRelease(directoryString);
                if (!paths)
                    return false;

                FSEventStreamContext context {};
                context.info = this;
                constexpr FSEventStreamCreateFlags flags =
                    kFSEventStreamCreateFlagFileEvents
                    | kFSEventStreamCreateFlagNoDefer
                    | kFSEventStreamCreateFlagWatchRoot;
                m_Stream = FSEventStreamCreate(
                    kCFAllocatorDefault,
                    &FSEventsFileSystemWatcher::HandleEvents,
                    &context,
                    paths,
                    kFSEventStreamEventIdSinceNow,
                    0.1,
                    flags
                );
                CFRelease(paths);
                if (!m_Stream)
                    return false;

                m_Queue = dispatch_queue_create("com.elixir.editor.assets", DISPATCH_QUEUE_SERIAL);
                FSEventStreamSetDispatchQueue(m_Stream, m_Queue);
                if (!FSEventStreamStart(m_Stream))
                {
                    Stop();
                    return false;
                }
                return true;
            }

            void Stop() override
            {
                if (m_Stream)
                {
                    FSEventStreamStop(m_Stream);
                    FSEventStreamInvalidate(m_Stream);
                    FSEventStreamRelease(m_Stream);
                    m_Stream = nullptr;
                }
                if (m_Queue)
                {
#if !OS_OBJECT_USE_OBJC
                    dispatch_release(m_Queue);
#endif
                    m_Queue = nullptr;
                }
                const std::lock_guard lock(m_CallbackMutex);
                m_Callback = {};
            }

        private:
            static void HandleEvents(
                ConstFSEventStreamRef,
                void* clientInfo,
                const std::size_t eventCount,
                void* eventPaths,
                const FSEventStreamEventFlags eventFlags[],
                const FSEventStreamEventId[]
            )
            {
                auto* watcher = static_cast<FSEventsFileSystemWatcher*>(clientInfo);
                auto** paths = static_cast<char**>(eventPaths);
                for (std::size_t index = 0; index < eventCount; ++index)
                {
                    SFileSystemEvent event;
                    event.Path = paths[index];
                    const FSEventStreamEventFlags flags = eventFlags[index];
                    if (flags & (kFSEventStreamEventFlagMustScanSubDirs
                        | kFSEventStreamEventFlagUserDropped
                        | kFSEventStreamEventFlagKernelDropped))
                        event.Type = EFileSystemEventType::Overflow;
                    else if (flags & kFSEventStreamEventFlagItemRenamed)
                        event.Type = EFileSystemEventType::Renamed;
                    else if (flags & kFSEventStreamEventFlagItemCreated)
                        event.Type = EFileSystemEventType::Added;
                    else if (flags & kFSEventStreamEventFlagItemRemoved)
                        event.Type = EFileSystemEventType::Removed;
                    else
                        event.Type = EFileSystemEventType::Modified;
                    watcher->Emit(event);
                }
            }

            void Emit(const SFileSystemEvent& event)
            {
                Callback callback;
                {
                    const std::lock_guard lock(m_CallbackMutex);
                    callback = m_Callback;
                }
                if (callback)
                    callback(event);
            }

            std::filesystem::path m_Directory;
            FSEventStreamRef m_Stream = nullptr;
            dispatch_queue_t m_Queue = nullptr;
            Callback m_Callback;
            std::mutex m_CallbackMutex;
        };
    }

    std::unique_ptr<IFileSystemWatcher> CreateNativeFileSystemWatcher()
    {
        return std::make_unique<FSEventsFileSystemWatcher>();
    }
}
