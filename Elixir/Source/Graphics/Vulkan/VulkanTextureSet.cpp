#include "epch.h"
#include "VulkanTextureSet.h"

#include <Graphics/Vulkan/VulkanTexture.h>
#include <Graphics/Vulkan/VulkanShader.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>
#include <Graphics/Vulkan/Utils.h>

namespace Elixir::Vulkan
{
    VulkanTextureSet::VulkanTextureSet(const GraphicsContext* context)
        : TextureSet(context)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_GraphicsContext = static_cast<const VulkanGraphicsContext*>(context);
        m_Pool = m_GraphicsContext->GetBindlessDescriptorPool();
    }

    VulkanTextureSet::~VulkanTextureSet()
    {
        EE_PROFILE_ZONE_SCOPED()
        m_Textures.clear();
    }

    SResourceHandle VulkanTextureSet::AddTexture(const Ref<Texture>& texture)
    {
        const auto handle = m_Pool->RegisterTexture(texture);
        m_Textures[handle] = texture;
        m_TextureCount++;
        return handle;
    }

    void VulkanTextureSet::RemoveTexture(const SResourceHandle handle)
    {
        m_Pool->UnregisterTexture(handle);

        if (const auto it = m_Textures.find(handle); it != m_Textures.end())
        {
            m_Textures.erase(it);
            m_TextureCount--;
        }
    }

    VkDescriptorSet VulkanTextureSet::GetDescriptorSet() const
    {
        return m_Pool->GetDescriptorSet();
    }

    VkDescriptorSetLayout VulkanTextureSet::GetDescriptorSetLayout() const
    {
        return m_Pool->GetDescriptorSetLayout();
    }
}