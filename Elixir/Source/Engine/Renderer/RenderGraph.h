#pragma once

#include <Engine/Core/Handle.h>
#include <Engine/Core/Executor.h>
#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/Texture.h>

#include <cstddef>
#include <functional>

namespace Elixir::Renderer
{
    typedef Handle<uint32_t> RGPassHandle;
    typedef Handle<uint32_t> RGResourceHandle;

    struct SRGBufferDesc
    {
        uint32_t Size = 0;
    };

    struct SRGBuffer
    {
        SRGBufferDesc Desc;
        RGResourceHandle Handle;
        Scope<Buffer> Buffer = nullptr;

        bool operator==(const SRGBuffer& other) const {
            return other.Handle == Handle;
        }
    };

    struct SRGTextureDesc
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
    };

    struct SRGTexture
    {
        SRGTextureDesc Desc;
        RGResourceHandle Handle;
        Scope<Texture> Texture = nullptr;

        bool operator==(const SRGTexture& other) const {
            return other.Handle == Handle;
        }
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
}

namespace std
{
    // TODO: Create a macro to define these std::hash definitions
    template<>
    struct std::hash<Renderer::SRGBuffer> {
        std::size_t operator()(const Renderer::SRGBuffer& buf) const noexcept {
            return Hash::HashValues(buf.Handle);
        }
    };

    template<>
    struct std::hash<Renderer::SRGTexture> {
        std::size_t operator()(const Renderer::SRGTexture& tex) const noexcept {
            return Hash::HashValues(tex.Handle);
        }
    };
}

namespace Elixir::Renderer
{
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

        RGResourceHandle CreateBuffer(const SRGBufferDesc& desc);
        RGResourceHandle CreateTexture(const SRGTextureDesc& desc);
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

        std::unordered_set<SRGBuffer> m_Buffers;
        std::unordered_set<SRGTexture> m_Textures;
        // TODO: make resources global handles, so a resource produced
        //       by one RG can be used by another without problems
        uint32_t m_ResourcesCount = 0;

        std::unordered_set<RGResourceHandle> m_ExternalOutputs;

        std::unordered_map<RGPassHandle, std::atomic<uint32_t>> m_UnresolvedDeps;

        bool m_Compiled = false;

        Executor* m_Executor = nullptr;
        Scope<WaitGroup> m_WaitGroup;
    };
}