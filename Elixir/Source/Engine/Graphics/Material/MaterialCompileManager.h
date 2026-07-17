#pragma once

#include <Engine/Graphics/Material/MaterialShaderMap.h>

#include <array>
#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Elixir
{
    enum class EMaterialCompilePriority : uint8_t { Background, Normal, Interactive };
    enum class EMaterialCompileState : uint8_t { Idle, Queued, Compiling };

    struct SMaterialCompileResult
    {
        uint64_t RequestId = 0;
        SMaterialShaderPermutation Permutation;
        std::optional<MaterialCompiler::SCompiled> Compiled;
    };

    // Process-wide scheduler for material compilation. A client supplies a logical
    // scope (normally a material slot) and permutation. Newer requests replace older
    // work for the same target, while different targets -- including targets from one
    // material slot -- can resolve through MaterialShaderMap in parallel. Only the
    // latest request for each target receives a callback.
    class ELIXIR_API MaterialCompileManager
    {
      public:
        using ClientId = uint64_t;
        using RequestId = uint64_t;
        using Task = std::function<void()>;
        using Scheduler = std::function<void(Task)>;
        using Resolver = std::function<std::optional<MaterialCompiler::SCompiled>(
            const MaterialGraph&, const SMaterialShaderPermutation&)>;
        using Callback = std::function<void(const SMaterialCompileResult&)>;

        explicit MaterialCompileManager(size_t maxParallelJobs = 0,
            Scheduler scheduler = {}, Resolver resolver = {});
        ~MaterialCompileManager();
        MaterialCompileManager(const MaterialCompileManager&) = delete;
        MaterialCompileManager& operator=(const MaterialCompileManager&) = delete;

        static MaterialCompileManager& Get();

        ClientId CreateClient();

        // Suppresses queued and future callbacks for this client. If a callback is
        // already running, waits for it before returning so the client's captured
        // state can be destroyed safely. In-flight DXC work is allowed to finish and
        // populate the shared shader map, but its stale result is not delivered.
        // Do not call this from one of the client's own completion callbacks.
        void DestroyClient(ClientId client);

        RequestId Request(ClientId client, uint64_t scope, MaterialGraph graph,
            SMaterialShaderPermutation permutation,
            Callback callback, EMaterialCompilePriority priority = EMaterialCompilePriority::Normal);

        // Replaces the scope's generation and schedules one independent job for every
        // usage declared by the graph. Different targets may run concurrently.
        std::vector<RequestId> RequestAll(ClientId client, uint64_t scope,
            const MaterialGraph& graph, Callback callback,
            EMaterialCompilePriority priority = EMaterialCompilePriority::Normal);

        // Invalidates queued or in-flight work for one logical scope. Compilation
        // already inside DXC is not interrupted; only its delivery is cancelled.
        void Cancel(ClientId client, uint64_t scope);

        [[nodiscard]] bool IsCurrent(ClientId client, uint64_t scope,
            const SMaterialShaderPermutation& permutation, RequestId request) const;
        // Aggregated state for every target in the logical scope.
        [[nodiscard]] EMaterialCompileState State(ClientId client, uint64_t scope) const;
        [[nodiscard]] EMaterialCompileState State(ClientId client, uint64_t scope,
            const SMaterialShaderPermutation& permutation) const;
        [[nodiscard]] size_t ActiveCount() const;
        [[nodiscard]] size_t QueuedCount() const;
        [[nodiscard]] size_t MaxParallelJobs() const { return m_MaxParallelJobs; }

      private:
        struct SKey
        {
            ClientId Client = 0;
            uint64_t Scope = 0;
            SMaterialShaderPermutation Permutation;

            bool operator==(const SKey&) const = default;
        };

        struct SKeyHash
        {
            size_t operator()(const SKey& key) const;
        };

        struct SJob
        {
            SKey Key;
            RequestId Id = 0;
            MaterialGraph Graph;
            Callback Completion;
            EMaterialCompilePriority Priority = EMaterialCompilePriority::Normal;
            bool Started = false;
            bool Cancelled = false;
        };

        struct SClient
        {
            bool Closing = false;
            size_t CallbacksInFlight = 0;
            std::condition_variable Idle;
        };

        using Job = std::shared_ptr<SJob>;

        static size_t DefaultMaxParallelJobs();
        std::vector<Job> TakeRunnableLocked();
        void Schedule(std::vector<Job> jobs);
        void Run(const Job& job);
        void FinishCallback(const Job& job);

        const size_t m_MaxParallelJobs;
        Scheduler m_Scheduler;
        Resolver m_Resolver;

        mutable std::mutex m_Mutex;
        ClientId m_NextClient = 1;
        RequestId m_NextRequest = 1;
        size_t m_ActiveJobs = 0;
        bool m_ShuttingDown = false;
        std::condition_variable m_Idle;
        std::unordered_map<ClientId, SClient> m_Clients;
        std::unordered_map<SKey, Job, SKeyHash> m_Latest;
        std::unordered_set<SKey, SKeyHash> m_ActiveKeys;
        std::array<std::deque<Job>, 3> m_Queues;
    };
}
