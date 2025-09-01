#include "epch.h"
#include "ShaderModule.h"

#include <Graphics/Vulkan/VulkanShaderModule.h>

namespace Elixir
{
    Ref<ShaderModule> ShaderModule::Create(
        const GraphicsContext* context,
        const EShaderStage stage,
        const SShaderModuleCreateInfo& info)
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanShaderModule>(context, stage, info);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    ShaderModule::ShaderModule(
        const GraphicsContext* context,
        const EShaderStage stage,
        const SShaderModuleCreateInfo& info
    ) : m_Path(info.Path), m_Entrypoint(info.Entrypoint), m_Stage(stage),
        m_GraphicsContext(context)
    {

    }
}