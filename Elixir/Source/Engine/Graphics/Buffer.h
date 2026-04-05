#pragma once

#include <Engine/Core/Buffer.h>
#include <Engine/Graphics/BufferLayout.h>
#include <Engine/Graphics/GraphicsContext.h>
#include <Engine/Graphics/Memory.h>

namespace Elixir
{
    typedef uint64_t BufferAddress;

    enum class EBufferUsage : uint32_t
    {
        TransferSrc				= 0x00000001,
        TransferDst				= 0x00000002,
        UniformTexelBuffer	    = 0x00000004,
        StorageTexelBuffer		= 0x00000008,
        UniformBuffer			= 0x00000010,
        StorageBuffer	        = 0x00000020,
        IndexBuffer		        = 0x00000040,
        VertexBuffer			= 0x00000080,
        IndirectBuffer			= 0x00000100,
        ShaderDeviceAddress		= 0x00020000
    };

    GENERATE_ENUM_CLASS_OPERATORS(EBufferUsage)

    struct SBufferCopy
    {
        size_t SrcOffset = 0;
        size_t DstOffset = 0;
        size_t Size = 0;

        static SBufferCopy Default(const size_t size = 0)
        {
            return { .Size = size };
        }
    };

    struct SBufferCreateInfo
    {
        SBuffer Buffer;
        EBufferUsage Usage;
        SAllocationInfo AllocationInfo;
    };

    class ELIXIR_API Buffer
    {
      public:
        virtual ~Buffer() = default;

        virtual void Destroy() = 0;

        virtual void Copy(
            const Ref<CommandBuffer>& cmd,
            const Ref<Buffer>& dst,
            std::span<SBufferCopy> regions = {}
        );
        virtual void Copy(
            const Ref<CommandBuffer>& cmd,
            const Buffer* dst,
            std::span<SBufferCopy> regions = {}
        );
        virtual void Copy(
            const CommandBuffer* cmd,
            const Ref<Buffer>& dst,
            std::span<SBufferCopy> regions = {}
        );
        virtual void Copy(
            const CommandBuffer* cmd,
            const Buffer* dst,
            std::span<SBufferCopy> regions = {}
        ) = 0;

        [[nodiscard]] virtual bool IsValid() const = 0;

        [[nodiscard]] const UUID& GetUUID() const { return m_UUID; }
        [[nodiscard]] EBufferUsage GetUsage() const { return m_Usage; }

        /**
         * Returns the buffer size in bytes.
         * @return The buffer size in bytes.
         */
        [[nodiscard]] size_t GetSize() const { return m_Size; }

        [[nodiscard]] const SAllocationInfo& GetAllocationInfo() const { return m_AllocationInfo; }

        bool operator==(const Buffer& other) const
        {
            return m_UUID == other.m_UUID;
        }

        Buffer& operator=(const Buffer&) = delete;
        Buffer& operator=(Buffer&&) = delete;

        static Ref<Buffer> Create(
            const GraphicsContext* context,
            const SBufferCreateInfo& info
        );

      protected:
        Buffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        Buffer(const Buffer&) = delete;
        Buffer(Buffer&&) = delete;

        UUID m_UUID;
        std::string m_DebugName;

        EBufferUsage m_Usage;
        size_t m_Size;

        SAllocationInfo m_AllocationInfo;

        const GraphicsContext* m_GraphicsContext;
    };

    class ELIXIR_API DynamicBuffer : public Buffer
    {
      public:
        ~DynamicBuffer() override = default;

        virtual void* Map() = 0;
        virtual void Unmap() = 0;

      protected:
        DynamicBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        DynamicBuffer(const DynamicBuffer&) = delete;

        void* m_PersistentMapping = nullptr;
    };

    class ELIXIR_API StagingBuffer : public DynamicBuffer
    {
    public:
        ~StagingBuffer() override = default;

        StagingBuffer& operator=(const StagingBuffer&) = delete;

        static Ref<StagingBuffer> Create(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr
        );

        static SBufferCreateInfo CreateBufferInfo(size_t size, const void* data);

    protected:
        StagingBuffer(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr
        );
        StagingBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        StagingBuffer(const StagingBuffer&) = delete;
    };

    class ELIXIR_API VertexBuffer : public Buffer
    {
      public:
        ~VertexBuffer() override = default;

        void Bind(
            const Ref<CommandBuffer>& cmd,
            uint32_t bindingCount = 1,
            uint32_t firstBinding = 0
        ) const;

        [[nodiscard]] const BufferLayout& GetLayout() const { return m_Layout; }
        void SetLayout(const BufferLayout& layout) { m_Layout = layout; }

        [[nodiscard]] BufferAddress GetAddress() const { return m_Address; }

        static Ref<VertexBuffer> Create(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr
        );

        static SBufferCreateInfo CreateBufferInfo(size_t size, const void* data);

      protected:
        VertexBuffer(const GraphicsContext* context, size_t size, const void* data = nullptr);
        VertexBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        VertexBuffer(const VertexBuffer&) = delete;

        virtual void CreateBufferAddress() = 0;

        BufferLayout m_Layout;
        BufferAddress m_Address;
    };

    class ELIXIR_API DynamicVertexBuffer : public DynamicBuffer
    {
    public:
        ~DynamicVertexBuffer() override = default;

        void Bind(
            const Ref<CommandBuffer>& cmd,
            uint32_t bindingCount = 1,
            uint32_t firstBinding = 0
        ) const;

        void UpdateData(const void* data, size_t size, size_t offset = 0) const;

        [[nodiscard]] const BufferLayout& GetLayout() const { return m_Layout; }
        void SetLayout(const BufferLayout& layout) { m_Layout = layout; }

        [[nodiscard]] BufferAddress GetAddress() const { return m_Address; }

        static Ref<DynamicVertexBuffer> Create(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr
        );

        static SBufferCreateInfo CreateBufferInfo(size_t size, const void* data);

    protected:
        DynamicVertexBuffer(const GraphicsContext* context, size_t size, const void* data = nullptr);
        DynamicVertexBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        DynamicVertexBuffer(const DynamicVertexBuffer&) = delete;

        virtual void CreateBufferAddress() = 0;

        BufferLayout m_Layout;
        BufferAddress m_Address;
    };

    enum class EIndexType
    {
        UInt16, UInt32
    };

    class ELIXIR_API IndexBuffer : public Buffer
    {
      public:
        ~IndexBuffer() override = default;

        void Bind(const Ref<CommandBuffer>& cmd) const;

        [[nodiscard]] EIndexType GetIndexType() const { return m_IndexType; }

        static Ref<IndexBuffer> Create(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr,
            EIndexType type = EIndexType::UInt32
        );

        static SBufferCreateInfo CreateBufferInfo(size_t size, const void* data);

      protected:
        IndexBuffer(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr,
            EIndexType type = EIndexType::UInt32
        );
        IndexBuffer(
            const GraphicsContext* context,
            const SBufferCreateInfo& info,
            EIndexType type = EIndexType::UInt32
        );
        IndexBuffer(const IndexBuffer&) = delete;

        EIndexType m_IndexType;
    };

    class ELIXIR_API DynamicIndexBuffer : public DynamicBuffer
    {
    public:
        ~DynamicIndexBuffer() override = default;

        void Bind(const Ref<CommandBuffer>& cmd) const;

        void UpdateData(const void* data, size_t size, size_t offset = 0) const;

        [[nodiscard]] EIndexType GetIndexType() const { return m_IndexType; }

        static Ref<DynamicIndexBuffer> Create(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr,
            EIndexType type = EIndexType::UInt32
        );

        static SBufferCreateInfo CreateBufferInfo(size_t size, const void* data);

    protected:
        DynamicIndexBuffer(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr,
            EIndexType type = EIndexType::UInt32
        );
        DynamicIndexBuffer(
            const GraphicsContext* context,
            const SBufferCreateInfo& info,
            EIndexType type = EIndexType::UInt32
        );
        DynamicIndexBuffer(const IndexBuffer&) = delete;

        EIndexType m_IndexType;
    };

    class ELIXIR_API UniformBuffer : public DynamicBuffer
    {
    public:
        ~UniformBuffer() override = default;

        void UpdateData(const void* data, size_t size, size_t offset = 0);

        static Ref<UniformBuffer> Create(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr
        );

        static SBufferCreateInfo CreateBufferInfo(size_t size, const void* data);

    protected:
        UniformBuffer(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr
        );
        UniformBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
    };

    class ELIXIR_API PushConstantBuffer
    {
    public:
        PushConstantBuffer(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr
        );

        virtual ~PushConstantBuffer() = default;

        virtual void* Map();
        virtual void Unmap(uint32_t size) {}
        virtual void Unmap(uint32_t offset, uint32_t size) {}

        const UUID& GetUUID() const { return m_UUID; }
        virtual SBuffer& GetBuffer() { return m_Buffer; }
        virtual const SBuffer& GetBuffer() const { return m_Buffer; }
        virtual uint32_t GetSize() const { return m_Buffer.Size; }

        bool operator==(const PushConstantBuffer& other) const
        {
            return m_UUID == other.m_UUID;
        }

        static Ref<PushConstantBuffer> Create(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr
        );

    protected:
        UUID m_UUID;
        std::string m_DebugName;

        SBuffer m_Buffer;

        const GraphicsContext* m_GraphicsContext;
    };
}