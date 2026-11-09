#include "EditorVFXManager.h"

#include <Engine/Logging/Log.h>
#include <cmath>
#include <vector>
#include <fstream>
#include <iterator>

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

EditorVFXManager::EditorVFXManager(
    const Elixir::GraphicsContext* context,
    const Elixir::ShaderLoader* loader,
    Elixir::Materials::MaterialRegistry& registry,
    Elixir::Materials::MaterialSystem& materials
)
    : Manager(context, loader, registry, materials),
      m_Playback(Elixir::CreateRef<EditorVFX::VegetationPlayback>()),
      m_Vegetation(context, loader), m_Leaves(context, loader), m_Balloons(context, loader)
{
}

void EditorVFXManager::SetLeafRenderers(
    std::vector<std::optional<EditorVFX::LeafSettings>> settings
)
{
    m_Leaves.SetSettings(std::move(settings));
}

void EditorVFXManager::OnRenderFrame(
    const Elixir::Ref<const Elixir::Aether::Simulation::RenderFrame>& frame,
    const Elixir::Camera& camera,
    const Elixir::Ref<Elixir::Image>&
)
{
    m_Vegetation.Prepare(frame, camera);
    m_Leaves.Prepare(frame, camera);
    m_Balloons.Prepare(frame, camera);
}

void EditorVFXManager::RenderCustomRenderers(
    const Elixir::Ref<Elixir::Image>& color,
    const Elixir::Ref<Elixir::Image>& depth
)
{
    m_Vegetation.Render(color, depth);
    m_Leaves.Render(color, depth);
    m_Balloons.Render(color, depth);
}

void EditorVFXManager::SetBalloonRenderers(
    std::vector<std::optional<EditorVFX::BalloonSettings>> settings
)
{
    m_Balloons.SetSettings(std::move(settings));
}

Elixir::Ref<Elixir::Aether::System> EditorVFXManager::ParseEffect(
    const std::string_view source,
    std::string* error
) const
{
    return EditorVFX::ParseVegetationEffect(source, error, m_Playback);
}

Elixir::Ref<Elixir::Aether::System> EditorVFXManager::LoadEffect(
    const std::filesystem::path& path
) const
{
    std::ifstream stream(path);
    if (!stream) return nullptr;
    const std::string source{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    return ParseEffect(source);
}

void EditorVFXManager::SetStrandGrowthPaused(const bool paused)
{
    m_Playback->Paused.store(paused);
}
