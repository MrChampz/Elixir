#include "epch.h"
#include "MaterialRenderProxy.h"

#include <algorithm>

namespace Elixir
{
    namespace
    {
        EMaterialBlendMode ResolveBlendMode(const MaterialInstance& instance)
        {
            const Ref<Material>& material = instance.GetParent();
            if (!material || !material->HasGraph())
                return EMaterialBlendMode::Opaque;
            return material->GetGraph()->GetBlendMode();
        }
    }

    MaterialRenderProxy::MaterialRenderProxy(std::string name,
        Ref<const MaterialResource> resource,
        std::unordered_map<std::string, SMaterialParam> parameters)
        : m_Name(std::move(name)), m_Resource(std::move(resource)),
          m_Parameters(std::move(parameters))
    {
    }

    Ref<const MaterialRenderProxy> MaterialRenderProxy::Create(
        const MaterialInstance& instance,
        const Ref<const MaterialResource>& requestedResource)
    {
        if (!instance.GetParent())
            return nullptr;

        const SMaterialShaderPermutation& permutation =
            requestedResource ? requestedResource->GetPermutation()
                              : MaterialShaderPermutation::SurfaceStaticMesh();
        const EMaterialBlendMode blendMode = requestedResource
            ? requestedResource->GetBlendMode() : ResolveBlendMode(instance);
        Ref<const MaterialResource> resource = requestedResource;
        if (!resource)
            resource = MaterialResource::Create(instance, permutation, blendMode);
        else if (!resource->IsCompatible(instance, permutation, blendMode))
            return nullptr;
        if (!resource)
            return nullptr;

        std::unordered_map<std::string, SMaterialParam> parameters =
            instance.GetParent()->GetDefaults();
        for (const auto& [name, value] : instance.GetOverrides())
        {
            const auto definition = parameters.find(name);
            if (definition != parameters.end() && definition->second.Type == value.Type)
                definition->second = value;
        }
        return Ref<const MaterialRenderProxy>(new MaterialRenderProxy(
            instance.GetName(), std::move(resource), std::move(parameters)));
    }

    const SMaterialParam* MaterialRenderProxy::GetParameter(const std::string& name) const
    {
        const auto it = m_Parameters.find(name);
        return it != m_Parameters.end() ? &it->second : nullptr;
    }

    float MaterialRenderProxy::GetScalar(const std::string& name) const
    {
        const SMaterialParam* parameter = GetParameter(name);
        return parameter && parameter->Type == SMaterialParam::EType::Scalar
            ? parameter->Scalar : 0.0f;
    }

    glm::vec4 MaterialRenderProxy::GetVector(const std::string& name) const
    {
        const SMaterialParam* parameter = GetParameter(name);
        return parameter && parameter->Type == SMaterialParam::EType::Vector
            ? parameter->Vector : glm::vec4(0.0f);
    }

    Ref<Texture> MaterialRenderProxy::GetTexture(const std::string& name) const
    {
        const SMaterialParam* parameter = GetParameter(name);
        return parameter && parameter->Type == SMaterialParam::EType::Texture
            ? parameter->TextureRef : nullptr;
    }

    uint32_t MaterialRenderProxy::CollectGraphParams(
        glm::vec4* out, const uint32_t maxCount,
        const TextureIndexResolver& textureResolver) const
    {
        const Ref<const Material>& material = m_Resource->GetMaterial();
        if (!out || maxCount == 0 || !material || !material->HasGraph())
            return 0;

        uint32_t count = 0;
        for (const auto& parameter : material->GetGraphParameters())
        {
            if (parameter.Slot >= maxCount)
                continue;
            const SMaterialParam* value = GetParameter(parameter.Name);
            if (!value || value->Type != parameter.Type)
                continue;

            if (parameter.Type == SMaterialParam::EType::Scalar)
                out[parameter.Slot] = glm::vec4(value->Scalar, 0.0f, 0.0f, 0.0f);
            else if (parameter.Type == SMaterialParam::EType::Vector)
                out[parameter.Slot] = value->Vector;
            else
            {
                constexpr uint32_t invalid = 0xffffffffu;
                uint32_t index = invalid;
                if (value->TextureRef && textureResolver)
                    index = textureResolver(value->TextureRef);
                out[parameter.Slot] = glm::vec4(
                    index == invalid ? MaterialInstance::NO_TEXTURE_INDEX : (float)index,
                    0.0f, 0.0f, 0.0f);
            }
            count = std::max(count, parameter.Slot + 1);
        }
        return count;
    }
}
