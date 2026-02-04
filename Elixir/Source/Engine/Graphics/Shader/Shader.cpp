#include "epch.h"
#include "Shader.h"

#include <Graphics/Vulkan/VulkanShader.h>

namespace Elixir
{
    constexpr size_t GetStageIndex(EShaderStage stage) noexcept
    {
        return static_cast<size_t>(stage);
    }

    Ref<Texture> Shader::GetTexture(const std::string& name) const
    {
        if (const auto binding = GetShaderBinding(name))
        {
            return GetTexture(*binding);
        }

        EE_CORE_ERROR("No texture binding named \"{0}\" found in shader...", name)
        return nullptr;
    }

    Ref<Texture> Shader::GetTexture(SShaderBinding binding) const
    {
        if (m_Textures.contains(binding))
        {
            return m_Textures.at(binding);
        }

        EE_CORE_ERROR(
            "No texture for (set \"{0}\", binding \"{1}\") in shader...", binding.Set,
            binding.Binding
        )
        return nullptr;
    }

    Ref<TextureSet> Shader::GetTextureSet(const std::string& name) const
    {
        if (const auto binding = GetShaderBinding(name))
        {
            return GetTextureSet(*binding);
        }

        EE_CORE_ERROR("No texture set binding named \"{0}\" found in shader...", name)
        return nullptr;
    }

    Ref<TextureSet> Shader::GetTextureSet(SShaderBinding binding) const
    {
        if (m_TextureSets.contains(binding))
        {
            return m_TextureSets.at(binding);
        }

        EE_CORE_ERROR(
            "No texture set for (set \"{0}\", binding \"{1}\") in shader...", binding.Set,
            binding.Binding
        )
        return nullptr;
    }

    Ref<Sampler> Shader::GetSampler(const std::string& name) const
    {
        if (const auto binding = GetShaderBinding(name))
        {
            return GetSampler(*binding);
        }

        EE_CORE_ERROR("No sampler binding named \"{0}\" found in shader...", name)
        return nullptr;
    }

    Ref<Sampler> Shader::GetSampler(SShaderBinding binding) const
    {
        if (m_Samplers.contains(binding))
        {
            return m_Samplers.at(binding);
        }

        EE_CORE_ERROR(
            "No sampler for (set \"{0}\", binding \"{1}\") in shader...", binding.Set,
            binding.Binding
        )
        return nullptr;
    }

    Ref<ShaderModule> Shader::GetModule(const EShaderStage stage) const
    {
        return m_Modules[GetStageIndex(stage)];
    }

    Ref<Shader> Shader::Create(const GraphicsContext* context, SShaderCreateInfo&& info)
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanShader>(context, std::move(info));
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    Shader::Shader(const GraphicsContext* context, SShaderCreateInfo&& info)
        : m_Name(info.Name), m_GraphicsContext(context)
    {
        struct ShaderModuleInfo
        {
            EShaderStage Stage;
            Scope<SShaderModuleCreateInfo> Module;
        };

        ShaderModuleInfo modules[] = {
            {EShaderStage::Vertex, std::move(info.Vertex)},
            {EShaderStage::Pixel, std::move(info.Pixel)},
            {EShaderStage::Compute, std::move(info.Compute)},
        };

        for (auto& entry : modules)
        {
            if (!entry.Module)
                continue;

            const auto module = CreateModule(entry.Stage, entry.Module);
            m_Modules[GetStageIndex(entry.Stage)] = module;
        }

        CreateBindingLookup();
    }

    Ref<ShaderModule> Shader::CreateModule(
        const EShaderStage stage,
        const Scope<SShaderModuleCreateInfo>& info
    )
    {
        const auto module = ShaderModule::Create(
            m_GraphicsContext, stage, info->Entrypoint, info->Bytecode, info->Path
        );

        for (const auto& resource : info->Resources)
        {
            const auto ref = AddResourceToBindingTable(resource);
            module->AddResource(ref);
        }

        for (const auto& buffer : info->ConstantBuffers)
        {
            const auto ref = AddConstantBufferToBindingTable(buffer);
            module->AddConstantBuffer(ref);
        }

        for (const auto& constant : info->PushConstants)
        {
            const auto ref = AddPushConstantToBindingTable(constant);
            module->AddPushConstant(ref);
        }

        return module;
    }

    const ShaderResource* Shader::AddResourceToBindingTable(ShaderResource resource)
    {
        const auto set = resource.GetSet();
        const auto binding = resource.GetBinding();

        const auto& [it, _] = m_Resources.Resources
            .insert_or_assign({set, binding}, resource);
        return &it->second;
    }

    const ShaderConstantBuffer* Shader::AddConstantBufferToBindingTable(
        ShaderConstantBuffer buffer
    )
    {
        const auto set = buffer.GetSet();
        const auto binding = buffer.GetBinding();

        const auto& [it, _] = m_Resources.ConstantBuffers
            .insert_or_assign({set, binding}, buffer);
        return &it->second;
    }

    const ShaderPushConstant* Shader::AddPushConstantToBindingTable(
        ShaderPushConstant constant
    )
    {
        const auto set = constant.GetSet();
        const auto binding = constant.GetBinding();

        const auto& [it, _] = m_Resources.PushConstants
            .insert_or_assign({set, binding}, constant);
        return &it->second;
    }

    void Shader::CreateBindingLookup()
    {
        for (const auto& [binding, resource] : m_Resources.Resources)
        {
            SaveShaderBindingToLookup(resource.GetName(), binding);
        }

        for (const auto& [binding, buffer] : m_Resources.ConstantBuffers)
        {
            SaveShaderBindingToLookup(buffer.GetName(), binding);
        }

        for (const auto& [binding, constant] : m_Resources.PushConstants)
        {
            SaveShaderBindingToLookup(constant.GetName(), binding);
        }
    }

    void Shader::SaveShaderBindingToLookup(
        const std::string& name,
        const SShaderBinding binding
    )
    {
        m_BindingLookup[name] = binding;
    }

    const SShaderBinding* Shader::GetShaderBinding(const std::string& name) const
    {
        if (m_BindingLookup.contains(name))
        {
            return &m_BindingLookup.at(name);
        }

        return nullptr;
    }
}