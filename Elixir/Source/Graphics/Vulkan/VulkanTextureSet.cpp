#include "epch.h"
#include "VulkanTextureSet.h"

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
        Clear();
    }

    void VulkanTextureSet::Clear()
    {
        for (const auto& handle : m_Images | std::views::values)
            m_Pool->UnregisterImage(handle);

        m_Images.clear();
        m_ImageCount = 0;
    }

    SResourceHandle VulkanTextureSet::AddImage(const Ref<Image>& image)
    {
        if (m_Images.contains(image))
        {
            return m_Images[image];
        }

        const auto handle = m_Pool->RegisterImage(image);
        if (!handle.IsValid()) return handle;
        m_Images[image] = handle;
        m_ImageCount++;
        return handle;
    }

    void VulkanTextureSet::RemoveImage(const SResourceHandle handle)
    {

        const auto it = std::ranges::find_if(m_Images, [handle](const auto& entry) { return entry.second == handle; });
        if (it != m_Images.end())
        {
            m_Pool->UnregisterImage(handle);
            m_Images.erase(it);
            m_ImageCount--;
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
