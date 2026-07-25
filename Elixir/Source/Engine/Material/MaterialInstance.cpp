#include "epch.h"
#include "MaterialInstance.h"

namespace Elixir
{
    void MaterialInstance::SetScalar(const std::string& name, const float value)
    {
        m_Overrides[name] = SMaterialParam::MakeScalar(value);
    }

    float MaterialInstance::GetScalar(const std::string& name) const
    {
        const auto* param = Resolve(name);
        return param ? param->Scalar : 0.0f;
    }

    void MaterialInstance::SetVector(const std::string& name, const glm::vec4& value)
    {
        m_Overrides[name] = SMaterialParam::MakeVector(value);
    }

    glm::vec4 MaterialInstance::GetVector(const std::string& name) const
    {
        const auto* param = Resolve(name);
        return param ? param->Vector : glm::vec4(0.0f);
    }

    void MaterialInstance::SetTexture(const std::string& name, const Ref<Texture>& texture)
    {
        m_Overrides[name] = SMaterialParam::MakeTexture(texture);
    }

    Ref<Texture> MaterialInstance::GetTexture(const std::string& name) const
    {
        const auto* param = Resolve(name);
        return param ? param->Texture : nullptr;
    }

    const SMaterialParam* MaterialInstance::Resolve(const std::string& name) const
    {
        const auto it = m_Overrides.find(name);
        if (it != m_Overrides.end())
            return &it->second;
        return m_Parent ? m_Parent->GetDefaultParam(name) : nullptr;
    }
}
