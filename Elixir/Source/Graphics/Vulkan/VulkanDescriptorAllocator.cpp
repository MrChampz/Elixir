#include "epch.h"
#include "VulkanDescriptorAllocator.h"

#include <Graphics/Vulkan/VulkanGraphicsContext.h>
#include <Graphics/Vulkan/Utils.h>

namespace Elixir::Vulkan
{
    VulkanDescriptorAllocator::VulkanDescriptorAllocator(
        const VulkanGraphicsContext* context,
        const uint32_t maxSets,
        const std::span<SDescriptorPoolSizeRatio> ratios,
        const bool bindless
    ) : m_GraphicsContext(context)
    {
        EE_PROFILE_ZONE_SCOPED()
        InitPool(maxSets, ratios, bindless);
    }

    VulkanDescriptorAllocator::~VulkanDescriptorAllocator()
    {
        EE_PROFILE_ZONE_SCOPED()
        DestroyPool();
    }

    VkDescriptorSet VulkanDescriptorAllocator::Allocate(const VkDescriptorSetLayout layout) const
    {
        EE_PROFILE_ZONE_SCOPED()

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pNext = nullptr;
        allocInfo.descriptorPool = m_Pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSet set;
        VK_CHECK_RESULT(
            vkAllocateDescriptorSets(
                m_GraphicsContext->GetDevice(),
                &allocInfo,
                &set
            )
        );

        return set;
    }

    void VulkanDescriptorAllocator::Reset() const
    {
        EE_PROFILE_ZONE_SCOPED()
        vkResetDescriptorPool(m_GraphicsContext->GetDevice(), m_Pool, 0);
    }

    void VulkanDescriptorAllocator::InitPool(
        const uint32_t maxSets,
        std::span<SDescriptorPoolSizeRatio> ratios,
        const bool bindless
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        std::vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.reserve(ratios.size());

        for (auto& [Type, Ratio] : ratios)
        {
            VkDescriptorPoolSize size;
            size.type = Type;
            size.descriptorCount = (uint32_t)(Ratio * maxSets);

            poolSizes.emplace_back(size);
        }

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = bindless ? 1 : maxSets;
        poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.flags = 0;

        if (bindless)
        {
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        }
        else
        {
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        }

        VK_CHECK_RESULT(
            vkCreateDescriptorPool(
                m_GraphicsContext->GetDevice(),
                &poolInfo,
                nullptr,
                &m_Pool
            )
        );
    }

    void VulkanDescriptorAllocator::DestroyPool() const
    {
        EE_PROFILE_ZONE_SCOPED()
        vkDestroyDescriptorPool(m_GraphicsContext->GetDevice(), m_Pool, nullptr);
    }
}