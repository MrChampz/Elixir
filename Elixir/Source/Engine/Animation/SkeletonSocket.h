#pragma once

#include <Engine/Animation/BoneTransform.h>

namespace Elixir
{
    /** Defines an attachment point relative to one skeleton bone. */
    struct SSkeletonSocket
    {
        /** Unique nonempty name within the skeleton. */
        std::string Name;

        /** Index in Skeleton::GetBones(), independent of mesh skin palettes. */
        uint32_t BoneIndex = 0;

        /** Offset in the bone's local space, including attachment rotation and scale. */
        SBoneTransform RelativeTransform;
    };
}
