#pragma once

#include <Engine/Core/Handle.h>
#include <Engine/Core/Executor.h>

namespace Elixir::Renderer
{
    typedef Handle<uint32_t> RGPassHandle;
    typedef Handle<uint32_t> RGResourceHandle;

    enum class ERGResourceType
    {
        Texture,
        Buffer
    };

    enum class ERGResourceUsage
    {
        Read,
        Write
    };

    struct SRGResourceDesc
    {
        ERGResourceType Type;
        uint32_t Width = 0;
        uint32_t Height = 0;
    };

    struct SRGResource
    {
        RGResourceHandle Handle;
        SRGResourceDesc Desc;
        std::vector<ERGResourceUsage> Usage;
    };

    struct SRGPass
    {
        RGPassHandle Handle;
        std::string Name;
        std::vector<RGResourceHandle> Inputs;
        std::vector<RGResourceHandle> Outputs;

        // TODO: Pass CommandList as param to callback.
        std::function<void()> ExecuteCallback;

        std::vector<RGPassHandle> Dependents;

        bool Culled = false;
        std::atomic<bool> Executed = false;

        SRGPass() = default;

        SRGPass(const SRGPass& other)
            : Handle(other.Handle),
              Name(other.Name),
              Inputs(other.Inputs),
              Outputs(other.Outputs),
              ExecuteCallback(other.ExecuteCallback),
              Dependents(other.Dependents),
              Culled(other.Culled),
              Executed(other.Executed.load())
        {}

        SRGPass(SRGPass&& other) noexcept
            : Handle(std::move(other.Handle)),
              Name(std::move(other.Name)),
              Inputs(std::move(other.Inputs)),
              Outputs(std::move(other.Outputs)),
              ExecuteCallback(std::move(other.ExecuteCallback)),
              Dependents(std::move(other.Dependents)),
              Culled(other.Culled),
              Executed(other.Executed.load())
        {}

        SRGPass& operator=(const SRGPass& other)
        {
            Handle = other.Handle;
            Name = other.Name;
            Inputs = other.Inputs;
            Outputs = other.Outputs;
            ExecuteCallback = other.ExecuteCallback;
            Dependents = other.Dependents;
            Culled = other.Culled;
            Executed.store(other.Executed.load());
            return *this;
        }

        SRGPass& operator=(SRGPass&& other) noexcept
        {
            Handle = std::move(other.Handle);
            Name = std::move(other.Name);
            Inputs = std::move(other.Inputs);
            Outputs = std::move(other.Outputs);
            ExecuteCallback = std::move(other.ExecuteCallback);
            Dependents = std::move(other.Dependents);
            Culled = other.Culled;
            Executed.store(other.Executed.load());
            return *this;
        }
    };

    class ELIXIR_API RenderGraph
    {
      public:
        explicit RenderGraph(Executor* executor) : m_Executor(executor)
        {
            EE_PROFILE_ZONE_SCOPED()
            m_WaitGroup = CreateScope<WaitGroup>(executor);
        }

        ~RenderGraph()
        {
            EE_PROFILE_ZONE_SCOPED()
        }

        RGResourceHandle CreateResource(const SRGResourceDesc& desc);
        void MarkExternalOutput(RGResourceHandle handle);

        void AddPass(
            const std::string& name,
            const std::vector<RGResourceHandle>& inputs,
            const std::vector<RGResourceHandle>& outputs,
            const std::function<void()>& callback
        );

        void Compile();
        void Execute();

        const std::vector<SRGPass>& GetPasses() const { return m_Passes; }
        bool IsCompiled() const { return m_Compiled; }

      private:
        void GeneratePassLookup();
        void BuildDependencies();
        void CullUnusedPasses();
        void SortPassesTopologically();
        void ReorderPasses(const std::vector<RGPassHandle>& passes);
        void ExecutePass(RGPassHandle handle);

        SRGPass* FindPass(RGPassHandle handle);

        std::vector<SRGPass> m_Passes;
        std::unordered_map<RGPassHandle, SRGPass*> m_PassLookup;
        //std::unordered_set<SRGResource> m_Resources;

        std::unordered_set<RGResourceHandle> m_ExternalOutputs;

        std::unordered_map<RGPassHandle, std::atomic<uint32_t>> m_UnresolvedDeps;

        bool m_Compiled = false;

        Executor* m_Executor = nullptr;
        Scope<WaitGroup> m_WaitGroup;
    };
}
