#pragma once

#ifdef EE_PLATFORM_WINDOWS

#include <Engine/Graphics/Buffer.h>
#include <Graphics/D3D12/D3D12GraphicsContext.h>

namespace Elixir::D3D12
{
    ID3D12Resource* TryToGetD3D12Buffer(const Buffer* buffer);

    template <typename Base>
    class D3D12BaseBuffer : public Base
    {
      public:
        template <typename... Args>
        D3D12BaseBuffer(const GraphicsContext* context, const SBufferCreateInfo& info, Args&&... args);
        ~D3D12BaseBuffer() override = default;

        void Fill(
            const Ref<CommandBuffer>& cmd,
            uint32_t data,
            int32_t offset = 0,
            size_t size = 0
        ) override;
        void Destroy() override;
        void Barrier(const CommandBuffer* cmd, EPipelineStage stage, EPipelineAccess access) override;
        void Copy(
            const CommandBuffer* cmd,
            const Buffer* dst,
            std::span<SBufferCopy> regions = {}
        ) override;

        bool IsValid() const override { return m_Resource != nullptr; }

        ID3D12Resource* GetD3D12Resource() const { return m_Resource.Get(); }
        D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const;
        D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView(uint64_t offset = 0) const;
        D3D12_INDEX_BUFFER_VIEW GetIndexBufferView(EIndexType type, uint64_t offset = 0) const;

        void CreateConstantBufferView(D3D12_CPU_DESCRIPTOR_HANDLE destination) const;
        void CreateShaderResourceView(
            D3D12_CPU_DESCRIPTOR_HANDLE destination,
            UINT structureStride = 0
        ) const;
        void CreateUnorderedAccessView(
            D3D12_CPU_DESCRIPTOR_HANDLE destination,
            UINT structureStride = 0
        ) const;

      protected:
        void CreateBuffer(const SBufferCreateInfo& info);
        void InitBuffer(const SBuffer& buffer);
        void UploadInitialData(const SBuffer& buffer);

        ComPtr<ID3D12Resource> m_Resource;
        D3D12_RESOURCE_STATES m_State = D3D12_RESOURCE_STATE_COMMON;
        const D3D12GraphicsContext* m_GraphicsContext = nullptr;
    };

    template <typename Base>
    class D3D12DynamicBuffer : public D3D12BaseBuffer<Base>
    {
      public:
        template <typename... Args>
        D3D12DynamicBuffer(const GraphicsContext* context, const SBufferCreateInfo& info, Args&&... args);
        ~D3D12DynamicBuffer() override = default;

        void* Map() override;
        void Unmap() override;

      protected:
        void InitBuffer(const SBuffer& buffer);
    };

    class ELIXIR_API D3D12Buffer final : public D3D12BaseBuffer<Buffer>
    {
      public:
        D3D12Buffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        ~D3D12Buffer() override;
    };

    class ELIXIR_API D3D12StagingBuffer final : public D3D12DynamicBuffer<StagingBuffer>
    {
      public:
        D3D12StagingBuffer(const GraphicsContext* context, size_t size, const void* data = nullptr);
        D3D12StagingBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        ~D3D12StagingBuffer() override;
    };

    class ELIXIR_API D3D12VertexBuffer final : public D3D12BaseBuffer<VertexBuffer>
    {
      public:
        D3D12VertexBuffer(const GraphicsContext* context, size_t size, const void* data = nullptr);
        D3D12VertexBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        ~D3D12VertexBuffer() override;

      private:
        void CreateBufferAddress() override;
    };

    class ELIXIR_API D3D12DynamicVertexBuffer final
        : public D3D12DynamicBuffer<DynamicVertexBuffer>
    {
      public:
        D3D12DynamicVertexBuffer(const GraphicsContext* context, size_t size, const void* data = nullptr);
        D3D12DynamicVertexBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        ~D3D12DynamicVertexBuffer() override;

      private:
        void CreateBufferAddress() override;
    };

    class ELIXIR_API D3D12IndexBuffer final : public D3D12BaseBuffer<IndexBuffer>
    {
      public:
        D3D12IndexBuffer(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr,
            EIndexType type = EIndexType::UInt32
        );
        D3D12IndexBuffer(
            const GraphicsContext* context,
            const SBufferCreateInfo& info,
            EIndexType type = EIndexType::UInt32
        );
        ~D3D12IndexBuffer() override;
    };

    class ELIXIR_API D3D12DynamicIndexBuffer final
        : public D3D12DynamicBuffer<DynamicIndexBuffer>
    {
      public:
        D3D12DynamicIndexBuffer(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr,
            EIndexType type = EIndexType::UInt32
        );
        D3D12DynamicIndexBuffer(
            const GraphicsContext* context,
            const SBufferCreateInfo& info,
            EIndexType type = EIndexType::UInt32
        );
        ~D3D12DynamicIndexBuffer() override;
    };

    class ELIXIR_API D3D12StorageBuffer final : public D3D12BaseBuffer<StorageBuffer>
    {
      public:
        D3D12StorageBuffer(const GraphicsContext* context, size_t size, const void* data = nullptr);
        D3D12StorageBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        ~D3D12StorageBuffer() override;

      private:
        D3D12StorageBuffer(
            const GraphicsContext* context,
            const SBufferCreateInfo& info,
            bool uploadInitialData
        );
    };

    class ELIXIR_API D3D12DynamicStorageBuffer final
        : public D3D12DynamicBuffer<DynamicStorageBuffer>
    {
      public:
        D3D12DynamicStorageBuffer(const GraphicsContext* context, size_t size, const void* data = nullptr);
        D3D12DynamicStorageBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        ~D3D12DynamicStorageBuffer() override;
    };

    class ELIXIR_API D3D12UniformBuffer final : public D3D12DynamicBuffer<UniformBuffer>
    {
      public:
        D3D12UniformBuffer(const GraphicsContext* context, size_t size, const void* data = nullptr);
        D3D12UniformBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        ~D3D12UniformBuffer() override;
    };
}

#endif
