#include "epch.h"
#include "Shader.h"

#include <Graphics/Vulkan/VulkanShader.h>

namespace Elixir
{
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

        EE_CORE_ERROR("No texture set for (set \"{0}\", binding \"{1}\") in shader...", binding.Set, binding.Binding)
        return nullptr;
    }

    Ref<Shader> Shader::Create(const GraphicsContext* context, const SShaderCreateInfo& info)
    {
        switch (context->GetAPI())
        {
            case EGraphicsAPI::Vulkan:
                return CreateRef<Vulkan::VulkanShader>(context, info);
            default:
                EE_CORE_ASSERT(false, "Unknown GraphicsAPI!")
                return nullptr;
        }
    }

    Shader::Shader(const GraphicsContext* context, SShaderCreateInfo& info)
        : m_Name(info.Name), m_GraphicsContext(context)
    {
        struct ShaderModuleInfo
        {
            EShaderStage Stage;
            Scope<SShaderModuleCreateInfo> Module;
        };

        auto modules = {
            ShaderModuleInfo{EShaderStage::Vertex, std::move(info.Vertex)},
            ShaderModuleInfo{EShaderStage::Pixel, std::move(info.Pixel)},
            ShaderModuleInfo{EShaderStage::Compute, std::move(info.Compute)},
        };

        for (auto& [stage, createInfo] : modules)
        {
            if (!createInfo)
                continue;

            auto module = ShaderModule::Create(m_GraphicsContext, stage, *createInfo);
            m_Modules.push_back(module);
        }

        CreateBindingLookup();
    }

    void Shader::CreateBindingTable()
    {
        // TODO: For each module, get all resources, process them into sets/bindings.
    }

    void Shader::CreateBindingLookup()
    {
        for (const auto& bindings : m_Resources.Resources)
        {
            for (const auto& resource : bindings)
            {
                const auto name = resource.GetName();
                const auto set = resource.GetSet();
                const auto binding = resource.GetBinding();
                SaveShaderBindingToLookup(name, { set, binding });
            }
        }

        for (const auto& bindings : m_Resources.ConstantBuffers)
        {
            for (const auto& buffer : bindings)
            {
                const auto name = buffer.GetName();
                const auto set = buffer.GetSet();
                const auto binding = buffer.GetBinding();
                SaveShaderBindingToLookup(name, { set, binding });
            }
        }

        for (const auto& bindings : m_Resources.PushConstants)
        {
            for (const auto& pushConstant : bindings)
            {
                const auto name = pushConstant.GetName();
                const auto set = pushConstant.GetSet();
                const auto binding = pushConstant.GetBinding();
                SaveShaderBindingToLookup(name, { set, binding });
            }
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