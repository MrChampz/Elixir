#include "epch.h"
#include "VulkanShader.h"

#include "VulkanBuffer.h"
#include "Converters.h"
#include "Utils.h"

#include <ranges>

namespace Elixir::Vulkan
{
    VulkanShader::VulkanShader(const GraphicsContext* context, SShaderCreateInfo&& info)
        : Shader(context, std::move(info))
    {
        EE_PROFILE_ZONE_SCOPED()
        m_GraphicsContext = static_cast<const VulkanGraphicsContext*>(context);

        CreateDescriptorSetLayouts();
        CreateDescriptorSets();
    }

    VulkanShader::~VulkanShader()
    {
        EE_PROFILE_ZONE_SCOPED()

        // TODO: Set descriptor sets as unused in Command Pool
        VK_CHECK_RESULT(
            vkFreeDescriptorSets(
                m_GraphicsContext->GetDevice(),
                m_GraphicsContext->GetDescriptorPool(),
                m_DescriptorSets.size(),
                m_DescriptorSets.data()
            )
        );

        for (const auto& layout : m_DescriptorSetLayouts)
        {
            vkDestroyDescriptorSetLayout(
                m_GraphicsContext->GetDevice(),
                layout,
                nullptr
            );
        }

        m_DescriptorSetLayouts.clear();
    }

    void VulkanShader::CreateDescriptorSetLayouts()
    {
        std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> sets;

        for (const auto& buffer : std::views::values(m_Resources.ConstantBuffers))
        {
            const auto set = buffer.GetSet();

            VkDescriptorSetLayoutBinding binding = {};
            binding.binding = buffer.GetBinding();
            binding.descriptorCount = 1;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS; // TODO: GetShaderState(buffer->GetStage());

            sets[set].push_back(binding);
        }

        for (const auto& resource : std::views::values(m_Resources.Resources))
        {
            const auto set = resource.GetSet();

            VkDescriptorSetLayoutBinding binding = {};
            binding.binding = resource.GetBinding();
            binding.descriptorCount = resource.GetCount();
            binding.descriptorType = Converters::GetDescriptorType(resource.GetType());
            binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS; // TODO: GetShaderState(resource->GetStage());

            sets[set].push_back(binding);
        }

        m_DescriptorSetLayouts.reserve(sets.size());

        for (const auto& bindings : std::views::values(sets))
        {
            VkDescriptorSetLayoutCreateInfo layoutInfo = {};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.pBindings = bindings.data();
            layoutInfo.bindingCount = bindings.size();

            VkDescriptorSetLayout layout;
            VK_CHECK_RESULT(
                vkCreateDescriptorSetLayout(
                    m_GraphicsContext->GetDevice(),
                    &layoutInfo,
                    nullptr,
                    &layout
                )
            );

            m_DescriptorSetLayouts.push_back(layout);
        }
    }

    void VulkanShader::CreateDescriptorSets()
    {
        if (m_DescriptorSetLayouts.empty())
            return;

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pSetLayouts = m_DescriptorSetLayouts.data();
        allocInfo.descriptorSetCount = m_DescriptorSetLayouts.size();
        allocInfo.descriptorPool = m_GraphicsContext->GetDescriptorPool();

        m_DescriptorSets.resize(m_DescriptorSetLayouts.size());
        VK_CHECK_RESULT(vkAllocateDescriptorSets(
            m_GraphicsContext->GetDevice(), &allocInfo, m_DescriptorSets.data()
        ));

        UpdateDescriptorSets();
    }

    void VulkanShader::UpdateDescriptorSets()
    {
        std::vector<VkWriteDescriptorSet> writeDescriptorSets;

        for (const auto [binding, texture] : m_Textures)
        {
            const auto writeSet = GetWriteDescriptorSet(binding, texture);
            writeDescriptorSets.push_back(writeSet);
        }

        for (const auto [binding, buffer] : m_ConstantBuffers)
        {
            const auto writeSet = GetWriteDescriptorSet(binding, buffer);
            writeDescriptorSets.push_back(writeSet);
        }

        vkUpdateDescriptorSets(
            m_GraphicsContext->GetDevice(), writeDescriptorSets.size(),
            writeDescriptorSets.data(), 0, nullptr
        );
    }

    VkWriteDescriptorSet VulkanShader::GetWriteDescriptorSet(
        const SShaderBinding binding,
        const Ref<Texture>& texture
    ) const
    {
        const auto& resource = m_Resources.Resources.at(binding);

        // TODO: Support arrays of textures
        const auto count = 1;   // resource->GetCount();
        const auto imageInfos = new VkDescriptorImageInfo[count];

        for (auto i = 0; i < count; i++)
        {
            const auto tex = TryToGetVulkanImage(texture.get());
            imageInfos[i] = tex->GetVulkanDescriptorInfo();
        }

        VkWriteDescriptorSet writeSet = {};
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet = m_DescriptorSets[resource.GetSet()];
        writeSet.dstBinding = resource.GetBinding();
        writeSet.descriptorType = Converters::GetDescriptorType(resource.GetType());
        writeSet.descriptorCount = count;
        writeSet.pImageInfo = imageInfos;

        return writeSet;
    }

    void VulkanShader::UpdateDescriptorSet(
        const SShaderBinding binding,
        const Ref<Texture>& texture
    ) const
    {
        const auto writeSet = GetWriteDescriptorSet(binding, texture);

        vkUpdateDescriptorSets(
            m_GraphicsContext->GetDevice(),
            1,
            &writeSet,
            0,
            nullptr
        );
    }

    VkWriteDescriptorSet VulkanShader::GetWriteDescriptorSet(
        const SShaderBinding binding,
        const Ref<UniformBuffer>& buffer
    ) const
    {
        const auto& resource = m_Resources.ConstantBuffers.at(binding);
        const auto buf = std::static_pointer_cast<VulkanUniformBuffer>(buffer);

        VkWriteDescriptorSet writeSet = {};
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet = m_DescriptorSets[resource.GetSet()];
        writeSet.dstBinding = resource.GetBinding();
        writeSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeSet.descriptorCount = 1;
        writeSet.pBufferInfo = &buf->GetVulkanDescriptorInfo();

        return writeSet;
    }

    void VulkanShader::UpdateDescriptorSet(
        const SShaderBinding binding,
        const Ref<UniformBuffer>& buffer
    ) const
    {
        const auto writeSet = GetWriteDescriptorSet(binding, buffer);

        vkUpdateDescriptorSets(
            m_GraphicsContext->GetDevice(),
            1,
            &writeSet,
            0,
            nullptr
        );
    }
}