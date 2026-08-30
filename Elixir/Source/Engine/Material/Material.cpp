#include "epch.h"
#include "Material.h"

#include <Engine/Material/MaterialInstance.h>

namespace Elixir
{
    Ref<MaterialInstance> Material::CreateInstance()
    {
        return CreateRef<MaterialInstance>(shared_from_this());
    }

    void Material::SetGraph(MaterialGraph graph)
    {
        m_Graph = std::move(graph);
        ++m_Revision;
    }

    bool Material::SetUsage(const EMaterialUsage usage, const bool enabled)
    {
        const uint32_t mask = GetMaterialUsageMask(usage);
        const uint32_t updatedMask = enabled
            ? m_UsageMask | mask
            : m_UsageMask & ~mask;

        if (updatedMask == m_UsageMask)
            return false;

        m_UsageMask = updatedMask;
        ++m_Revision;
        return true;
    }

    bool Material::SupportsUsage(const EMaterialUsage usage) const
    {
        return (m_UsageMask & GetMaterialUsageMask(usage)) != 0;
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
    ) const
    {
        const auto* parameter = FindParameter(name);
        return parameter && IsValueCompatible(*parameter, value);
    }

    bool Material::ValidateGraph(std::string* error) const
    {
        class ParameterLookup final : public IMaterialNodeValidationContext
        {
        public:
            explicit ParameterLookup(const Material& material) : m_Material(material) {}

            bool HasValueParameter(
                const std::string_view name,
                const EMaterialValueType valueType
            ) const override
            {
                const auto* parameter = m_Material.FindParameter(std::string(name));
                return parameter && parameter->Kind == EMaterialParameterKind::Value &&
                    parameter->ValueType == valueType;
            }

            bool HasTextureParameter(const std::string_view name) const override
            {
                const auto* parameter = m_Material.FindParameter(std::string(name));
                return parameter && parameter->Kind == EMaterialParameterKind::Texture;
            }

        private:
            const Material& m_Material;
        };

        return m_Graph.Validate(ParameterLookup(*this), error);
    }

    bool Material::IsValueCompatible(
        const SMaterialParameterDefinition& definition,
        const SMaterialParam& value
    )
    {
        if (definition.Kind == EMaterialParameterKind::Texture)
            return value.Type == EMaterialParameterType::Texture;

        if (definition.ValueType == EMaterialValueType::Float)
            return value.Type == EMaterialParameterType::Scalar;

        return value.Type == EMaterialParameterType::Vector;
    }
}
