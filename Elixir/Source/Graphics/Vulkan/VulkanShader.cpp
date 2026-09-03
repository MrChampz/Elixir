#include "epch.h"
#include "VulkanShader.h"

#include "VulkanBuffer.h"
#include "Converters.h"
#include "Utils.h"
#include "VulkanCommandBuffer.h"
#include "VulkanSampler.h"
#include "VulkanTextureSet.h"

#include <ranges>
#include <type_traits>

namespace Elixir::Vulkan
{
    VulkanShader::VulkanShader(const GraphicsContext* context, SShaderCreateInfo&& info)
      : Shader(context, std::move(info)),
        m_DescriptorSets(*context)
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

        for (uint32_t frameIndex = 0; frameIndex < GraphicsContext::FRAMES; ++frameIndex)
        {
            auto& sets = m_DescriptorSets.GetResource(frameIndex);
            if (!sets.empty())
                m_GraphicsContext->GetDescriptorPool()->FreeDescriptorSets(sets);
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

    void VulkanShader::Bind(const Ref<CommandBuffer>& cmd, const Pipeline* pipeline)
    {
        const auto vkCmd = static_pointer_cast<VulkanCommandBuffer>(cmd);

        ApplyPendingDescriptorState();
        vkCmd->BindDescriptorSets(pipeline, m_PipelineLayout, 0, GetDescriptorSets());

        for (const auto& [binding, constant] : m_Resources.PushConstants)
        {
            if (m_PushConstants.contains(binding))
            {
                cmd->SetPushConstant(
                    m_PushConstants[binding],
                    this,
                    constant.GetShaderStages()
                );
            }
        }
    }

    void VulkanShader::SetPushConstant(const std::string& name, void* data, const size_t size)
    {
        SetPushConstant(nullptr, name, data, size);
    }

    void VulkanShader::SetPushConstant(
        const Ref<CommandBuffer>& cmd,
        const std::string& name,
        void* data,
        const size_t size
    )
    {
        if (const auto binding = GetShaderBinding(name))
        {
            Ref<PushConstantBuffer> buffer = nullptr;

            if (m_PushConstants.contains(*binding))
            {
                buffer = m_PushConstants.at(*binding);
                const auto mapped = buffer->Map();
                Memory::Memcpy(mapped, data, size);
                buffer->Unmap(size);
            }
            else
            {
                buffer = PushConstantBuffer::Create(
                    m_GraphicsContext,
                    size,
                    data
                );
                m_PushConstants[*binding] = buffer;
            }

            // If cmd was provided we can set the push constant immediately.
            if (cmd)
            {
                const auto descriptor = m_Resources.PushConstants.at(*binding);
                cmd->SetPushConstant(buffer, this, descriptor.GetShaderStages());
            }

            return;
        }

        EE_CORE_ERROR("No push constant binding named \"{0}\" found in shader...", name)
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
                const auto buffer = UniformBuffer::Create(
                    m_GraphicsContext,
                    size,
                    data
                );
                BindConstantBuffer(name, buffer);
            }
            return;
        }

        EE_CORE_ERROR("No constant buffer binding named \"{0}\" found in shader...", name)
    }

    void VulkanShader::BindTexture(const std::string& name, const Ref<Texture>& texture)
    {
        if (const auto binding = GetShaderBinding(name))
        {
            if (m_DescriptorSets.Set(*binding, DescriptorValue{ texture }))
                m_Textures[*binding] = texture;
            return;
        }

        EE_CORE_ERROR("No texture binding named \"{0}\" found in shader...", name)
    }

    void VulkanShader::BindTextureSet(const std::string& name, const Ref<TextureSet>& set)
    {
        if (const auto binding = GetShaderBinding(name))
        {
            m_TextureSets[*binding] = set;
            return;
        }

        EE_CORE_ERROR("No texture set binding named \"{0}\" found in shader...", name)
    }

    void VulkanShader::BindSampler(const std::string& name, const Ref<Sampler>& sampler)
    {
        if (const auto binding = GetShaderBinding(name))
        {
            if (m_DescriptorSets.Set(*binding, DescriptorValue{ sampler }))
                m_Samplers[*binding] = sampler;
            return;
        }

        EE_CORE_ERROR("No texture binding named \"{0}\" found in shader...", name)
    }

    void VulkanShader::BindStorageBuffer(
        const std::string& name,
        const Ref<StorageBuffer>& buffer
    )
    {
        if (const auto binding = GetShaderBinding(name))
        {
            if (m_DescriptorSets.Set(*binding, DescriptorValue{ buffer }))
                m_StorageBuffers[*binding] = buffer;
            return;
        }

        EE_CORE_ERROR("No storage buffer binding named \"{0}\" found in shader...", name)
    }

    void VulkanShader::BindStorageBuffer(
        const std::string& name,
        const Ref<DynamicStorageBuffer>& buffer
    )
    {
        if (const auto binding = GetShaderBinding(name))
        {
            if (m_DescriptorSets.Set(*binding, DescriptorValue{ buffer }))
                m_DynStorageBuffers[*binding] = buffer;
            return;
        }

        EE_CORE_ERROR("No storage buffer binding named \"{0}\" found in shader...", name)
    }

    void VulkanShader::BindConstantBuffer(
        const std::string& name,
        const Ref<UniformBuffer>& buffer
    )
    {
        if (const auto binding = GetShaderBinding(name))
        {
            if (m_DescriptorSets.Set(*binding, DescriptorValue{ buffer }))
                m_ConstantBuffers[*binding] = buffer;
            return;
        }

        EE_CORE_ERROR("No constant buffer binding named \"{0}\" found in shader...", name)
    }

    std::vector<VkDescriptorSet> VulkanShader::GetDescriptorSets() const
    {
        std::vector sets(m_DescriptorSets.GetCurrentFrameResource());
        if (m_BindlessSet)
        {
            const auto bindlessPool = m_GraphicsContext->GetBindlessDescriptorPool();
            sets.push_back(bindlessPool->GetDescriptorSet());
        }

        return std::move(sets);
    }

    std::vector<VkPushConstantRange> VulkanShader::GetPushConstantRanges() const
    {
        std::vector<VkPushConstantRange> ranges;
        ranges.reserve(m_PushConstants.size());

        for (const auto& constant : m_Resources.PushConstants | std::views::values)
        {
            VkPushConstantRange range = {};
            range.offset = constant.GetOffset();
            range.size = constant.GetSize();
            range.stageFlags = Converters::GetShaderStages(constant.GetShaderStages());

            ranges.push_back(range);
        }

        return std::move(ranges);
    }

    void VulkanShader::CreateDescriptorSetLayouts()
    {
        std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> sets;

        for (const auto& buffer : std::views::values(m_Resources.StorageBuffers))
        {
            const auto set = buffer.GetSet();

            VkDescriptorSetLayoutBinding binding = {};
            binding.binding = buffer.GetBinding();
            binding.descriptorCount = 1;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            binding.stageFlags = Converters::GetShaderStages(buffer.GetShaderStages());

            sets[set].push_back(binding);
        }

        for (const auto& buffer : std::views::values(m_Resources.ConstantBuffers))
        {
            const auto set = buffer.GetSet();

            VkDescriptorSetLayoutBinding binding = {};
            binding.binding = buffer.GetBinding();
            binding.descriptorCount = 1;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            binding.stageFlags = Converters::GetShaderStages(buffer.GetShaderStages());

            sets[set].push_back(binding);
        }

        for (const auto& resource : std::views::values(m_Resources.Resources))
        {
            if (resource.IsBindless())
            {
                m_BindlessSet = true;
                continue;
            }

            const auto set = resource.GetSet();

            VkDescriptorSetLayoutBinding binding = {};
            binding.binding = resource.GetBinding();
            binding.descriptorCount = resource.GetCount();
            binding.descriptorType = Converters::GetDescriptorType(resource.GetType());
            binding.stageFlags = Converters::GetShaderStages(resource.GetShaderStages());
            binding.pImmutableSamplers = nullptr;

            sets[set].push_back(binding);
        }

        uint32_t maxSet = 0;
        for (const auto& set : sets | std::views::keys)
            maxSet = std::max(maxSet, set);

        const auto setCount = sets.empty() ? 0 : maxSet + 1;
        m_DescriptorSetLayouts.resize(setCount);

        for (uint32_t set = 0; set < setCount; ++set)
        {
            const auto it = sets.find(set);

            VkDescriptorSetLayoutCreateInfo layoutInfo = {};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

            if (it != sets.end())
            {
                const auto& bindings = it->second;
                layoutInfo.pBindings = bindings.data();
                layoutInfo.bindingCount = (uint32_t)bindings.size();
            }

            VK_CHECK_RESULT(
                vkCreateDescriptorSetLayout(
                    m_GraphicsContext->GetDevice(),
                    &layoutInfo,
                    nullptr,
                    &m_DescriptorSetLayouts[set]
                )
            );
        }
    }

    void VulkanShader::CreateDescriptorSets()
    {
        if (m_DescriptorSetLayouts.empty())
            return;

        for (uint32_t frameIndex = 0; frameIndex < GraphicsContext::FRAMES; ++frameIndex)
        {
            auto& sets = m_DescriptorSets.GetResource(frameIndex);
            sets.resize(m_DescriptorSetLayouts.size());

            for (auto i = 0; i < m_DescriptorSetLayouts.size(); ++i)
            {
                VkDescriptorSetAllocateInfo allocInfo = {};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.pSetLayouts = &m_DescriptorSetLayouts[i];
                allocInfo.descriptorSetCount = 1;
                allocInfo.descriptorPool =
                    m_GraphicsContext->GetDescriptorPool()->GetVulkanDescriptorPool();

                VK_CHECK_RESULT(
                    vkAllocateDescriptorSets(
                        m_GraphicsContext->GetDevice(),
                        &allocInfo,
                        &sets[i]
                    )
                );
            }
        }
    }

    void VulkanShader::CreatePipelineLayout()
    {
        std::vector layouts(m_DescriptorSetLayouts);
        const std::vector ranges = GetPushConstantRanges();

        if (m_BindlessSet)
        {
            const auto bindlessPool = m_GraphicsContext->GetBindlessDescriptorPool();
            const auto bindlessLayout = bindlessPool->GetDescriptorSetLayout();
            layouts.push_back(bindlessLayout);
        }

        VkPipelineLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.setLayoutCount = (uint32_t)layouts.size();
        info.pSetLayouts = layouts.data();
        info.pushConstantRangeCount = (uint32_t)ranges.size();
        info.pPushConstantRanges = ranges.data();

        VK_CHECK_RESULT(
            vkCreatePipelineLayout(
                m_GraphicsContext->GetDevice(),
                &info,
                nullptr,
                &m_PipelineLayout
            )
        );
    }

    void VulkanShader::ApplyPendingDescriptorState()
    {
        m_DescriptorSets.ApplyPendingState([this](auto&, const auto changes)
        {
            std::vector<VkWriteDescriptorSet> writes;
            writes.reserve(changes.size());

            for (const auto& change : changes)
            {
                std::visit(
                    [this, &writes, binding = change.Key](const auto& value)
                    {
                        using Value = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<Value, Ref<Texture>>)
                            writes.push_back(GetWriteDescriptorSet(binding, value.get()));
                        else
                            writes.push_back(GetWriteDescriptorSet(binding, value));
                    },
                    change.Value
                );
            }

            vkUpdateDescriptorSets(
                m_GraphicsContext->GetDevice(),
                static_cast<uint32_t>(writes.size()),
                writes.data(),
                0,
                nullptr
            );
        });
    }

    VkWriteDescriptorSet VulkanShader::GetWriteDescriptorSet(
        const SShaderBinding binding,
        const Texture* texture
    ) const
    {
        const auto& resource = m_Resources.Resources.at(binding);

        // TODO: Support arrays of textures
        const auto count = 1;   // resource->GetCount();
        std::vector<VkDescriptorImageInfo> imageInfos;
        imageInfos.reserve(count);

        for (auto i = 0; i < count; i++)
        {
            const auto tex = TryToGetVulkanImage(texture);
            imageInfos.push_back(tex->GetVulkanDescriptorInfo());
        }

        m_ImageInfoCache[binding] = std::move(imageInfos);

        VkWriteDescriptorSet writeSet = {};
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet = m_DescriptorSets.GetCurrentFrameResource()[resource.GetSet()];
        writeSet.dstBinding = resource.GetBinding();
        writeSet.descriptorType = Converters::GetDescriptorType(resource.GetType());
        writeSet.descriptorCount = m_ImageInfoCache[binding].size();
        writeSet.pImageInfo = m_ImageInfoCache[binding].data();

        return writeSet;
    }

    VkWriteDescriptorSet VulkanShader::GetWriteDescriptorSet(
        const SShaderBinding binding,
        const Ref<Sampler>& sampler
    ) const
    {
        const auto& resource = m_Resources.Resources.at(binding);

        // TODO: Support arrays of samplers
        const auto count = 1;   // resource->GetCount();
        std::vector<VkDescriptorImageInfo> imageInfos;
        imageInfos.reserve(count);

        for (auto i = 0; i < count; i++)
        {
            const auto vk_Sampler = std::static_pointer_cast<VulkanSampler>(sampler);
            imageInfos.push_back(vk_Sampler->GetVulkanDescriptorInfo());
        }

        m_ImageInfoCache[binding] = std::move(imageInfos);

        VkWriteDescriptorSet writeSet = {};
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet = m_DescriptorSets.GetCurrentFrameResource()[resource.GetSet()];
        writeSet.dstBinding = resource.GetBinding();
        writeSet.descriptorType = Converters::GetDescriptorType(resource.GetType());
        writeSet.descriptorCount = m_ImageInfoCache[binding].size();
        writeSet.pImageInfo = m_ImageInfoCache[binding].data();

        return writeSet;
    }

    VkWriteDescriptorSet VulkanShader::GetWriteDescriptorSet(
        const SShaderBinding binding,
        const Ref<StorageBuffer>& buffer
    ) const
    {
        const auto& resource = m_Resources.StorageBuffers.at(binding);
        const auto buf = std::static_pointer_cast<VulkanStorageBuffer>(buffer);

        m_BufferInfoCache[binding] = buf->GetVulkanDescriptorInfo();

        VkWriteDescriptorSet writeSet = {};
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet = m_DescriptorSets.GetCurrentFrameResource()[resource.GetSet()];
        writeSet.dstBinding = resource.GetBinding();
        writeSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeSet.descriptorCount = 1;
        writeSet.pBufferInfo = &m_BufferInfoCache[binding];

        return writeSet;
    }

    VkWriteDescriptorSet VulkanShader::GetWriteDescriptorSet(
        const SShaderBinding binding,
        const Ref<DynamicStorageBuffer>& buffer
    ) const
    {
        const auto& resource = m_Resources.StorageBuffers.at(binding);
        const auto buf = std::static_pointer_cast<VulkanDynamicStorageBuffer>(buffer);

        m_BufferInfoCache[binding] = buf->GetVulkanDescriptorInfo();

        VkWriteDescriptorSet writeSet = {};
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet = m_DescriptorSets.GetCurrentFrameResource()[resource.GetSet()];
        writeSet.dstBinding = resource.GetBinding();
        writeSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeSet.descriptorCount = 1;
        writeSet.pBufferInfo = &m_BufferInfoCache[binding];

        return writeSet;
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
        writeSet.dstSet = m_DescriptorSets.GetCurrentFrameResource()[resource.GetSet()];
        writeSet.dstBinding = resource.GetBinding();
        writeSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeSet.descriptorCount = 1;
        writeSet.pBufferInfo = &m_BufferInfoCache[binding];

        return writeSet;
    }
}
