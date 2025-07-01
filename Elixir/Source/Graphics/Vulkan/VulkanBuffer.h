#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Elixir::Vulkan
{
    VkBuffer TryToGetVulkanBuffer(const Buffer* buffer);

    template <class Base>
    class VulkanBaseBuffer : public Base
    {
      public:
        virtual ~VulkanBaseBuffer() = default;

        virtual void Destroy() override;

        using Buffer::Copy;
        virtual void Copy(
            const CommandBuffer* cmd,
            const Buffer* dst,
            std::span<SBufferCopy> regions = {}
        ) override;

        [[nodiscard]] virtual bool IsValid() const override
        {
            return m_Buffer != VK_NULL_HANDLE;
        }

        [[nodiscard]] VkBuffer GetVulkanBuffer() const { return m_Buffer; }
        [[nodiscard]] const VkDescriptorBufferInfo& GetVulkanDescriptorInfo() const
        {
            return m_DescriptorInfo;
        }

        VulkanBaseBuffer& operator=(const VulkanBaseBuffer&) = delete;
        VulkanBaseBuffer& operator=(VulkanBaseBuffer&&) = delete;

      protected:
        VulkanBaseBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        VulkanBaseBuffer(const VulkanBaseBuffer&) = delete;
        VulkanBaseBuffer(VulkanBaseBuffer&&) = delete;

        virtual void CreateBuffer(const SBufferCreateInfo& info);
        virtual void InitBuffer(const SBuffer& buffer);
        virtual void CreateDescriptorInfo();

        VkBuffer m_Buffer = VK_NULL_HANDLE;
        VkDescriptorBufferInfo m_DescriptorInfo{};

        VmaAllocation m_Allocation = VK_NULL_HANDLE;

        const VulkanGraphicsContext* m_GraphicsContext;
    };

    template <class Base>
    class VulkanDynamicBuffer : public VulkanBaseBuffer<Base>
    {
      public:
        virtual void* Map() override;
        virtual void Unmap() override;

      protected:
        VulkanDynamicBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
    };

    class ELIXIR_API VulkanBuffer final : public VulkanBaseBuffer<Buffer>
    {
      public:
        VulkanBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        ~VulkanBuffer() override;
    };

    class ELIXIR_API VulkanStagingBuffer final : public VulkanDynamicBuffer<StagingBuffer>
    {
    public:
        VulkanStagingBuffer(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr
        );
        VulkanStagingBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        ~VulkanStagingBuffer() override;

    protected:
        void InitBuffer(const SBuffer& buffer) override;
    };

    class ELIXIR_API VulkanVertexBuffer final : public VulkanBaseBuffer<VertexBuffer>
    {
      public:
        VulkanVertexBuffer(
            const GraphicsContext* context,
            size_t size,
            const void* data = nullptr
        );
        VulkanVertexBuffer(const GraphicsContext* context, const SBufferCreateInfo& info);
        ~VulkanVertexBuffer() override;

      protected:
        void CreateBufferAddress() override;
    };

    class ELIXIR_API VulkanIndexBuffer final : public VulkanBaseBuffer<IndexBuffer>
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
    };

    class ELIXIR_API VulkanUniformBuffer final : public VulkanDynamicBuffer<UniformBuffer>
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

    protected:
        void InitBuffer(const SBuffer& buffer) override;
    };
}