#pragma once

#include <Engine/Graphics/Shader/ShaderModule.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

namespace Elixir::Vulkan
{
    using namespace Elixir;

    class ELIXIR_API VulkanShaderModule final : public ShaderModule
    {
      public:
        ~VulkanShaderModule() override;

      protected:
        VulkanShaderModule(
            const GraphicsContext* context,
            EShaderStage stage,
            const SShaderModuleCreateInfo& info
        );

        void CreateShaderModule(const SShaderModuleCreateInfo& info);

        VkShaderModule m_Module;

        const VulkanGraphicsContext* m_GraphicsContext;
    };
}
