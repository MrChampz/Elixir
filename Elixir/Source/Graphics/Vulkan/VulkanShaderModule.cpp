#include "epch.h"
#include "VulkanShaderModule.h"

#include "Utils.h"

namespace Elixir::Vulkan
{
    VulkanShaderModule::~VulkanShaderModule()
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    VulkanShaderModule::VulkanShaderModule(
        const GraphicsContext* context,
        const EShaderStage stage,
        const SShaderModuleCreateInfo& info
    ) : ShaderModule(context, stage, info)
    {
        EE_PROFILE_ZONE_SCOPED()
        m_GraphicsContext = static_cast<const VulkanGraphicsContext*>(context);

        CreateShaderModule(info);
    }

    void VulkanShaderModule::CreateShaderModule(const SShaderModuleCreateInfo& info)
    {
        VkShaderModuleCreateInfo moduleInfo{};
        moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleInfo.pCode = info.Bytecode.data();
        moduleInfo.codeSize = info.Bytecode.size() * sizeof(uint32_t);

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