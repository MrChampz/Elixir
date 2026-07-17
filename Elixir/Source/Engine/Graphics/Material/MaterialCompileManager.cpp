#include "epch.h"
#include "MaterialCompileManager.h"

#include <Engine/Core/Executor/Executor.h>

#include <algorithm>
#include <thread>

namespace Elixir
{
    namespace
    {
        size_t PriorityIndex(const EMaterialCompilePriority priority)
        {
            return static_cast<size_t>(priority);
        }
    }

    MaterialCompileManager::MaterialCompileManager(
        const size_t maxParallelJobs, Scheduler scheduler, Resolver resolver)
        : m_MaxParallelJobs(maxParallelJobs > 0 ? maxParallelJobs : DefaultMaxParallelJobs())
        , m_Scheduler(scheduler ? std::move(scheduler) : [](Task task)
        {
            Executor::Get().Enqueue(std::move(task));
        })
        , m_Resolver(resolver ? std::move(resolver) : [](const MaterialGraph& graph,
            const SMaterialShaderPermutation& permutation)
        {
            return MaterialShaderMap::Get().GetOrCompile(graph, permutation);
        })
    {
    }

    MaterialCompileManager::~MaterialCompileManager()
    {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_ShuttingDown = true;
        for (auto& [client, state] : m_Clients)
            state.Closing = true;
        for (auto& [key, job] : m_Latest)
            job->Cancelled = true;
        m_Latest.clear();
        m_Idle.wait(lock, [&]
        {
            if (m_ActiveJobs != 0)
                return false;
            for (const auto& [client, state] : m_Clients)
                if (state.CallbacksInFlight != 0)
                    return false;
            return true;
        });
    }

    MaterialCompileManager& MaterialCompileManager::Get()
    {
        static MaterialCompileManager manager;
        return manager;
    }

    MaterialCompileManager::ClientId MaterialCompileManager::CreateClient()
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        const ClientId client = m_NextClient++;
        m_Clients.try_emplace(client);
        return client;
    }

    void MaterialCompileManager::DestroyClient(const ClientId client)
    {
        std::vector<Job> runnable;
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            const auto owner = m_Clients.find(client);
            if (owner == m_Clients.end())
                return;

            owner->second.Closing = true;
            for (auto request = m_Latest.begin(); request != m_Latest.end();)
            {
                if (request->first.Client == client)
                {
                    request->second->Cancelled = true;
                    request = m_Latest.erase(request);
                }
                else
                    ++request;
            }
            runnable = TakeRunnableLocked();
        }
        Schedule(std::move(runnable));

        std::unique_lock<std::mutex> lock(m_Mutex);
        const auto owner = m_Clients.find(client);
        if (owner == m_Clients.end())
            return;
        SClient* clientState = &owner->second;
        clientState->Idle.wait(lock, [&] { return clientState->CallbacksInFlight == 0; });
        m_Clients.erase(client);
    }

    MaterialCompileManager::RequestId MaterialCompileManager::Request(const ClientId client,
        const uint64_t scope, MaterialGraph graph, SMaterialShaderPermutation permutation,
        Callback callback, const EMaterialCompilePriority priority)
    {
        std::vector<Job> runnable;
        RequestId requestId = 0;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            const auto owner = m_Clients.find(client);
            if (m_ShuttingDown || owner == m_Clients.end() || owner->second.Closing)
                return 0;

            Job job = std::make_shared<SJob>();
            job->Key = { client, scope, permutation };
            job->Id = m_NextRequest++;
            job->Graph = std::move(graph);
            job->Completion = std::move(callback);
            job->Priority = priority;
            requestId = job->Id;

            if (const auto previous = m_Latest.find(job->Key); previous != m_Latest.end())
                previous->second->Cancelled = true;
            m_Latest[job->Key] = job;
            m_Queues[PriorityIndex(priority)].push_back(job);
            runnable = TakeRunnableLocked();
        }
        Schedule(std::move(runnable));
        return requestId;
    }

    std::vector<MaterialCompileManager::RequestId> MaterialCompileManager::RequestAll(
        const ClientId client, const uint64_t scope, const MaterialGraph& graph,
        Callback callback, const EMaterialCompilePriority priority)
    {
        Cancel(client, scope);
        const auto permutations = MaterialShaderPermutation::Expand(
            graph.GetDomain(), graph.GetUsages());
        std::vector<RequestId> requests;
        requests.reserve(permutations.size());
        for (const auto& permutation : permutations)
        {
            const RequestId request = Request(
                client, scope, graph, permutation, callback, priority);
            if (request != 0)
                requests.push_back(request);
        }
        return requests;
    }

    void MaterialCompileManager::Cancel(const ClientId client, const uint64_t scope)
    {
        std::vector<Job> runnable;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            for (auto request = m_Latest.begin(); request != m_Latest.end();)
            {
                if (request->first.Client == client && request->first.Scope == scope)
                {
                    request->second->Cancelled = true;
                    request = m_Latest.erase(request);
                }
                else
                    ++request;
            }
            runnable = TakeRunnableLocked();
        }
        Schedule(std::move(runnable));
    }

    bool MaterialCompileManager::IsCurrent(
        const ClientId client, const uint64_t scope,
        const SMaterialShaderPermutation& permutation, const RequestId requestId) const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        const auto request = m_Latest.find({ client, scope, permutation });
        return request != m_Latest.end() && request->second->Id == requestId
            && !request->second->Cancelled;
    }

    EMaterialCompileState MaterialCompileManager::State(
        const ClientId client, const uint64_t scope) const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        EMaterialCompileState state = EMaterialCompileState::Idle;
        for (const auto& [key, request] : m_Latest)
        {
            if (key.Client != client || key.Scope != scope)
                continue;
            if (request->Started)
                return EMaterialCompileState::Compiling;
            state = EMaterialCompileState::Queued;
        }
        return state;
    }

    EMaterialCompileState MaterialCompileManager::State(
        const ClientId client, const uint64_t scope,
        const SMaterialShaderPermutation& permutation) const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        const auto request = m_Latest.find({ client, scope, permutation });
        if (request == m_Latest.end())
            return EMaterialCompileState::Idle;
        return request->second->Started
            ? EMaterialCompileState::Compiling
            : EMaterialCompileState::Queued;
    }

    size_t MaterialCompileManager::ActiveCount() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_ActiveJobs;
    }

    size_t MaterialCompileManager::QueuedCount() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        size_t queued = 0;
        for (const auto& [key, job] : m_Latest)
            if (!job->Started)
                ++queued;
        return queued;
    }

    size_t MaterialCompileManager::SKeyHash::operator()(const SKey& key) const
    {
        const size_t client = std::hash<ClientId>{}(key.Client);
        const size_t scope = std::hash<uint64_t>{}(key.Scope);
        size_t hash = client ^ (scope + 0x9e3779b97f4a7c15ull + (client << 6u) + (client >> 2u));
        const size_t permutation = SMaterialShaderPermutationHash{}(key.Permutation);
        return hash ^ (permutation + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u));
    }

    size_t MaterialCompileManager::DefaultMaxParallelJobs()
    {
        const size_t hardware = std::thread::hardware_concurrency();
        if (hardware <= 2)
            return 1;
        return std::min<size_t>(4, hardware - 2);
    }

    std::vector<MaterialCompileManager::Job> MaterialCompileManager::TakeRunnableLocked()
    {
        std::vector<Job> runnable;
        while (m_ActiveJobs < m_MaxParallelJobs)
        {
            Job job;
            for (size_t priority = m_Queues.size(); priority-- > 0;)
            {
                auto& queue = m_Queues[priority];
                for (auto candidateIt = queue.begin(); candidateIt != queue.end();)
                {
                    Job candidate = *candidateIt;
                    const auto latest = m_Latest.find(candidate->Key);
                    const auto owner = m_Clients.find(candidate->Key.Client);
                    if (candidate->Cancelled || latest == m_Latest.end()
                        || latest->second != candidate || owner == m_Clients.end()
                        || owner->second.Closing)
                    {
                        candidateIt = queue.erase(candidateIt);
                        continue;
                    }
                    if (!m_ActiveKeys.contains(candidate->Key))
                    {
                        job = std::move(candidate);
                        queue.erase(candidateIt);
                        break;
                    }
                    ++candidateIt;
                }
                if (job)
                    break;
            }
            if (!job)
                break;

            job->Started = true;
            ++m_ActiveJobs;
            m_ActiveKeys.insert(job->Key);
            runnable.push_back(std::move(job));
        }
        return runnable;
    }

    void MaterialCompileManager::Schedule(std::vector<Job> jobs)
    {
        for (const Job& job : jobs)
            m_Scheduler([this, job] { Run(job); });
    }

    void MaterialCompileManager::Run(const Job& job)
    {
        std::optional<MaterialCompiler::SCompiled> compiled;
        try
        {
            compiled = m_Resolver(job->Graph, job->Key.Permutation);
        }
        catch (const std::exception& error)
        {
            EE_CORE_ERROR("MaterialCompileManager resolver failed: {}", error.what())
        }
        catch (...)
        {
            EE_CORE_ERROR("MaterialCompileManager resolver failed with an unknown exception.")
        }

        Callback callback;
        std::vector<Job> runnable;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            EE_CORE_ASSERT(m_ActiveJobs > 0, "MaterialCompileManager active-job underflow.")
            --m_ActiveJobs;
            // Released outside the assert: EE_CORE_ASSERT drops its argument entirely
            // outside Debug, and leaving the key registered would gate this scope out of
            // TakeRunnableLocked forever.
            [[maybe_unused]] const size_t released = m_ActiveKeys.erase(job->Key);
            EE_CORE_ASSERT(released == 1, "MaterialCompileManager active scope was not registered.")

            const auto latest = m_Latest.find(job->Key);
            const auto owner = m_Clients.find(job->Key.Client);
            if (!job->Cancelled && latest != m_Latest.end() && latest->second == job
                && owner != m_Clients.end() && !owner->second.Closing)
            {
                callback = job->Completion;
                if (callback)
                    ++owner->second.CallbacksInFlight;
                else
                    m_Latest.erase(latest);
            }
            runnable = TakeRunnableLocked();
            if (m_ActiveJobs == 0)
                m_Idle.notify_all();
        }

        // Start the next DXC jobs before running the renderer-local continuation.
        // Pipeline creation can then overlap unrelated shader compilation while the
        // backend keeps its own resource-build synchronization.
        Schedule(std::move(runnable));

        if (callback)
        {
            try
            {
                callback({ job->Id, job->Key.Permutation, std::move(compiled) });
            }
            catch (const std::exception& error)
            {
                EE_CORE_ERROR("MaterialCompileManager callback failed: {}", error.what())
            }
            catch (...)
            {
                EE_CORE_ERROR("MaterialCompileManager callback failed with an unknown exception.")
            }
            FinishCallback(job);
        }
    }

    void MaterialCompileManager::FinishCallback(const Job& job)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        const auto latest = m_Latest.find(job->Key);
        if (latest != m_Latest.end() && latest->second == job)
            m_Latest.erase(latest);

        const auto owner = m_Clients.find(job->Key.Client);
        if (owner == m_Clients.end())
            return;
        EE_CORE_ASSERT(owner->second.CallbacksInFlight > 0,
            "MaterialCompileManager callback underflow.")
        --owner->second.CallbacksInFlight;
        if (owner->second.CallbacksInFlight == 0)
        {
            owner->second.Idle.notify_all();
            m_Idle.notify_all();
        }
    }
}
