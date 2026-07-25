#include "epch.h"
#include "Material.h"

namespace Elixir
{
    void Material::SetDefaultParam(const std::string& name, const SMaterialParam& value)
    {
        m_DefaultParams[name] = value;
    }

    const SMaterialParam* Material::GetDefaultParam(const std::string& name) const
    {
        const auto it = m_DefaultParams.find(name);
        return it != m_DefaultParams.end() ? &it->second : nullptr;
    }
}
