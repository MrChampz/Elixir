#include "epch.h"
#include "AnimationController.h"

namespace Elixir
{
    Ref<AnimationController> AnimationController::Create(SAnimationControllerCreateInfo info)
    {
        if (info.Name.empty() || info.Layers.empty())
        {
            EE_CORE_ERROR("AnimationController requires a name and at least one layer.")
            return nullptr;
        }

        std::unordered_set<std::string> names;
        for (const auto& parameter : info.Parameters)
        {
            const auto* value = std::get_if<float>(&parameter.DefaultValue);

            if (parameter.Name.empty() || !names.insert(parameter.Name).second ||
                (value && !std::isfinite(*value)))
            {
                EE_CORE_ERROR(
                    "AnimationController requires unique, valid parameter definitions."
                )
                return nullptr;
            }
        }

        std::unordered_set<std::string> layerNames;
        Ref<const Skeleton> skeleton;

        std::vector<SStateMachine> machines;
        machines.reserve(info.Layers.size());

        for (size_t i = 0; i < info.Layers.size(); ++i)
        {
            const auto& layer = info.Layers[i];
            const auto base = i == 0;

            const auto [_, inserted] = layerNames.insert(layer.Name);
            if (layer.Name.empty() || !inserted)
            {
                EE_CORE_ERROR("Animation controller layers require unique, nonempty names.")
                return nullptr;
            }

            if ((base && layer.Name != BaseLayerName) ||
                (!base && layer.Name == BaseLayerName))
            {
                EE_CORE_ERROR("Base must be the first animation controller layer.")
                return nullptr;
            }

            if (!std::isfinite(layer.Weight) || layer.Weight < 0 || layer.Weight > 1)
            {
                EE_CORE_ERROR("Animation layer '{}' requires a weight in [0, 1].", layer.Name)
                return nullptr;
            }

            if (base)
            {
                if (layer.Mask ||
                    layer.Weight != 1 ||
                    layer.BlendMode != EAnimationLayerBlendMode::Override ||
                    !layer.AdditiveReferencePose.empty())
                {
                    EE_CORE_ERROR(
                        "Base requires weight one, Override, no mask, and no additive reference."
                    )
                    return nullptr;
                }
            }
            else if (!layer.Mask)
            {
                EE_CORE_ERROR("Animation layer '{}' requires a bone mask.", layer.Name)
                return nullptr;
            }

            auto machine = CompileStateMachine(
                layer.StateMachine,
                info.Parameters,
                skeleton
            );
            if (!machine) return nullptr;

            if (!base)
            {
                const auto& maskSkeleton = layer.Mask->GetSkeleton();
                if (maskSkeleton != skeleton && !skeleton->IsCompatible(*maskSkeleton))
                {
                    EE_CORE_ERROR("Animation layer '{}' has an incompatible mask.", layer.Name)
                    return nullptr;
                }
            }

            const auto& referencePose = layer.AdditiveReferencePose;
            if (!referencePose.empty() && referencePose.size() != skeleton->GetBones().size())
            {
                EE_CORE_ERROR(
                    "Animation layer '{}' requires one reference transform per bone.",
                    layer.Name
                )
                return nullptr;
            }

            const auto invalidTransform = std::ranges::any_of(
                referencePose,
                [](const auto& transform) { return !transform.IsValid(); }
            );
            if (invalidTransform)
            {
                EE_CORE_ERROR(
                    "Animation layer '{}' has an invalid additive reference transform.",
                    layer.Name
                )
                return nullptr;
            }

            machines.push_back(std::move(*machine));
        }

        return Ref<AnimationController>(new AnimationController(
            std::move(info),
            std::move(skeleton),
            std::move(machines)
        ));
    }

    std::optional<AnimationController::SStateMachine> AnimationController::CompileStateMachine(
        const SAnimationStateMachine& machine,
        const std::vector<SAnimationParameter>& parameters,
        Ref<const Skeleton>& skeleton
    )
    {
        std::unordered_set<std::string> names;

        if (machine.States.empty())
        {
            EE_CORE_ERROR("Animation state machines require at least one state.")
            return std::nullopt;
        }

        std::vector<std::vector<size_t>> blendParameters;

        for (const auto& state : machine.States)
        {
            if (state.Name.empty() ||
                !names.insert(state.Name).second ||
                bool(state.Sequence) == bool(state.BlendSpace))
            {
                EE_CORE_ERROR(
                    "Animation states require unique names and exactly one pose source."
                )
                return std::nullopt;
            }

            const auto& stateSkeleton = state.Sequence
                ? state.Sequence->GetSkeleton()
                : state.BlendSpace->GetSkeleton();

            if (skeleton && skeleton != stateSkeleton &&
                !skeleton->IsCompatible(*stateSkeleton))
            {
                EE_CORE_ERROR("AnimationController states require compatible skeletons.")
                return std::nullopt;
            }

            skeleton = stateSkeleton;

            const auto count = state.BlendSpace
                ? state.BlendSpace->GetDimensionCount()
                : 0;

            if (state.BlendParameters.size() != count)
            {
                EE_CORE_ERROR(
                    "State blend parameters must match its pose source's axis count."
                )
                return std::nullopt;
            }

            std::vector<size_t> resolved;

            for (const auto& binding : state.BlendParameters)
            {
                const auto parameter = std::ranges::find_if(
                    parameters,
                    [&](const auto& value) { return value.Name == binding; }
                );

                if (parameter == parameters.end() ||
                    !std::holds_alternative<float>(parameter->DefaultValue))
                {
                    EE_CORE_ERROR("Blend-space states require existing numeric parameters.")
                    return std::nullopt;
                }

                resolved.push_back(size_t(parameter - parameters.begin()));
            }

            blendParameters.push_back(std::move(resolved));
        }

        const auto findState = [&](const std::string& name)
        {
            return std::ranges::find_if(
                machine.States,
                [&](const auto& state) { return state.Name == name; }
            );
        };

        const auto initial = findState(machine.InitialState);
        if (initial == machine.States.end())
        {
            EE_CORE_ERROR("AnimationController initial state does not exist.")
            return std::nullopt;
        }

        const auto initialIndex = size_t(initial - machine.States.begin());
        std::vector<std::vector<STransition>> transitions(machine.States.size());

        for (const auto& transition : machine.Transitions)
        {
            const auto from = findState(transition.From);
            const auto to = findState(transition.To);

            if (from == machine.States.end() ||
                to == machine.States.end() ||
                from == to ||
                !std::isfinite(transition.FadeDuration) ||
                transition.FadeDuration < 0.0f ||
                (transition.WaitForCompletion && from->Looping))
            {
                EE_CORE_ERROR("AnimationController contains an invalid transition.")
                return std::nullopt;
            }

            STransition resolved{
                .Target = size_t(to - machine.States.begin()),
                .FadeDuration = transition.FadeDuration,
                .WaitForCompletion = transition.WaitForCompletion,
            };

            for (const auto& condition : transition.Conditions)
            {
                const auto parameter = std::ranges::find_if(
                    parameters,
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

                if (parameter == parameters.end() ||
                    parameter->DefaultValue.index() != condition.Value.index() ||
                    (value && !std::isfinite(*value)) || !validComparison)
                {
                    EE_CORE_ERROR("AnimationController contains an invalid typed condition.")
                    return std::nullopt;
                }

                resolved.Conditions.push_back({
                    size_t(parameter - parameters.begin()),
                    comparison,
                    condition.Value
                });
            }

            const auto index = size_t(from - machine.States.begin());
            transitions[index].push_back(std::move(resolved));
        }

        return SStateMachine{
            initialIndex,
            std::move(transitions),
            std::move(blendParameters)
        };
    }

    AnimationController::AnimationController(
        SAnimationControllerCreateInfo info,
        Ref<const Skeleton> skeleton,
        std::vector<SStateMachine> machines
    ) : m_Name(std::move(info.Name)),
        m_Skeleton(std::move(skeleton)),
        m_Parameters(std::move(info.Parameters)),
        m_Layers(std::move(info.Layers)),
        m_StateMachines(std::move(machines)) {}
}
