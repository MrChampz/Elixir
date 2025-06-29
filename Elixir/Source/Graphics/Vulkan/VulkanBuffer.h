#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Elixir::Vulkan
{
    class VulkanBaseBuffer
    {
      public:
        virtual ~VulkanBaseBuffer() = default;

        virtual void Destroy();

        virtual void Copy(
            const CommandBuffer* cmd,
            const Buffer* dst,
            std::span<SBufferCopy> regions = {}
        );

        [[nodiscard]] virtual bool IsDestroyed() const { return m_Destroyed; }

        [[nodiscard]] VkBuffer GetVulkanBuffer() const { return m_Buffer; }
        [[nodiscard]] const VkDescriptorBufferInfo& GetVulkanDescriptorInfo() const { return m_DescriptorInfo; }

        VulkanBaseBuffer& operator=(const VulkanBaseBuffer&) = delete;
        VulkanBaseBuffer& operator=(VulkanBaseBuffer&&) = delete;

      protected:
        VulkanBaseBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        VulkanBaseBuffer(const VulkanBaseBuffer&) = delete;
        VulkanBaseBuffer(VulkanBaseBuffer&&) = delete;

        virtual void CreateBuffer(const SBufferCreateInfo& info);
        virtual void InitBufferWithData(const SBuffer& buffer);

        BufferAddress CreateAndReturnBufferAddress() const;

        VkBuffer m_Buffer = VK_NULL_HANDLE;
        VkDescriptorBufferInfo m_DescriptorInfo{};

        VmaAllocation m_Allocation = VK_NULL_HANDLE;

        bool m_Destroyed = false;

        const VulkanGraphicsContext* m_GraphicsContext;
    };

    class VulkanDynamicBuffer : public VulkanBaseBuffer
    {
      public:
        virtual void* Map();
        virtual void Unmap();

        VulkanDynamicBuffer& operator=(const VulkanDynamicBuffer&) = delete;
        VulkanDynamicBuffer& operator=(VulkanDynamicBuffer&&) = delete;

      protected:
        VulkanDynamicBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        VulkanDynamicBuffer(const VulkanDynamicBuffer&) = delete;
        VulkanDynamicBuffer(VulkanDynamicBuffer&&) = delete;
    };

    class ELIXIR_API VulkanBuffer final : public Buffer, public VulkanBaseBuffer
    {
      public:
        VulkanBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        ~VulkanBuffer() override;

        void Destroy() override { VulkanBaseBuffer::Destroy(); }

        void Copy(
            const CommandBuffer* cmd,
            const Buffer* dst,
            const std::span<SBufferCopy> regions = {}
        ) override
        {
            VulkanBaseBuffer::Copy(cmd, dst, regions);
        }

        [[nodiscard]] bool IsDestroyed() const override
        {
            return VulkanBaseBuffer::IsDestroyed();
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

    class ELIXIR_API VulkanStagingBuffer final : public StagingBuffer, public VulkanDynamicBuffer
    {
    public:
        VulkanStagingBuffer(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr
        );
        VulkanStagingBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        ~VulkanStagingBuffer() override;

        void Destroy() override { VulkanBaseBuffer::Destroy(); }

        void* Map() override { return VulkanDynamicBuffer::Map(); }
        void Unmap() override { VulkanDynamicBuffer::Unmap(); }

        void Copy(
            const CommandBuffer* cmd,
            const Buffer* dst,
            const std::span<SBufferCopy> regions = {}
        ) override
        {
            VulkanBaseBuffer::Copy(cmd, dst, regions);
        }

        [[nodiscard]] bool IsDestroyed() const override
        {
            return VulkanBaseBuffer::IsDestroyed();
        }

    protected:
        void CreateBuffer(const SBufferCreateInfo& info) override
        {
            VulkanBaseBuffer::CreateBuffer(info);
        }

        void InitBufferWithData(const SBuffer& buffer) override;
    };

    class ELIXIR_API VulkanVertexBuffer final : public VertexBuffer, public VulkanBaseBuffer
    {
      public:
        VulkanVertexBuffer(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr
        );
        VulkanVertexBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        ~VulkanVertexBuffer() override;

        void Destroy() override { VulkanBaseBuffer::Destroy(); }

        void Copy(
            const CommandBuffer* cmd,
            const Buffer* dst,
            const std::span<SBufferCopy> regions = {}
        ) override
        {
            VulkanBaseBuffer::Copy(cmd, dst, regions);
        }

        [[nodiscard]] bool IsDestroyed() const override
        {
            return VulkanBaseBuffer::IsDestroyed();
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

        void CreateBufferAddress() override;
    };

    class ELIXIR_API VulkanIndexBuffer final : public IndexBuffer, public VulkanBaseBuffer
    {
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
        ~VulkanIndexBuffer() override;

        void Destroy() override { VulkanBaseBuffer::Destroy(); }

        void Copy(
            const CommandBuffer* cmd,
            const Buffer* dst,
            const std::span<SBufferCopy> regions = {}
        ) override
        {
            VulkanBaseBuffer::Copy(cmd, dst, regions);
        }

        [[nodiscard]] bool IsDestroyed() const override
        {
            return VulkanBaseBuffer::IsDestroyed();
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

    class ELIXIR_API VulkanUniformBuffer final : public UniformBuffer, public VulkanDynamicBuffer
    {
    public:
        VulkanUniformBuffer(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr
        );
        VulkanUniformBuffer(
            const GraphicsContext* context,
            const SBufferCreateInfo& info
        );
        ~VulkanUniformBuffer() override;

        void Destroy() override { VulkanBaseBuffer::Destroy(); }

        void* Map() override { return VulkanDynamicBuffer::Map(); }
        void Unmap() override { VulkanDynamicBuffer::Unmap(); }

        void Copy(
            const CommandBuffer* cmd,
            const Buffer* dst,
            const std::span<SBufferCopy> regions = {}
        ) override
        {
            VulkanBaseBuffer::Copy(cmd, dst, regions);
        }

        [[nodiscard]] bool IsDestroyed() const override
        {
            return VulkanBaseBuffer::IsDestroyed();
        }

    protected:
        void CreateBuffer(const SBufferCreateInfo& info) override
        {
            VulkanBaseBuffer::CreateBuffer(info);
        }

        void InitBufferWithData(const SBuffer& buffer) override;
    };
}