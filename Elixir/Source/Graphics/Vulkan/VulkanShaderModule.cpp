#include "epch.h"
#include "VulkanShaderModule.h"

#include "Utils.h"

namespace Elixir::Vulkan
{
    const uint32_t* ConvertBytecode(const std::vector<Byte>& bytecode)
    {
        return reinterpret_cast<const uint32_t*>(bytecode.data());
    }

    VulkanShaderModule::VulkanShaderModule(
        const GraphicsContext* context,
        const EShaderStage stage,
        const std::string& entrypoint,
        const std::vector<Byte>& bytecode,
        const std::filesystem::path& path
    ) : ShaderModule(context, stage, entrypoint, path)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_GraphicsContext = static_cast<const VulkanGraphicsContext*>(context);

        CreateShaderModule(bytecode);
    }

    VulkanShaderModule::~VulkanShaderModule()
    {
        EE_PROFILE_ZONE_SCOPED()

        vkDestroyShaderModule(
            m_GraphicsContext->GetDevice(),
            m_Module,
            nullptr
        );
    }

    void VulkanShaderModule::CreateShaderModule(const std::vector<Byte>& bytecode)
    {
        VkShaderModuleCreateInfo moduleInfo{};
        moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleInfo.pCode = ConvertBytecode(bytecode);
        moduleInfo.codeSize = bytecode.size();

        VK_CHECK_RESULT(
            vkCreateShaderModule(
                m_GraphicsContext->GetDevice(),
                &moduleInfo,
                nullptr,
                &m_Module
            )
        );
    }
}