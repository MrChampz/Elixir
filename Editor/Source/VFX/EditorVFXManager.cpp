#include "EditorVFXManager.h"

#include <Engine/Logging/Log.h>
#include <cmath>
#include <vector>

void EditorVFXManager::Replay(const Elixir::Camera& camera,
    const Elixir::Ref<Elixir::Image>& sceneTarget, const float targetSeconds)
{
    if (!std::isfinite(targetSeconds) || targetSeconds < 0 || targetSeconds > 5)
    {
        EE_CORE_ERROR("VFX preview replay requires a target between zero and five seconds.");
        return;
    }

    constexpr float stepSeconds = 1.0f / 60.0f;
    const auto fullSteps = static_cast<size_t>(std::floor(targetSeconds / stepSeconds));
    std::vector<float> steps(fullSteps, stepSeconds);
    const float remainder = targetSeconds - static_cast<float>(fullSteps) * stepSeconds;
    if (remainder > 0.000001f) steps.push_back(remainder);
    if (steps.empty()) steps.push_back(0);

    RenderSteps(camera, sceneTarget, steps, 0);
}
