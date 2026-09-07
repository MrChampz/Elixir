#include "epch.h"
#include "StaticMeshInstance.h"

namespace Elixir
{
    Ref<StaticMeshInstance> StaticMeshInstance::Create(
        Ref<StaticMesh> mesh,
        const glm::mat4& localTransform
    )
    {
        if (!mesh)
        {
            EE_CORE_ERROR("StaticMeshInstance requires a static mesh.")
            return nullptr;
        }

        return Ref<StaticMeshInstance>(new StaticMeshInstance(std::move(mesh), localTransform));
    }

    StaticMeshInstance::StaticMeshInstance(
        Ref<StaticMesh> mesh,
        const glm::mat4& localTransform
    ) : m_Mesh(std::move(mesh)), m_LocalTransform(localTransform) {}
}
