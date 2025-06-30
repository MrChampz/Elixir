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

      protected:
        void Create(const SSamplerCreateInfo& info);
        void Destroy() override;

      private:
        VkSampler m_Sampler = VK_NULL_HANDLE;
        const VulkanGraphicsContext* m_GraphicsContext;
    };
}