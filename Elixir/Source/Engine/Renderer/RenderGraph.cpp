#include "epch.h"
#include "RenderGraph.h"

#include <ranges>

namespace Elixir::Renderer
{
    RGResourceHandle RenderGraph::CreateBuffer(const SRGBufferDesc& desc)
    {
        const auto handle = RGResourceHandle(m_ResourcesCount++);

        auto buffer = SRGBuffer{};
        buffer.Handle = handle;
        buffer.Desc = std::move(desc);

        m_Buffers.insert(std::move(buffer));
        return handle;
    }

    RGResourceHandle RenderGraph::CreateTexture(const SRGTextureDesc& desc)
    {
        const auto handle = RGResourceHandle(m_ResourcesCount++);

        auto texture = SRGTexture{};
        texture.Handle = handle;
        texture.Desc = std::move(desc);

        m_Textures.insert(std::move(texture));
        return handle;
    }

    void RenderGraph::MarkExternalOutput(const RGResourceHandle handle)
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
        CullUnusedPasses();
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

        for (auto& p : m_Passes)
            p.Dependents.clear();

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

    void RenderGraph::CullUnusedPasses()
    {
        // Mark all resources that are interesting for graph
        // (is used as input by some node or is external).
        std::unordered_set<RGResourceHandle> interestingResources = m_ExternalOutputs;

        // Mark passes that generate or perform some operation on interesting resources.
        std::vector interestingPasses(m_Passes.size(), false);

        for (int i = m_Passes.size() - 1; i >= 0; --i)
        {
            auto& pass = m_Passes[i];
            bool interesting = false;

            for (const auto& output : pass.Outputs)
            {
                if (interestingResources.contains(output))
                {
                    interesting = true;
                    break;
                }
            }

            if (interesting)
            {
                interestingPasses[i] = true;

                for (const auto& input : pass.Inputs)
                {
                    interestingResources.insert(input);
                }

                for (const auto& output : pass.Outputs)
                {
                    interestingResources.insert(output);
                }
            }
        }

        for (auto i = 0; i < m_Passes.size(); ++i)
        {
            m_Passes[i].Culled = !interestingPasses[i];
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
        if (!pass || pass->Culled || pass->Executed.load())
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

                pass->Executed.store(true);

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