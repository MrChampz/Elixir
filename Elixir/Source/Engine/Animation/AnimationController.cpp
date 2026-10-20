#include "epch.h"
#include "AnimationController.h"

namespace Elixir
{
    Ref<AnimationController> AnimationController::Create(SAnimationControllerCreateInfo info)
    {
        if (info.Name.empty() || info.States.empty())
        {
            EE_CORE_ERROR("AnimationController requires a name and at least one state.")
            return nullptr;
        }

        std::unordered_set<std::string> names;
        for (const auto& parameter : info.Parameters)
        {
            const auto* value = std::get_if<float>(&parameter.DefaultValue);

            if (parameter.Name.empty() ||
                !names.insert(parameter.Name).second ||
                (value && !std::isfinite(*value)))
            {
                EE_CORE_ERROR(
                    "AnimationController requires unique, valid parameter definitions."
                )
                return nullptr;
            }
        }

        names.clear();

        Ref<const Skeleton> skeleton;
        std::vector<std::array<std::optional<size_t>, 2>> blendParameters;

        for (const auto& state : info.States)
        {
            if (state.Name.empty() ||
                !names.insert(state.Name).second ||
                int(bool(state.Sequence)) + int(bool(state.BlendSpace)) +
                    int(bool(state.Space2D)) != 1)
            {
                EE_CORE_ERROR(
                    "Animation states require unique names and exactly one pose source."
                )
                return nullptr;
            }

            const auto& stateSkeleton = state.Sequence
                ? state.Sequence->GetSkeleton()
                : state.BlendSpace
                    ? state.BlendSpace->GetSkeleton()
                    : state.Space2D->GetSkeleton();

            if (skeleton &&
                skeleton != stateSkeleton &&
                !skeleton->IsCompatible(*stateSkeleton))
            {
                EE_CORE_ERROR("AnimationController states require compatible skeletons.")
                return nullptr;
            }

            skeleton = stateSkeleton;

            std::array<std::optional<size_t>, 2> resolved;
            const std::array bindings{ state.BlendParameter, state.BlendParameterY };

            for (size_t axis = 0; axis < 2; ++axis)
            {
                const auto required = axis == 0
                    ? bool(state.BlendSpace || state.Space2D)
                    : bool(state.Space2D);

                if (!required)
                {
                    if (bindings[axis].empty())
                        continue;

                    EE_CORE_ERROR("Animation state has an unused blend parameter.")
                    return nullptr;
                }

                const auto parameter = std::ranges::find_if(
                    info.Parameters,
                    [&](const auto& value)
                    {
                        return value.Name == bindings[axis];
                    }
                );

                if (parameter == info.Parameters.end() ||
                    !std::holds_alternative<float>(parameter->DefaultValue))
                {
                    EE_CORE_ERROR("Blend-space states require existing numeric parameters.")
                    return nullptr;
                }

                resolved[axis] = size_t(parameter - info.Parameters.begin());
            }

            blendParameters.push_back(resolved);
        }

        const auto findState = [&](const std::string& name)
        {
            return std::ranges::find_if(
                info.States,
                [&](const auto& state)
                {
                    return state.Name == name;
                }
            );
        };

        const auto initial = findState(info.InitialState);
        if (initial == info.States.end())
        {
            EE_CORE_ERROR("AnimationController initial state does not exist.")
            return nullptr;
        }

        const auto initialIndex = size_t(initial - info.States.begin());
        std::vector<std::vector<STransition>> transitions(info.States.size());

        for (const auto& transition : info.Transitions)
        {
            const auto from = findState(transition.From);
            const auto to = findState(transition.To);

            if (from == info.States.end() ||
                to == info.States.end() ||
                from == to ||
                !std::isfinite(transition.FadeDuration) ||
                transition.FadeDuration < 0.0f ||
                (transition.WaitForCompletion && from->Looping))
            {
                EE_CORE_ERROR("AnimationController contains an invalid transition.")
                return nullptr;
            }

            STransition resolved{
                .Target = size_t(to - info.States.begin()),
                .FadeDuration = transition.FadeDuration,
                .WaitForCompletion = transition.WaitForCompletion,
            };

            for (const auto& condition : transition.Conditions)
            {
                const auto parameter = std::ranges::find_if(
                    info.Parameters,
                    [&](const auto& definition)
                    {
                        return definition.Name == condition.Parameter;
                    }
                );

                const auto* value = std::get_if<float>(&condition.Value);
                const auto comparison = condition.Comparison;

                const auto validComparison =
                    comparison == EAnimationComparison::Equal ||
                    comparison == EAnimationComparison::NotEqual ||
                    (value && (comparison == EAnimationComparison::Less ||
                               comparison == EAnimationComparison::LessEqual ||
                               comparison == EAnimationComparison::Greater ||
                               comparison == EAnimationComparison::GreaterEqual));

                if (parameter == info.Parameters.end() ||
                    parameter->DefaultValue.index() != condition.Value.index() ||
                    (value && !std::isfinite(*value)) || !validComparison)
                {
                    EE_CORE_ERROR("AnimationController contains an invalid typed condition.")
                    return nullptr;
                }

                resolved.Conditions.push_back({
                    size_t(parameter - info.Parameters.begin()),
                    comparison,
                    condition.Value
                });
            }

            const auto index = size_t(from - info.States.begin());
            transitions[index].push_back(std::move(resolved));
        }

        return Ref<AnimationController>(new AnimationController(
            std::move(info),
            std::move(transitions),
            initialIndex,
            std::move(blendParameters)
        ));
    }

    AnimationController::AnimationController(
        SAnimationControllerCreateInfo info,
        std::vector<std::vector<STransition>> transitions,
        const size_t initialState,
        std::vector<std::array<std::optional<size_t>, 2>> blendParameters
    ) : m_Name(std::move(info.Name)),
        m_Parameters(std::move(info.Parameters)),
        m_States(std::move(info.States)),
        m_Transitions(std::move(transitions)),
        m_InitialState(initialState),
        m_BlendParameters(std::move(blendParameters))
    {
        m_Skeleton = info.States.front().Sequence
            ? info.States.front().Sequence->GetSkeleton()
            : info.States.front().BlendSpace
                ? info.States.front().BlendSpace->GetSkeleton()
                : info.States.front().Space2D->GetSkeleton();
    }
}
