#include "epch.h"
#include "VulkanShader.h"

#include "VulkanBuffer.h"
#include "Converters.h"
#include "Utils.h"
#include "VulkanCommandBuffer.h"

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
        CreatePipelineLayout();
    }

    VulkanShader::~VulkanShader()
    {
        EE_PROFILE_ZONE_SCOPED()

        vkDestroyPipelineLayout(
            m_GraphicsContext->GetDevice(),
            m_PipelineLayout,
            nullptr
        );

        if (!m_DescriptorSets.empty())
        {
            // TODO: Set descriptor sets as unused in Command Pool
            VK_CHECK_RESULT(
                vkFreeDescriptorSets(
                    m_GraphicsContext->GetDevice(),
                    m_GraphicsContext->GetDescriptorPool(),
                    m_DescriptorSets.size(),
                    m_DescriptorSets.data()
                )
            );
            m_DescriptorSets.clear();
        }

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

    void VulkanShader::Bind(const Ref<CommandBuffer>& cmd)
    {
        const auto vkCmd = static_pointer_cast<VulkanCommandBuffer>(cmd);
        vkCmd->BindDescriptorSets(m_PipelineLayout, 0, m_DescriptorSets);

        // for (auto& constant : m_PushConstants)
        // {
        //
        // }
    }

    void VulkanShader::SetPushConstant(const std::string& name, void* data, size_t size)
    {

    }

    void VulkanShader::SetConstantBuffer(const std::string& name, void* data, const size_t size)
    {
        if (const auto binding = GetShaderBinding(name))
        {
            if (m_ConstantBuffers.contains(*binding))
            {
                const auto& buffer = m_ConstantBuffers.at(*binding);
                const auto mapped = buffer->Map();
                Memory::Memcpy(mapped, data, size);
                buffer->Unmap();
            }
            else
            {
                m_ConstantBuffers[*binding] = UniformBuffer::Create(
                    m_GraphicsContext,
                    size,
                    data
                );
                UpdateDescriptorSet(*binding, m_ConstantBuffers[*binding]);
            }
            return;
        }

        EE_CORE_ERROR("No cbuffer binding named \"{0}\" found in shader...", name)
    }

    void VulkanShader::BindTexture(const std::string& name, const Ref<Texture>& texture)
    {
        if (const auto binding = GetShaderBinding(name))
        {
            m_Textures[*binding] = texture;
            UpdateDescriptorSet(*binding, texture);
            return;
        }

        EE_CORE_ERROR("No texture binding named \"{0}\" found in shader...", name)
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

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        VkSampler immutableSampler;
        vkCreateSampler(m_GraphicsContext->GetDevice(), &samplerInfo, nullptr, &immutableSampler);

        for (const auto& resource : std::views::values(m_Resources.Resources))
        {
            const auto set = resource.GetSet();

            VkDescriptorSetLayoutBinding binding = {};
            binding.binding = resource.GetBinding();
            binding.descriptorCount = resource.GetCount();
            binding.descriptorType = Converters::GetDescriptorType(resource.GetType());
            binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS; // TODO: GetShaderState(resource->GetStage());

            // TODO: Use global immutable sampler
            binding.pImmutableSamplers = &immutableSampler;

            sets[set].push_back(binding);
        }

        m_DescriptorSetLayouts.resize(sets.size());

        for (const auto& [set, bindings] : sets)
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

            m_DescriptorSetLayouts[set] = layout;
        }
    }

    void VulkanShader::CreateDescriptorSets()
    {
        if (m_DescriptorSetLayouts.empty())
            return;

        m_DescriptorSets.resize(m_DescriptorSetLayouts.size());

        for (auto i = 0; i < m_DescriptorSetLayouts.size(); i++)
        {
            VkDescriptorSetAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.pSetLayouts = &m_DescriptorSetLayouts[i];
            allocInfo.descriptorSetCount = 1;
            allocInfo.descriptorPool = m_GraphicsContext->GetDescriptorPool();

            VK_CHECK_RESULT(
                vkAllocateDescriptorSets(
                    m_GraphicsContext->GetDevice(),
                    &allocInfo,
                    &m_DescriptorSets[i]
                )
            );
        }

        UpdateDescriptorSets();
    }

    void VulkanShader::CreatePipelineLayout()
    {
        VkPipelineLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.setLayoutCount = (uint32_t)m_DescriptorSetLayouts.size();
        info.pSetLayouts = m_DescriptorSetLayouts.data();

        VK_CHECK_RESULT(
            vkCreatePipelineLayout(
                m_GraphicsContext->GetDevice(),
                &info,
                nullptr,
                &m_PipelineLayout
            )
        );
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
        std::vector<VkDescriptorImageInfo> imageInfos;
        imageInfos.reserve(count);

        for (auto i = 0; i < count; i++)
        {
            const auto tex = TryToGetVulkanImage(texture.get());
            imageInfos.push_back(tex->GetVulkanDescriptorInfo());
        }

        m_ImageInfoCache[binding] = std::move(imageInfos);

        VkWriteDescriptorSet writeSet = {};
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet = m_DescriptorSets[resource.GetSet()];
        writeSet.dstBinding = resource.GetBinding();
        writeSet.descriptorType = Converters::GetDescriptorType(resource.GetType());
        writeSet.descriptorCount = m_ImageInfoCache[binding].size();
        writeSet.pImageInfo = m_ImageInfoCache[binding].data();

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

        m_BufferInfoCache[binding] = buf->GetVulkanDescriptorInfo();

        VkWriteDescriptorSet writeSet = {};
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet = m_DescriptorSets[resource.GetSet()];
        writeSet.dstBinding = resource.GetBinding();
        writeSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeSet.descriptorCount = 1;
        writeSet.pBufferInfo = &m_BufferInfoCache[binding];

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