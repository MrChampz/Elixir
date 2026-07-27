#include "epch.h"
#include "Material.h"

namespace Elixir
{
    void Material::SetGraph(MaterialGraph graph)
    {
        m_Graph = std::move(graph);
        ++m_Revision;
    }

    bool Material::SetDefaultParam(const std::string& name, const SMaterialParam& value)
    {
        const auto it  = m_Parameters.find(name);
        if (it == m_Parameters.end() || !IsValueCompatible(it->second, value))
            return false;

        it->second.DefaultValue = value;
        ++m_Revision;
        return true;
    }

    const SMaterialParam* Material::GetDefaultParam(const std::string& name) const
    {
        const auto* parameter = FindParameter(name);
        return parameter ? &parameter->DefaultValue : nullptr;
    }

    bool Material::DefineParameter(
        std::string name,
        const SMaterialParameterDefinition& definition
    )
    {
        if (name.empty() || !IsValueCompatible(definition, definition.DefaultValue))
            return false;

        const auto [_, inserted] = m_Parameters.emplace(std::move(name), definition);
        if (inserted) ++m_Revision;

        return inserted;
    }

    const SMaterialParameterDefinition* Material::FindParameter(const std::string& name) const
    {
        const auto it = m_Parameters.find(name);
        return it != m_Parameters.end() ? &it->second : nullptr;
    }

    bool Material::IsParameterValueCompatible(
        const std::string& name,
        const SMaterialParam& value
    )
    {
        const auto* parameter = FindParameter(name);
        return parameter && IsValueCompatible(*parameter, value);
    }

    bool Material::ValidateGraph(std::string* error) const
    {
        for (const auto& [_, node] : m_Graph.GetNodes())
        {
            if (node.Type == EMaterialNodeType::Parameter)
            {
                const auto* parameter = FindParameter(node.ParameterName);
                if (parameter &&
                    parameter->Kind == EMaterialParameterKind::Value &&
                    parameter->ValueType == node.OutputType)
                    continue;

                if (error)
                    *error = "Invalid value parameter: " + node.ParameterName;

                return false;
            }

            if (node.Type == EMaterialNodeType::TextureSample)
            {
                const auto* parameter = FindParameter(node.TextureParameterName);
                if (parameter &&
                    parameter->Kind == EMaterialParameterKind::Texture)
                    continue;

                if (error)
                    *error = "Invalid texture parameter: " + node.TextureParameterName;

                return false;
            }
        }

        return true;
    }

    bool Material::IsValueCompatible(
        const SMaterialParameterDefinition& definition,
        const SMaterialParam& value
    )
    {
        if (definition.Kind == EMaterialParameterKind::Texture)
            return value.Type == EMaterialParameterType::Texture;

        if (definition.ValueType == EMaterialGraphValueType::Float)
            return value.Type == EMaterialParameterType::Scalar;

        return value.Type == EMaterialParameterType::Vector;
    }
}
