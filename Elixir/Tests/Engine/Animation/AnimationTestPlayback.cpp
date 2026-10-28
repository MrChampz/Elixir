#include "AnimationTestNodes.h"
#include "AnimationTestCharacter.h"
#include "AnimationTestPlayback.h"

bool PlayWithInputs(Elixir::AnimationTestCharacter& mesh,
    Elixir::Ref<const Elixir::AnimationNode> source, const std::span<const float> values,
    const float fadeDuration, const bool looping)
{
    auto& player = mesh.GetBaseLayer().GetPlayer();
    return player.Play(MakeNode(std::move(source)), values, fadeDuration, looping) &&
        mesh.GetAnimation().RefreshPose();
}

bool PlayWithInputs(Elixir::AnimationTestCharacter& mesh,
    Elixir::Ref<const Elixir::AnimationNode> source,
    const std::initializer_list<float> values, const float fadeDuration, const bool looping)
{
    return PlayWithInputs(mesh, MakeNode(std::move(source)),
        std::span(values.begin(), values.size()), fadeDuration, looping);
}

bool PlayWithInputs(Elixir::Animation& animation,
    Elixir::Ref<const Elixir::AnimationNode> source,
    const std::initializer_list<float> values, const float fadeDuration, const bool looping)
{
    auto& player = animation.GetBaseLayer().GetPlayer();
    return player.Play(MakeNode(std::move(source)), values, fadeDuration, looping) &&
        animation.RefreshPose();
}
