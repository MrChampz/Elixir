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
    namespace
    {
        uint64_t GetResourceGeneration(const Ref<Image>& image)
        {
            const auto vkImage = TryToGetVulkanImage(image.get());
            return vkImage ? vkImage->GetVulkanResourceGeneration() : 0;
        }

        VkImageLayout GetDescriptorImageLayout(const Ref<Image>& image)
        {
            const auto vkImage = TryToGetVulkanImage(image.get());
            return vkImage
                ? vkImage->GetVulkanDescriptorInfo().imageLayout
                : VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

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

        m_DescriptorSets.ForEach([this](auto& sets)
        {
            if (!sets.empty())
                m_GraphicsContext->GetDescriptorPool()->FreeDescriptorSets(sets);
        });

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
        const auto binding = GetShaderBinding(name);
        if (!binding || !m_Resources.ConstantBuffers.contains(*binding))
        {
            EE_CORE_ERROR("No constant buffer binding named \"{0}\" found in shader...", name)
            return;
        }

        if (!data || size == 0 || size > m_GraphicsContext->GetMaxConstantBufferRange())
        {
            EE_CORE_ERROR("Constant buffer updates require data and a supported byte count.")
            return;
        }

        const auto found = m_ConstantBuffers.find(*binding);
        if (found == m_ConstantBuffers.end())
        {
            const auto capacity = std::max<size_t>(
                size,
                m_Resources.ConstantBuffers.at(*binding).GetSize()
            );

            if (capacity > m_GraphicsContext->GetMaxConstantBufferRange())
            {
                EE_CORE_ERROR("Shader constant buffer exceeds the device uniform range.")
                return;
            }

            const auto buffer = UniformBuffer::Create(m_GraphicsContext, capacity);
            if (!buffer || !buffer->IsValid()) return;

            buffer->UpdateData(data, size);
            BindConstantBuffer(name, buffer);

            return;
        }

        const auto buffer = std::dynamic_pointer_cast<DynamicBuffer>(found->second);
        if (!buffer || size > buffer->GetSize())
        {
            EE_CORE_ERROR("Constant buffer updates require a mappable buffer with enough space.")
            return;
        }

        const auto mapped = buffer->Map();
        if (!mapped)
        {
            EE_CORE_ERROR("Constant buffer could not be mapped for an update.")
            return;
        }

        Memory::Memcpy(mapped, data, size);
        buffer->Unmap();
    }

    void VulkanShader::BindTexture(const std::string& name, const Ref<Texture>& texture)
    {
        if (const auto binding = GetShaderBinding(name))
        {
            m_Textures[*binding] = texture;
            m_DescriptorSets.Set(
                *binding,
                DescriptorValue{ STextureDescriptorValue{
                    texture,
                    texture ? GetResourceGeneration(texture->GetImage()) : 0,
                    texture ? GetDescriptorImageLayout(texture->GetImage())
                            : VK_IMAGE_LAYOUT_UNDEFINED
                } }
            );
            return;
        }

        EE_CORE_ERROR("No texture binding named \"{0}\" found in shader...", name)
    }

    void VulkanShader::BindImage(const std::string& name, const Ref<Image>& image)
    {
        if (const auto binding = GetShaderBinding(name))
        {
            m_Images[*binding] = image;
            m_DescriptorSets.Set(
                *binding,
                DescriptorValue{ SImageDescriptorValue{
                    image,
                    GetResourceGeneration(image),
                    GetDescriptorImageLayout(image)
                } }
            );
            return;
        }

        EE_CORE_ERROR("No image binding named \"{0}\" found in shader...", name)
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
        const Ref<Buffer>& buffer
    )
    {
        if (!buffer || !buffer->IsValid() ||
            !HasAnyFlags(buffer->GetUsage(), EBufferUsage::StorageBuffer) ||
            buffer->GetSize() == 0 ||
            buffer->GetSize() > m_GraphicsContext->GetMaxStorageBufferRange())
        {
            EE_CORE_ERROR("Storage binding requires a valid buffer with storage usage and range.")
            return;
        }

        const auto binding = GetShaderBinding(name);
        if (!binding || !m_Resources.StorageBuffers.contains(*binding))
        {
            EE_CORE_ERROR("No storage buffer binding named \"{0}\" found in shader...", name)
            return;
        }

        if (m_DescriptorSets.Set(*binding, DescriptorValue{ buffer }))
            m_StorageBuffers[*binding] = buffer;
    }

    void VulkanShader::BindConstantBuffer(
        const std::string& name,
        const Ref<Buffer>& buffer
    )
    {
        const auto binding = GetShaderBinding(name);
        if (!binding || !m_Resources.ConstantBuffers.contains(*binding))
        {
            EE_CORE_ERROR("No constant buffer binding named \"{0}\" found in shader...", name)
            return;
        }

        if (!buffer || !buffer->IsValid() ||
            !HasAnyFlags(buffer->GetUsage(), EBufferUsage::UniformBuffer) ||
            buffer->GetSize() < m_Resources.ConstantBuffers.at(*binding).GetSize() ||
            buffer->GetSize() == 0 ||
            buffer->GetSize() > m_GraphicsContext->GetMaxConstantBufferRange())
        {
            EE_CORE_ERROR("Constant binding requires a valid buffer with uniform usage and range.")
            return;
        }

        if (m_DescriptorSets.Set(*binding, DescriptorValue{ buffer }))
            m_ConstantBuffers[*binding] = buffer;
    }

    std::vector<VkDescriptorSet> VulkanShader::GetDescriptorSets() const
    {
        std::vector sets(m_DescriptorSets.GetCurrent());
        if (m_BindlessSetIndex)
        {
            const auto bindlessPool = m_GraphicsContext->GetBindlessDescriptorPool();
            sets[*m_BindlessSetIndex] = bindlessPool->GetDescriptorSet();
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
                const auto set = resource.GetSet();
                EE_CORE_ASSERT(
                    !m_BindlessSetIndex || *m_BindlessSetIndex == set,
                    "A shader can use bindless resources from only one descriptor set."
                )
                m_BindlessSetIndex = set;
                sets.try_emplace(set);
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

        m_DescriptorSets.ForEach([this](auto& sets)
        {
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
        });
    }

    void VulkanShader::CreatePipelineLayout()
    {
        std::vector layouts(m_DescriptorSetLayouts);
        const std::vector ranges = GetPushConstantRanges();

        if (m_BindlessSetIndex)
        {
            const auto bindlessPool = m_GraphicsContext->GetBindlessDescriptorPool();
            const auto bindlessLayout = bindlessPool->GetDescriptorSetLayout();
            layouts[*m_BindlessSetIndex] = bindlessLayout;
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
        RefreshImageDescriptorState();

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
                        if constexpr (std::is_same_v<Value, SImageDescriptorValue>)
                            writes.push_back(GetWriteDescriptorSet(binding, value.Image.get()));
                        else if constexpr (std::is_same_v<Value, STextureDescriptorValue>)
                            writes.push_back(GetWriteDescriptorSet(binding, value.Texture.get()));
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

    void VulkanShader::RefreshImageDescriptorState()
    {
        for (const auto& [binding, image] : m_Images)
        {
            m_DescriptorSets.Set(
                binding,
                DescriptorValue{ SImageDescriptorValue{
                    image,
                    GetResourceGeneration(image),
                    GetDescriptorImageLayout(image)
                } }
            );
        }

        for (const auto& [binding, texture] : m_Textures)
        {
            m_DescriptorSets.Set(
                binding,
                DescriptorValue{ STextureDescriptorValue{
                    texture,
                    texture ? GetResourceGeneration(texture->GetImage()) : 0,
                    texture ? GetDescriptorImageLayout(texture->GetImage())
                            : VK_IMAGE_LAYOUT_UNDEFINED
                } }
            );
        }
    }

    VkWriteDescriptorSet VulkanShader::GetWriteDescriptorSet(
        const SShaderBinding binding,
        const Image* image
    ) const
    {
        const auto& resource = m_Resources.Resources.at(binding);
        const auto vkImage = TryToGetVulkanImage(image);

        EE_CORE_ASSERT(vkImage, "Image bindings require a Vulkan image.")

        m_ImageInfoCache[binding] = { vkImage->GetVulkanDescriptorInfo() };

        VkWriteDescriptorSet writeSet = {};
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet = m_DescriptorSets.GetCurrent()[resource.GetSet()];
        writeSet.dstBinding = resource.GetBinding();
        writeSet.descriptorType = Converters::GetDescriptorType(resource.GetType());
        writeSet.descriptorCount = (uint32_t)m_ImageInfoCache[binding].size();
        writeSet.pImageInfo = m_ImageInfoCache[binding].data();

        return writeSet;
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
            const auto image = TryToGetVulkanImage(texture->GetImage().get());
            imageInfos.push_back(image->GetVulkanDescriptorInfo());
        }

        m_ImageInfoCache[binding] = std::move(imageInfos);

        VkWriteDescriptorSet writeSet = {};
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet = m_DescriptorSets.GetCurrent()[resource.GetSet()];
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
        writeSet.dstSet = m_DescriptorSets.GetCurrent()[resource.GetSet()];
        writeSet.dstBinding = resource.GetBinding();
        writeSet.descriptorType = Converters::GetDescriptorType(resource.GetType());
        writeSet.descriptorCount = m_ImageInfoCache[binding].size();
        writeSet.pImageInfo = m_ImageInfoCache[binding].data();

        return writeSet;
    }

    VkWriteDescriptorSet VulkanShader::GetWriteDescriptorSet(
        const SShaderBinding binding,
        const Ref<Buffer>& buffer
    ) const
    {
        const auto uniform = m_Resources.ConstantBuffers.contains(binding);
        m_BufferInfoCache[binding] = {
            .buffer = TryToGetVulkanBuffer(buffer.get()),
            .offset = 0,
            .range = buffer->GetSize(),
        };

        VkWriteDescriptorSet writeSet = {};
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet = m_DescriptorSets.GetCurrent()[binding.Set];
        writeSet.dstBinding = binding.Binding;
        writeSet.descriptorType = uniform
            ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
            : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeSet.descriptorCount = 1;
        writeSet.pBufferInfo = &m_BufferInfoCache[binding];

        return writeSet;
    }
}
