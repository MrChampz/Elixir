#pragma once

#include <vulkan/vulkan.h>

namespace Elixir::Vulkan
{
    class VulkanGraphicsContext;

    struct SDescriptorPoolSizeRatio
    {
        VkDescriptorType Type;
        float Ratio;
    };

    class ELIXIR_API VulkanDescriptorAllocator
    {
      public:
        VulkanDescriptorAllocator(
            const VulkanGraphicsContext* context,
            uint32_t maxSets,
            std::span<SDescriptorPoolSizeRatio> ratios,
            bool bindless = false
        );

        ~VulkanDescriptorAllocator();

        VkDescriptorSet Allocate(VkDescriptorSetLayout layout) const;
        void Reset() const;

        VkDescriptorPool GetPool() const { return m_Pool; }

      private:
        void InitPool(
            uint32_t maxSets,
            std::span<SDescriptorPoolSizeRatio> ratios,
            bool bindless
        );

        void DestroyPool() const;

        VkDescriptorPool m_Pool;
        const VulkanGraphicsContext* m_GraphicsContext;
    };
}