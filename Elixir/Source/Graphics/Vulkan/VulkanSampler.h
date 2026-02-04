#pragma once

#include <Engine/Graphics/Sampler.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

#include <vulkan/vulkan.h>

namespace Elixir::Vulkan
{
    class ELIXIR_API VulkanSampler final : public Sampler
    {
      public:
        VulkanSampler(const GraphicsContext* context, const SSamplerCreateInfo& info);
        ~VulkanSampler() override;

        [[nodiscard]] bool IsValid() const override { return m_Sampler != VK_NULL_HANDLE; }

        VkSampler GetVulkanSampler() const { return m_Sampler; }
        const VkDescriptorImageInfo& GetVulkanDescriptorInfo() const { return m_DescriptorInfo; }

      protected:
        void Create(const SSamplerCreateInfo& info);
        void CreateDescriptorInfo();
        void Destroy() override;

      private:
        VkSampler m_Sampler = VK_NULL_HANDLE;
        VkDescriptorImageInfo m_DescriptorInfo{};

        const VulkanGraphicsContext* m_GraphicsContext;
    };
}