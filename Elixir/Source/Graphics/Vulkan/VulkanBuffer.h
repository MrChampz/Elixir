#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Elixir::Vulkan
{
    template <class>
    class VulkanBaseBuffer
    {
      public:
        virtual ~VulkanBaseBuffer() = default;

        virtual void Destroy();

        virtual void* Map();
        virtual void Unmap();

        virtual void Copy(
            const CommandBuffer* cmd,
            const Buffer* dst,
            std::span<SBufferCopy> regions = {}
        );

        VkBuffer GetVulkanBuffer() const { return m_Buffer; }
        const VmaAllocationInfo& GetVulkanAllocationInfo() const { return m_AllocationInfo; }

      protected:
        VulkanBaseBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);

        virtual void CreateBuffer(const SBufferCreateInfo& info);
        virtual void InitBufferWithData(const SBuffer& buffer);

        BufferAddress CreateAndReturnBufferAddress() const;

        VkBuffer m_Buffer;
        VmaAllocation m_Allocation;
        VmaAllocationInfo m_AllocationInfo;

        const VulkanGraphicsContext* m_GraphicsContext;
    };

    class ELIXIR_API VulkanBuffer final : public Buffer,
        public VulkanBaseBuffer<VulkanBuffer>
    {
        template <class> friend class VulkanBaseBuffer;
      public:
        VulkanBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);

        ~VulkanBuffer() override
        {
            EE_PROFILE_ZONE_SCOPED()
            Destroy();
        }

        void Destroy() override { VulkanBaseBuffer::Destroy(); }

        void Copy(
            const CommandBuffer* cmd,
            const Buffer* dst,
            const std::span<SBufferCopy> regions = {}
        ) override
        {
            VulkanBaseBuffer::Copy(cmd, dst, regions);
        }

      protected:
        void CreateBuffer(const SBufferCreateInfo& info) override
        {
            VulkanBaseBuffer::CreateBuffer(info);
        }

        void InitBufferWithData(const SBuffer& buffer) override
        {
            VulkanBaseBuffer::InitBufferWithData(buffer);
        }
    };

    class ELIXIR_API VulkanStagingBuffer final : public StagingBuffer,
        public VulkanBaseBuffer<VulkanStagingBuffer>
    {
        template <class> friend class VulkanBaseBuffer;
      public:
        VulkanStagingBuffer(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr
        );
        VulkanStagingBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);

        ~VulkanStagingBuffer() override
        {
            EE_PROFILE_ZONE_SCOPED()
            Destroy();
        }

        void Destroy() override { VulkanBaseBuffer::Destroy(); }

        void* Map() override { return VulkanBaseBuffer::Map(); }
        void Unmap() override { VulkanBaseBuffer::Unmap(); }

        void Copy(
            const CommandBuffer* cmd,
            const Buffer* dst,
            const std::span<SBufferCopy> regions = {}
        ) override
        {
            VulkanBaseBuffer::Copy(cmd, dst, regions);
        }

      protected:
        void CreateBuffer(const SBufferCreateInfo& info) override
        {
            VulkanBaseBuffer::CreateBuffer(info);
        }

        void InitBufferWithData(const SBuffer& buffer) override;
    };

    class ELIXIR_API VulkanVertexBuffer final : public VertexBuffer,
        public VulkanBaseBuffer<VulkanVertexBuffer>
    {
        template <class> friend class VulkanBaseBuffer;
      public:
        VulkanVertexBuffer(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr
        );
        VulkanVertexBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);

        ~VulkanVertexBuffer() override
        {
            EE_PROFILE_ZONE_SCOPED()
            Destroy();
        }

        void Destroy() override { VulkanBaseBuffer::Destroy(); }

        void Copy(
            const CommandBuffer* cmd,
            const Buffer* dst,
            const std::span<SBufferCopy> regions = {}
        ) override
        {
            VulkanBaseBuffer::Copy(cmd, dst, regions);
        }

      protected:
        void CreateBuffer(const SBufferCreateInfo& info) override
        {
            VulkanBaseBuffer::CreateBuffer(info);
        }

        void InitBufferWithData(const SBuffer& buffer) override
        {
            VulkanBaseBuffer::InitBufferWithData(buffer);
        }

        void CreateBufferAddress() override
        {
            m_Address = CreateAndReturnBufferAddress();
        }
    };

    class ELIXIR_API VulkanIndexBuffer final : public IndexBuffer,
        public VulkanBaseBuffer<VulkanIndexBuffer>
    {
        template <class> friend class VulkanBaseBuffer;
      public:
        VulkanIndexBuffer(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr,
            EIndexType type = EIndexType::UInt32
        );
        VulkanIndexBuffer(
            const GraphicsContext* context,
            const SBufferCreateInfo& info,
            EIndexType type = EIndexType::UInt32
        );
        
        ~VulkanIndexBuffer() override
        {
            EE_PROFILE_ZONE_SCOPED()
            Destroy();
        }

        void Destroy() override { VulkanBaseBuffer::Destroy(); }

        void Copy(
            const CommandBuffer* cmd,
            const Buffer* dst,
            const std::span<SBufferCopy> regions = {}
        ) override
        {
            VulkanBaseBuffer::Copy(cmd, dst, regions);
        }

      protected:
        void CreateBuffer(const SBufferCreateInfo& info) override
        {
            VulkanBaseBuffer::CreateBuffer(info);
        }

        void InitBufferWithData(const SBuffer& buffer) override
        {
            VulkanBaseBuffer::InitBufferWithData(buffer);
        }
    };
}