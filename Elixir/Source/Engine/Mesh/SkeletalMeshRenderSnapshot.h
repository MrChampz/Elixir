#pragma once

#include <Engine/Mesh/SkeletalMeshInstance.h>
#include <Engine/Animation/AnimationFrame.h>

namespace Elixir
{
    /** Captured skeletal draw data owned by a render task instead of a live animation. */
    struct SSkeletalMeshRenderSnapshot
    {
        /** Fixed mesh instance retained until the render task finishes. */
        Ref<const SkeletalMeshInstance> Instance;

        /** Immutable animation result retained for this draw; null uses the reference pose. */
        Ref<const SAnimationFrame> Frame;
        
        /** Captured placement in world space. */
        glm::mat4 Transform{1};
    };
}
