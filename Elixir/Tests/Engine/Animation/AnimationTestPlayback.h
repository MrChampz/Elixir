#pragma once

#include "AnimationTestCharacter.h"

/**
 * @brief Configure direct numeric inputs through a layer and publish its initial pose.
 * @param mesh Test character receiving the playback.
 * @param source Source to assign to the base layer.
 * @param values Numeric source coordinates.
 * @param fadeDuration Transition duration in seconds.
 * @param looping Repeat cyclic playback.
 */
bool PlayWithInputs(Elixir::AnimationTestCharacter& mesh,
    Elixir::Ref<const Elixir::AnimationNode> source, std::span<const float> values,
    float fadeDuration = 0, bool looping = true);

/**
 * @brief Configure literal coordinates through the same test setup path.
 * @param mesh Test character receiving the playback.
 * @param source Source to assign to the base layer.
 * @param values Numeric source coordinates.
 * @param fadeDuration Transition duration in seconds.
 * @param looping Repeat cyclic playback.
 */
bool PlayWithInputs(Elixir::AnimationTestCharacter& mesh,
    Elixir::Ref<const Elixir::AnimationNode> source, std::initializer_list<float> values,
    float fadeDuration = 0, bool looping = true);

/**
 * @brief Configure numeric coordinates directly on an independent test animation.
 * @param animation Animation receiving the source.
 * @param source Source assigned to the base control.
 * @param values Numeric coordinates.
 * @param fadeDuration Transition duration in seconds.
 * @param looping Repeat cyclic playback.
 */
bool PlayWithInputs(Elixir::Animation& animation,
    Elixir::Ref<const Elixir::AnimationNode> source, std::initializer_list<float> values,
    float fadeDuration = 0, bool looping = true);
