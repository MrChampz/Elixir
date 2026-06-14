#include "epch.h"
#include "ShaderModule.h"

#include <Graphics/Vulkan/VulkanShaderModule.h>

namespace Elixir
{
    void ShaderModule::AddResource(const ShaderResource* resource)
    {
        m_Resources.push_back(resource);
    }

    void ShaderModule::AddStorageBuffer(const ShaderStorageBuffer* buffer)
    {
        m_StorageBuffers.push_back(buffer);
    }

    void ShaderModule::AddConstantBuffer(const ShaderConstantBuffer* buffer)
    {
        m_ConstantBuffers.push_back(buffer);
    }

    void ShaderModule::AddPushConstant(const ShaderPushConstant* constant)
    {
        m_PushConstants.push_back(constant);
    }

    Ref<ShaderModule> ShaderModule::Create(
        const GraphicsContext* context,
        const EShaderStage stage,
        const std::string& entrypoint,
        const std::vector<Byte>& bytecode,
        const std::filesystem::path& path
    )
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanShaderModule>(
                    context,
                    stage,
                    entrypoint,
                    bytecode,
                    path
                );
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    ShaderModule::ShaderModule(
        const GraphicsContext* context,
        const EShaderStage stage,
        const std::string& entrypoint,
        const std::filesystem::path& path
    ) : m_Path(path), m_Entrypoint(entrypoint), m_Stage(stage),
        m_GraphicsContext(context)
    {
        EE_PROFILE_ZONE_SCOPED()
    }
}