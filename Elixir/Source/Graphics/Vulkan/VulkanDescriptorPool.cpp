#include "epch.h"
#include "VulkanDescriptorPool.h"

#include <Graphics/Vulkan/Utils.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

namespace Elixir::Vulkan
{
    /* VulkanBaseDescriptorPool */

    VulkanBaseDescriptorPool::~VulkanBaseDescriptorPool()
    {
        EE_PROFILE_ZONE_SCOPED()
        VulkanBaseDescriptorPool::DestroyPool();
    }

    void VulkanBaseDescriptorPool::InitPool(
        const std::vector<VkDescriptorPoolSize> sizes,
        const VkDescriptorPoolCreateFlags flags
    )
    {
        EE_PROFILE_ZONE_SCOPED()

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = m_MaxSets;
        poolInfo.poolSizeCount = (uint32_t)sizes.size();
        poolInfo.pPoolSizes = sizes.data();
        poolInfo.flags = flags;

        VK_CHECK_RESULT(
            vkCreateDescriptorPool(
                m_GraphicsContext->GetDevice(),
                &poolInfo,
                nullptr,
                &m_Pool
            )
        );
    }

    void VulkanBaseDescriptorPool::DestroyPool() const
    {
        EE_PROFILE_ZONE_SCOPED()
        vkDestroyDescriptorPool(m_GraphicsContext->GetDevice(), m_Pool, nullptr);
    }

    /* VulkanDescriptorPool */

    VulkanDescriptorPool::VulkanDescriptorPool(
        const VulkanGraphicsContext& context,
        const uint32_t maxSets,
        const std::span<SDescriptorPoolSizeRatio> ratios
    ) : VulkanBaseDescriptorPool(context, maxSets)
    {
        EE_PROFILE_ZONE_SCOPED()
        EE_CORE_TRACE("Initializing descriptor pool with capacity for {0} sets.", maxSets)

        std::vector<VkDescriptorPoolSize> sizes;
        sizes.reserve(ratios.size());

        for (auto& [Type, Ratio] : ratios)
        {
            VkDescriptorPoolSize size;
            size.type = Type;
            size.descriptorCount = (uint32_t)(Ratio * maxSets);

            sizes.emplace_back(size);
        }

        VulkanBaseDescriptorPool::InitPool(
            std::move(sizes),
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
        );

        EE_CORE_TRACE("Initialized descriptor pool.")
    }

    VkDescriptorSet VulkanDescriptorPool::Allocate(const VkDescriptorSetLayout layout) const
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

    void VulkanDescriptorPool::FreeDescriptorSet(const VkDescriptorSet set) const
    {
        VK_CHECK_RESULT(
            vkFreeDescriptorSets(
                m_GraphicsContext->GetDevice(),
                m_Pool,
                1,
                &set
            )
        );
    }

    void VulkanDescriptorPool::FreeDescriptorSets(
        const std::vector<VkDescriptorSet>& sets
    ) const
    {
        VK_CHECK_RESULT(
            vkFreeDescriptorSets(
                m_GraphicsContext->GetDevice(),
                m_Pool,
                sets.size(),
                sets.data()
            )
        );
    }

    void VulkanDescriptorPool::Reset() const
    {
        EE_PROFILE_ZONE_SCOPED()
        vkResetDescriptorPool(m_GraphicsContext->GetDevice(), m_Pool, 0);
    }

    /* VulkanBindlessDescriptorPool */

    // Binding 0: Texture array (combined image samplers)
    // Binding 1: Texture array (sampled images)
    // Binding 2: Sampler array
    // Binding 3: Constant buffer array
    // Binding 4: Storage buffer array
    static constexpr std::array BINDLESS_LAYOUT_BINDINGS = {
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_DESCRIPTOR_TYPE_SAMPLER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    };

    constexpr uint32_t GetBinding(const VkDescriptorType type)
    {
        switch (type)
        {
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return 0;
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return 1;
            case VK_DESCRIPTOR_TYPE_SAMPLER: return 2;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return 3;
            default: return 0;
        }
    }

    VulkanBindlessDescriptorPool::VulkanBindlessDescriptorPool(
        const VulkanGraphicsContext& context,
        const uint32_t maxTextures,
        const uint32_t maxSamplers,
        const uint32_t maxConstantBuffers
    ):  VulkanBaseDescriptorPool(context, 1),
        m_MaxTextures(maxTextures),
        m_MaxSamplers(maxSamplers),
        m_MaxConstantBuffers(maxConstantBuffers)
    {
        EE_PROFILE_ZONE_SCOPED()
        EE_CORE_ASSERT(maxTextures > 0, "Max textures must be greater than 0!")
        EE_CORE_TRACE(
            "Initializing bindless descriptor pool with capacity for {0} textures, {1} samplers, and {2} constant buffers",
            maxTextures, maxSamplers, maxConstantBuffers
        );

        std::vector<VkDescriptorPoolSize> sizes = {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_MaxTextures },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_MaxTextures          },
            { VK_DESCRIPTOR_TYPE_SAMPLER, m_MaxSamplers                },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_MaxConstantBuffers  }
        };

        VulkanBaseDescriptorPool::InitPool(
            std::move(sizes),
            VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
        );

        CreateDescriptorSetLayout();
        AllocateDescriptorSet();
        InitTexturesStructures();

        EE_CORE_TRACE("Initialized bindless descriptor pool.")
    }

    VulkanBindlessDescriptorPool::~VulkanBindlessDescriptorPool()
    {
        VulkanBaseDescriptorPool::DestroyPool();

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

        m_Textures.clear();
        m_TextureLookup.clear();
        m_TextureCount = 0;
    }

    void VulkanBindlessDescriptorPool::FlushDescriptors()
    {
        if (!m_DirtyTextures.empty())
        {
            UpdateTextureDescriptors(m_DirtyTextures);
            m_DirtyTextures.clear();
        }
    }

    SResourceHandle VulkanBindlessDescriptorPool::RegisterTexture(const Ref<Texture>& texture)
    {
        std::lock_guard lock(m_TextureMutex);

        EE_CORE_ASSERT(texture, "Texture is null!")

        // Check if the texture is already registered
        const auto it = m_TextureLookup.find(texture.get());
        if (it != m_TextureLookup.end())
        {
            const auto handle = it->second;
            m_Textures[handle.Index].RefCount++;
            return handle;
        }

        // Allocate new index
        const auto handle = AllocateTextureHandle();
        EE_CORE_ASSERT(handle.IsValid(), "Failed to allocate texture handle!")

        // Store texture
        m_Textures[handle.Index].Texture = texture;
        m_Textures[handle.Index].RefCount = 1;
        m_TextureLookup[texture.get()] = handle;
        m_TextureCount++;

        // Add to dirty texture list, to be updated
        m_DirtyTextures.push_back(texture);

        return handle;
    }

    void VulkanBindlessDescriptorPool::UpdateTexture(
        const SResourceHandle handle,
        const Ref<Texture>& texture
    )
    {
        std::lock_guard lock(m_TextureMutex);

        EE_CORE_ASSERT(texture, "Texture is null!")
        EE_CORE_ASSERT(handle.Index < m_MaxTextures, "Index out of bounds!")
        EE_CORE_ASSERT(m_Textures[handle.Index].Texture, "No texture at the given handle!")

        // Remove old texture from lookup
        const auto oldTexture = m_Textures[handle.Index].Texture;
        m_TextureLookup.erase(oldTexture.get());

        // Update texture
        m_Textures[handle.Index].Texture = texture;
        m_TextureLookup[texture.get()] = handle;

        // Add to dirty texture list, to be updated
        m_DirtyTextures.push_back(texture);
    }

    void VulkanBindlessDescriptorPool::UnregisterTexture(SResourceHandle handle)
    {
        std::lock_guard lock(m_TextureMutex);

        if (handle.Index >= m_MaxTextures)
        {
            EE_CORE_WARN("Attempted to unregister texture with invalid handle: {0}", handle)
            return;
        }

        if (!m_Textures[handle.Index].Texture)
        {
            EE_CORE_WARN("Attempted to unregister texture that is not registered: {0}", handle)
            return;
        }

        // Decrement reference count
        m_Textures[handle.Index].RefCount--;

        // Only free if no more references
        if (m_Textures[handle.Index].RefCount == 0)
        {
            const auto texture = m_Textures[handle.Index].Texture;
            m_TextureLookup.erase(texture.get());
            m_Textures[handle.Index].Texture = nullptr;
            m_TextureCount--;
            FreeTextureHandle(handle);
        }
    }

    SResourceHandle
    VulkanBindlessDescriptorPool::GetTextureHandle(const Ref<Texture>& texture) const
    {
        return m_TextureLookup.at(texture.get());
    }

    std::vector<VkDescriptorSetLayoutBinding>
    VulkanBindlessDescriptorPool::GetDescriptorSetLayoutBindings() const
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;

        for (const auto binding : BINDLESS_LAYOUT_BINDINGS)
        {
            VkDescriptorSetLayoutBinding layoutBinding = {};
            layoutBinding.binding = GetBinding(binding);
            layoutBinding.descriptorType = binding;
            layoutBinding.descriptorCount = GetMaxDescriptorsByType(binding);
            layoutBinding.stageFlags = VK_SHADER_STAGE_ALL;
            layoutBinding.pImmutableSamplers = nullptr;
            bindings.push_back(layoutBinding);
        }

        return bindings;
    }

    void VulkanBindlessDescriptorPool::CreateDescriptorSetLayout()
    {
        const auto bindings = GetDescriptorSetLayoutBindings();

        const std::vector<VkDescriptorBindingFlags> bindingFlags(
            bindings.size(),
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
        );

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {};
        bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsInfo.bindingCount = (uint32_t)bindingFlags.size();
        bindingFlagsInfo.pBindingFlags = bindingFlags.data();

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = &bindingFlagsInfo;
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layoutInfo.bindingCount = (uint32_t)bindings.size();
        layoutInfo.pBindings = bindings.data();

        VK_CHECK_RESULT(
            vkCreateDescriptorSetLayout(
                m_GraphicsContext->GetDevice(),
                &layoutInfo,
                nullptr,
                &m_DescriptorSetLayout
            )
        );
    }

    void VulkanBindlessDescriptorPool::AllocateDescriptorSet()
    {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_Pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_DescriptorSetLayout;

        VK_CHECK_RESULT(
            vkAllocateDescriptorSets(
                m_GraphicsContext->GetDevice(),
                &allocInfo,
                &m_DescriptorSet
            )
        );
    }

    void VulkanBindlessDescriptorPool::InitTexturesStructures()
    {
        m_Textures.resize(m_MaxTextures);
        m_RecycledIndices.reserve(m_MaxTextures / 10); // 10% churn
    }

    void VulkanBindlessDescriptorPool::UpdateTextureDescriptors(
        const std::vector<Ref<Texture>>& textures
    ) const
    {
        std::vector<VkWriteDescriptorSet> writeSets;
        writeSets.reserve(textures.size());

        for (const auto& texture : textures)
        {
            const auto vkTexture = TryToGetVulkanImage(texture.get());
            const auto imageInfo = vkTexture->GetVulkanDescriptorInfo();

            VkWriteDescriptorSet writeSet = {};
            writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeSet.dstSet = m_DescriptorSet;
            writeSet.dstBinding = GetBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
            writeSet.dstArrayElement = GetTextureHandle(texture).Index;
            writeSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writeSet.descriptorCount = 1,
            writeSet.pImageInfo = &imageInfo;

            writeSets.push_back(writeSet);
        }

        vkUpdateDescriptorSets(
            m_GraphicsContext->GetDevice(),
            writeSets.size(),
            writeSets.data(),
            0,
            nullptr
        );
    }

    SResourceHandle VulkanBindlessDescriptorPool::AllocateTextureHandle()
    {
        // First, try to reuse a recycled index
        if (!m_RecycledIndices.empty())
        {
            const uint32_t index = m_RecycledIndices.back();
            m_RecycledIndices.pop_back();
            return SResourceHandle::Texture(index);
        }

        // If no recycled indices, use the next fresh index
        if (m_NextFreeIndex < m_MaxTextures)
        {
            return SResourceHandle::Texture(m_NextFreeIndex++);
        }

        // Pool is completely full
        return SResourceHandle{};
    }

    void VulkanBindlessDescriptorPool::FreeTextureHandle(const SResourceHandle handle)
    {
        m_RecycledIndices.push_back(handle.Index);
    }

    uint32_t VulkanBindlessDescriptorPool::GetMaxDescriptorsByType(
        const VkDescriptorType type
    ) const
    {
        switch (type)
        {
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                return m_MaxTextures;
            case VK_DESCRIPTOR_TYPE_SAMPLER:
                return m_MaxSamplers;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                return m_MaxConstantBuffers;
            default:
                return 0;
        }
    }
}