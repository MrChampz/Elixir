#include "epch.h"
#include "SkeletalMeshInstance.h"

namespace Elixir
{
    Ref<SkeletalMeshInstance> SkeletalMeshInstance::Create(Ref<const SkeletalMesh> mesh)
    {
        if (!mesh)
        {
            EE_CORE_ERROR("Skeletal mesh instance requires a valid mesh.")
            return nullptr;
        }
        SModelPose reference;
        if (!mesh->GetSkeleton()->BuildModelPose(mesh->GetData().ReferencePose, reference))
            return nullptr;
        return Ref<SkeletalMeshInstance>(new SkeletalMeshInstance(
            std::move(mesh), std::move(reference)));
    }

    SkeletalMeshInstance::SkeletalMeshInstance(
        Ref<const SkeletalMesh> mesh,
        SModelPose referenceModelPose
    ) : m_Mesh(std::move(mesh)),
        m_ReferenceModelPose(std::move(referenceModelPose)) {}
}
