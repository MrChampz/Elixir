#include "epch.h"
#include "RenderGraph.h"

#include <ranges>

namespace Elixir::Renderer
{
    static uint32_t g_ResourceCount = 0;

    RGResourceHandle RenderGraph::CreateResource(const SRGResourceDesc& desc)
    {
        const auto handle = RGResourceHandle(g_ResourceCount++);
        //m_Resources[handle] = desc;
        return handle;
    }

    void RenderGraph::MarkExternalOutput(RGResourceHandle handle)
    {
        m_ExternalOutputs.insert(handle);
    }

    void RenderGraph::AddPass(
        const std::string& name,
        const std::vector<RGResourceHandle>& inputs,
        const std::vector<RGResourceHandle>& outputs,
        const std::function<void()>& callback
    )
    {
        auto pass = SRGPass{};
        pass.Handle = RGPassHandle(m_Passes.size());
        pass.Name = name;
        pass.Inputs = std::move(inputs);
        pass.Outputs = std::move(outputs);
        pass.ExecuteCallback = callback;

        m_Passes.push_back(std::move(pass));
    }

    void RenderGraph::Compile()
    {
        GeneratePassLookup();
        BuildDependencies();
        SortPassesTopologically();
        m_Compiled = true;
    }

    void RenderGraph::Execute()
    {
        if (!IsCompiled())
        {
            throw std::runtime_error("Render graph is not compiled!");
        }

        for (auto& pass : m_Passes)
        {
            if (m_UnresolvedDeps[pass.Handle].load() == 0)
            {
                ExecutePass(pass.Handle);
            }
        }

        // Wait for all tasks to finish.
        m_WaitGroup->Wait();
    }

    void RenderGraph::GeneratePassLookup()
    {
        for (auto& pass : m_Passes)
        {
            m_PassLookup[pass.Handle] = &pass;
        }
    }

    void RenderGraph::BuildDependencies()
    {
        std::unordered_map<RGResourceHandle, RGPassHandle> producers;

        for (auto& p : m_Passes) p.Dependents.clear();

        for (const auto& pass : m_Passes)
        {
            for (const auto& output : pass.Outputs)
            {
                producers[output] = pass.Handle;
            }
        }

        for (auto& pass : m_Passes)
        {
            uint32_t deps = 0;
            for (const auto& input : pass.Inputs)
            {
                if (auto it = producers.find(input); it != producers.end())
                {
                    ++deps;

                    const auto producerHandle = it->second;
                    if (const auto producer = FindPass(producerHandle))
                    {
                        producer->Dependents.push_back(pass.Handle);
                    }
                }
            }

            m_UnresolvedDeps[pass.Handle].store(deps);
        }
    }

    void RenderGraph::SortPassesTopologically()
    {
        std::unordered_map<RGPassHandle, int> inDegree(m_Passes.size());
        std::unordered_map<RGPassHandle, const SRGPass*> passes(m_Passes.size());

        // Count how many non-culled passes we should have
        size_t nonCulledCount = 0;
        for (const auto& p : m_Passes)
            if (!p.Culled)
                ++nonCulledCount;

        for (const auto& pass : m_Passes)
        {
            passes[pass.Handle] = &pass;
            if (pass.Culled)
                continue;
            inDegree[pass.Handle] = m_UnresolvedDeps[pass.Handle].load();
        }

        std::queue<const SRGPass*> queue;
        for (const auto& [handle, degree] : inDegree)
        {
            if (degree == 0)
            {
                queue.push(passes[handle]);
            }
        }

        std::vector<RGPassHandle> sorted;
        sorted.reserve(nonCulledCount);

        while (!queue.empty())
        {
            const auto pass = queue.front();
            queue.pop();
            if (pass->Culled)
                continue;

            sorted.push_back(pass->Handle);

            for (const auto handle : pass->Dependents)
            {
                const auto dependent = FindPass(handle);
                if (!dependent || dependent->Culled)
                    continue;

                auto it = inDegree.find(handle);
                if (it == inDegree.end())
                    continue;

                --(it->second);

                if (it->second == 0)
                {
                    queue.push(dependent);
                }
            }
        }

        if (sorted.size() != nonCulledCount)
        {
            throw new std::runtime_error("Cycle detected in render graph!");
        }

        ReorderPasses(sorted);
    }

    void RenderGraph::ReorderPasses(const std::vector<RGPassHandle>& passes)
    {
        std::vector<SRGPass> sorted;
        sorted.reserve(passes.size());

        for (const auto handle : passes)
        {
            auto* pass = FindPass(handle);
            EE_CORE_ASSERT(pass, "Pass handle not found!")
            sorted.push_back(std::move(*pass));
        }

        m_Passes = std::move(sorted);

        GeneratePassLookup();
    }

    void RenderGraph::ExecutePass(const RGPassHandle handle)
    {
        const auto* pass = FindPass(handle);
        if (!pass || pass->Culled || pass->Executed)
            return;

        struct Payload { RenderGraph* rg; RGPassHandle handle; };
        auto* payload = new Payload{ this, handle };

        // Run async task
        const auto task = Task{
            [](ftl::TaskScheduler*, void* p)
            {
                const auto* payload = static_cast<Payload*>(p);
                auto* rg = payload->rg;
                const auto handle = payload->handle;

                auto* pass = rg->FindPass(handle);
                if (!pass) { delete payload; return; }

                if (pass->ExecuteCallback)
                    pass->ExecuteCallback();

                pass->Executed = true;

                for (const auto dependent : pass->Dependents)
                {
                    if (rg->m_UnresolvedDeps[dependent].fetch_sub(1) == 1)
                    {
                        rg->ExecutePass(dependent);
                    }
                }

                delete payload;
            },
            payload
        };

        m_Executor->AddTask(task, ETaskPriority::Normal, m_WaitGroup.get());
    }

    SRGPass* RenderGraph::FindPass(const RGPassHandle handle)
    {
        return m_PassLookup[handle];
    }
}