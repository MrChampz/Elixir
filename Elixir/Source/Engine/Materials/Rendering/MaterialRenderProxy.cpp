#include "epch.h"
#include "MaterialRenderProxy.h"

#include <Engine/Materials/MaterialInstance.h>

namespace Elixir::Materials::Rendering
{
    Ref<const MaterialRenderProxy> MaterialRenderProxy::Create(
        Ref<const SCompiledMaterial> material,
        const MaterialInstance& instance
    )
    {
        if (!material ||
            !instance.GetParent() ||
            material->MaterialRevision != instance.GetParent()->GetRevision())
            return nullptr;

        auto proxy = CreateRef<MaterialRenderProxy>();
        proxy->m_CompiledMaterial = std::move(material);
        proxy->m_InstanceRevision = instance.GetRevision();

        for (const auto& parameter : proxy->m_CompiledMaterial->Parameters)
        {
            const auto* value = instance.GetResolvedParameter(parameter.Name);
            if (!value) return nullptr;

            if (parameter.Kind == EMaterialParameterKind::Texture)
            {
                const auto texCount = std::max(proxy->m_Textures.size(), size_t(parameter.Slot + 1));
                proxy->m_Textures.resize(texCount);
                proxy->m_Textures[parameter.Slot] = value->Texture;
                continue;
            }

            const auto valueCount = std::max(proxy->m_Values.size(), size_t(parameter.Slot + 1));
            proxy->m_Values.resize(valueCount);
            proxy->m_Values[parameter.Slot] = value->Type == EMaterialParameterType::Scalar
                ? glm::vec4(value->Scalar, 0.0f, 0.0f, 0.0f)
                : value->Vector;
        }

        return proxy;
    }
}
