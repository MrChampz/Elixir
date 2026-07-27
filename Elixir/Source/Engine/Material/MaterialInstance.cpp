#include "epch.h"
#include "MaterialInstance.h"

#include "MaterialRenderProxy.h"

namespace Elixir
{
    bool MaterialInstance::SetScalar(const std::string& name, const float value)
    {
        return SetOverride(name, SMaterialParam::MakeScalar(value));
    }

    float MaterialInstance::GetScalar(const std::string& name) const
    {
        const auto* param = Resolve(name);
        return param ? param->Scalar : 0.0f;
    }

    bool MaterialInstance::SetVector(const std::string& name, const glm::vec4& value)
    {
        return SetOverride(name, SMaterialParam::MakeVector(value));
    }

    glm::vec4 MaterialInstance::GetVector(const std::string& name) const
    {
        const auto* param = Resolve(name);
        return param ? param->Vector : glm::vec4(0.0f);
    }

    bool MaterialInstance::SetTexture(const std::string& name, const Ref<Texture>& texture)
    {
        return SetOverride(name, SMaterialParam::MakeTexture(texture));
    }

    Ref<Texture> MaterialInstance::GetTexture(const std::string& name) const
    {
        const auto* param = Resolve(name);
        return param ? param->Texture : nullptr;
    }

    Ref<const MaterialRenderProxy> MaterialInstance::CreateRenderProxy(
        Ref<const SCompiledMaterial> material
    ) const
    {
        return MaterialRenderProxy::Create(std::move(material), *this);
    }

    const SMaterialParam* MaterialInstance::GetResolvedParameter(const std::string& name) const
    {
        return Resolve(name);
    }

    bool MaterialInstance::SetOverride(const std::string& name, const SMaterialParam& value)
    {
        if (!m_Parent || !m_Parent->IsParameterValueCompatible(name, value))
            return false;

        m_Overrides[name] = value;
        ++m_Revision;

        return true;
    }

    const SMaterialParam* MaterialInstance::Resolve(const std::string& name) const
    {
        const auto it = m_Overrides.find(name);
        if (it != m_Overrides.end())
            return &it->second;
        return m_Parent ? m_Parent->GetDefaultParam(name) : nullptr;
    }
}
