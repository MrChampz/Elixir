#pragma once

#include <Engine/Mesh/SkeletalMesh.h>

namespace Elixir
{
    /** Retains a fixed mesh and its reference pose, independently of animation execution. */
    class ELIXIR_API SkeletalMeshInstance final
    {
    public:
        /**
         * @brief Create a renderable instance of a fixed skeletal mesh.
         * @param mesh Mesh retained for the instance's entire lifetime.
         * @return Null if the mesh or its reference model pose is invalid.
         */
        static Ref<SkeletalMeshInstance> Create(Ref<const SkeletalMesh> mesh);

        /** @brief Get the retained mesh asset. */
        const Ref<const SkeletalMesh>& GetMesh() const { return m_Mesh; }

        /** @brief Get model-space reference matrices for rendering without animation. */
        const SModelPose& GetReferenceModelPose() const { return m_ReferenceModelPose; }

    private:
        /**
         * @brief Retain a validated mesh and its precomputed reference matrices.
         * @param mesh Non-null fixed mesh asset.
         * @param referenceModelPose One model-space reference matrix per skeleton bone.
         */
        SkeletalMeshInstance(Ref<const SkeletalMesh> mesh, SModelPose referenceModelPose);

        const Ref<const SkeletalMesh> m_Mesh;
        SModelPose m_ReferenceModelPose;
    };
}
