#pragma once

#include <Engine/Graphics/Sampler.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

#include <vulkan/vulkan.h>

namespace Elixir::Vulkan
{
    class ELIXIR_API VulkanSampler final : public Sampler
    {
      public:
        VulkanSampler(const GraphicsContext* context);
        ~VulkanSampler();

        void Destroy() override;

        [[nodiscard]] bool IsDestroyed() const override { return m_Destroyed; }

        VkSampler GetVulkanSampler() const { return m_Sampler; }

      protected:
        void UpdateSampler() override;

      private:
        VkSampler m_Sampler = VK_NULL_HANDLE;
        bool m_Destroyed = true;

        const VulkanGraphicsContext* m_GraphicsContext;
    };
}