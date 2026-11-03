#include "epch.h"
#include "SkeletonSocket.h"

#include <Engine/Animation/Pose.h>

#include <cmath>

namespace Elixir
{
    std::optional<glm::mat4> SSkeletonSocket::GetTransform(const SModelPose& modelPose) const
    {
        if (BoneIndex >= modelPose.Matrices.size() || !RelativeTransform.IsValid())
            return std::nullopt;

        const auto result =
            modelPose.Matrices[BoneIndex] * RelativeTransform.ToMatrix();

        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                if (!std::isfinite(result[c][r]))
                {
                    EE_CORE_ERROR("Socket '{}' exceeds the supported matrix range.", Name)
                    return std::nullopt;
                }

        return result;
    }
}
