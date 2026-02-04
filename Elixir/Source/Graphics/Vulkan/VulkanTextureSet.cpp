#include "epch.h"
#include "VulkanTextureSet.h"

#include "VulkanTexture.h"

#include <Graphics/Vulkan/VulkanGraphicsContext.h>
#include <Graphics/Vulkan/Utils.h>

namespace Elixir::Vulkan
{
    VulkanTextureSet::VulkanTextureSet(
        const GraphicsContext* context,
        const uint32_t maxTextures
    ) : TextureSet(context, maxTextures)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_GraphicsContext = static_cast<const VulkanGraphicsContext*>(context);

        //CreateDescriptorSetLayout();
        InitDescriptorAllocator();
        //AllocateDescriptorSet();
        InitImageInfos();
    }

    VulkanTextureSet::~VulkanTextureSet()
    {
        if (m_DescriptorSetLayout)
        {
            vkDestroyDescriptorSetLayout(
                m_GraphicsContext->GetDevice(),
                m_DescriptorSetLayout,
                nullptr
            );
            m_DescriptorSetLayout = VK_NULL_HANDLE;
        }

        m_DescriptorSet = VK_NULL_HANDLE;
    }

    void VulkanTextureSet::SetTexture(const uint32_t index, const Ref<Texture>& texture)
    {
        TextureSet::SetTexture(index, texture);

        const auto vk_Tex = std::static_pointer_cast<VulkanTexture>(texture);
        m_ImageInfos[index] = vk_Tex->GetVulkanDescriptorInfo();
    }

    void VulkanTextureSet::RemoveTexture(const uint32_t index)
    {
        TextureSet::RemoveTexture(index);

        m_ImageInfos[index] = {};
        m_ImageInfos[index].sampler = VK_NULL_HANDLE;
        m_ImageInfos[index].imageView = VK_NULL_HANDLE;
    }

    VkDescriptorSetLayoutBinding VulkanTextureSet::GetLayoutBinding() const
    {
        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = m_MaxTextures;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        binding.pImmutableSamplers = nullptr;

        return binding;
    }

    VkDescriptorSet VulkanTextureSet::AllocateDescriptorSet(const VkDescriptorSetLayout layout)
    {
        m_DescriptorSet = m_DescriptorAllocator->Allocate(layout);
        return m_DescriptorSet;
    }

    void VulkanTextureSet::CreateDescriptorSetLayout()
    {
        const auto binding = GetLayoutBinding();

        constexpr VkDescriptorBindingFlags bindingFlags =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {};
        bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsInfo.bindingCount = 1;
        bindingFlagsInfo.pBindingFlags = &bindingFlags;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = &bindingFlagsInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;

        VK_CHECK_RESULT(
            vkCreateDescriptorSetLayout(
                m_GraphicsContext->GetDevice(),
                &layoutInfo,
                nullptr,
                &m_DescriptorSetLayout
            )
        );
    }

    void VulkanTextureSet::InitDescriptorAllocator()
    {
        std::vector<SDescriptorPoolSizeRatio> sizes =
        {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0.35 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0.35 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0.2 },
            { VK_DESCRIPTOR_TYPE_SAMPLER, 0.1 }
        };

        m_DescriptorAllocator = CreateRef<VulkanDescriptorAllocator>(
            m_GraphicsContext,
            m_MaxTextures * sizes.size(),
            sizes,
            true
        );
    }

    void VulkanTextureSet::InitImageInfos()
    {
        m_ImageInfos.resize(m_MaxTextures);

        for (auto& info : m_ImageInfos)
        {
            info.sampler = VK_NULL_HANDLE;
            info.imageView = VK_NULL_HANDLE;
            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    }

    void VulkanTextureSet::UpdateDescriptors()
    {
        if (!m_NeedsUpdate)
            return;

        VkWriteDescriptorSet writeSet = {};
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet = m_DescriptorSet;
        writeSet.dstBinding = 0;
        writeSet.dstArrayElement = 0;
        writeSet.descriptorCount = m_MaxTextures;
        writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeSet.pImageInfo = m_ImageInfos.data();

        vkUpdateDescriptorSets(
            m_GraphicsContext->GetDevice(),
            1,
            &writeSet,
            0,
            nullptr
        );

        m_NeedsUpdate = false;
    }
}