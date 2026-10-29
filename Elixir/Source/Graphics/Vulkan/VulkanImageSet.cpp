#include "epch.h"
#include "VulkanImageSet.h"

#include <Graphics/Vulkan/VulkanShader.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>
#include <Graphics/Vulkan/Utils.h>

namespace Elixir::Vulkan
{
    VulkanImageSet::VulkanImageSet(const GraphicsContext* context)
        : ImageSet(context)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_GraphicsContext = static_cast<const VulkanGraphicsContext*>(context);
        m_Pool = m_GraphicsContext->GetBindlessDescriptorPool();
    }

    VulkanImageSet::~VulkanImageSet()
    {
        EE_PROFILE_ZONE_SCOPED()
        Clear();
    }

    void VulkanImageSet::Clear()
    {
        for (const auto& handle : m_Images | std::views::values)
            m_Pool->UnregisterImage(handle);

        m_Images.clear();
        m_ImageCount = 0;
    }

    SResourceHandle VulkanImageSet::AddImage(const Ref<Image>& image)
    {
        if (m_Images.contains(image))
            return m_Images[image];

        const auto handle = m_Pool->RegisterImage(image);
        if (!handle.IsValid()) return handle;

        m_Images[image] = handle;
        m_ImageCount++;
        return handle;
    }

    void VulkanImageSet::RemoveImage(const SResourceHandle handle)
    {

        const auto it = std::ranges::find_if(
            m_Images,
            [handle](const auto& entry)
            {
                return entry.second == handle;
            }
        );

        if (it != m_Images.end())
        {
            m_Pool->UnregisterImage(handle);
            m_Images.erase(it);
            m_ImageCount--;
        }
    }

    VkDescriptorSet VulkanImageSet::GetDescriptorSet() const
    {
        return m_Pool->GetDescriptorSet();
    }

    VkDescriptorSetLayout VulkanImageSet::GetDescriptorSetLayout() const
    {
        return m_Pool->GetDescriptorSetLayout();
    }
}
