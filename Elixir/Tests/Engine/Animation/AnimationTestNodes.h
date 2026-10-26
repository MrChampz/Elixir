#pragma once

#include <Engine/Animation/SequenceNode.h>
#include <Engine/Animation/BlendSpaceNode.h>
#include <Engine/Animation/StateMachineNode.h>

/**
 * @brief Wrap a sequence resource for existing playback regression fixtures.
 * @param sequence Resource referenced by the created node.
 */
Elixir::Ref<const Elixir::AnimationNode> MakeNode(Elixir::Ref<const Elixir::Sequence> sequence);

/**
 * @brief Wrap a blend-space resource for existing playback regression fixtures.
 * @param space Resource referenced by the created node.
 */
Elixir::Ref<const Elixir::AnimationNode> MakeNode(Elixir::Ref<const Elixir::BlendSpace> space);

/**
 * @brief Wrap a state-machine resource for existing playback regression fixtures.
 * @param machine Resource referenced by the created node.
 */
Elixir::Ref<const Elixir::AnimationNode> MakeNode(Elixir::Ref<const Elixir::StateMachine> machine);

/**
 * @brief Preserve an already explicit node connection.
 * @param node Connection retained by the fixture.
 */
Elixir::Ref<const Elixir::AnimationNode> MakeNode(Elixir::Ref<const Elixir::AnimationNode> node);

/**
 * @brief Preserve an intentionally absent connection in validation tests.
 * @param value Null sentinel.
 */
Elixir::Ref<const Elixir::AnimationNode> MakeNode(std::nullptr_t value);
