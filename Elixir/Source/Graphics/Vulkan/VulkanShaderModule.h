#pragma once

#include <Engine/Graphics/Shader/ShaderModule.h>
#include <Graphics/Vulkan/VulkanGraphicsContext.h>

namespace Elixir::Vulkan
{
    using namespace Elixir;

    class ELIXIR_API VulkanShaderModule final : public ShaderModule
    {
      public:
        VulkanShaderModule(
            const GraphicsContext* context,
            EShaderStage stage,
            const std::string& entrypoint,
            const std::vector<Byte>& bytecode,
            const std::filesystem::path& path = ""
        );
        ~VulkanShaderModule() override;

      protected:
        void CreateShaderModule(const std::vector<Byte>& bytecode);

        VkShaderModule m_Module;

        const VulkanGraphicsContext* m_GraphicsContext;
    };
}
