#pragma once

#include <Engine/Mesh/StaticMesh.h>
#include <Engine/Mesh/SkeletalMeshInstance.h>
#include <Engine/Animation/AnimationFrame.h>

#include <variant>

namespace Elixir
{
    /** Fixed geometry retained for a static mesh draw. */
    struct SStaticMeshRenderData
    {
        /** Mesh retained until the queued draw finishes. */
        Ref<const StaticMesh> Mesh;
    };

    /** Fixed geometry and an optional immutable pose retained for a skeletal mesh draw. */
    struct SSkeletalMeshRenderData
    {
        /** Fixed mesh instance retained until the queued draw finishes. */
        Ref<const SkeletalMeshInstance> Instance;

        /** Immutable animation result; null selects the instance's reference pose. */
        Ref<const SAnimationFrame> Frame;
    };

    /**
     * Captured renderable data assembled before submission to the render queue.
     *
     * Producers may gather placement, geometry, and animation from separate components.
     * Consumers retain this value and its resources without consulting those components.
     */
    struct SRenderableSnapshot
    {
        /** Captured model-to-world matrix shared by every geometry type. */
        glm::mat4 Transform{1};

        /** Geometry-specific data; exactly one draw type is selected. */
        std::variant<SStaticMeshRenderData, SSkeletalMeshRenderData> Data;
    };
}
