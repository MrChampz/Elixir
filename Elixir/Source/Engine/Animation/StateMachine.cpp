#include "epch.h"
#include "StateMachine.h"

namespace Elixir
{
    /* SAnimationState */

    bool SAnimationState::IsValid() const
    {
        return !Name.empty() &&
            Node && Node->IsValid() &&
            Node->IsValidPlayback(Looping) &&
            InputParameters.size() == Node->GetInputCount();
    }

    /* SAnimationTransition */

    bool SAnimationTransition::IsValid(const std::span<const SAnimationState> states) const
    {
        if (!std::isfinite(FadeDuration) || FadeDuration < 0)
            return false;

        const auto from = std::ranges::find(states, From, &SAnimationState::Name);
        const auto to = std::ranges::find(states, To, &SAnimationState::Name);

        if (from == states.end() || to == states.end() || from == to)
            return false;

        if (WaitForCompletion && from->Looping)
            return false;

        const auto conditionsValid = std::ranges::all_of(
            Conditions,
            [](const auto& condition)
            {
                return condition.IsValid();
            }
        );

        return conditionsValid;
    }

    /* SStateMachineCreateInfo */

    bool SStateMachineCreateInfo::IsValid() const
    {
        if (States.empty()) return false;

        std::unordered_set<std::string> names;
        Ref<const Skeleton> skeleton;

        for (const auto& state : States)
        {
            if (!state.IsValid() || !names.insert(state.Name).second)
                return false;

            if (skeleton && !state.Node->IsCompatibleWith(skeleton))
                return false;

            skeleton = state.Node->GetSkeleton();
        }

        if (!names.contains(InitialState))
            return false;

        const auto transitionsValid = std::ranges::all_of(
            Transitions,
            [&](const auto& transition)
            {
                return transition.IsValid(States);
            }
        );

        return transitionsValid;
    }

    /* StateMachine */

    StateMachine::StateMachine(
        SStateMachineCreateInfo info,
        Ref<const Skeleton> skeleton,
        const size_t initial,
        std::vector<size_t> targets,
        std::vector<std::vector<size_t>> outgoing
    ) : m_CreateInfo(std::move(info)),
        m_Skeleton(std::move(skeleton)),
        m_InitialState(initial),
        m_Targets(std::move(targets)),
        m_Outgoing(std::move(outgoing)) {}

    Ref<StateMachine> StateMachine::Create(SStateMachineCreateInfo info)
    {
        if (!info.IsValid())
        {
            EE_CORE_ERROR("Animation state machine definition is invalid.")
            return nullptr;
        }

        std::unordered_map<std::string, size_t> indices;
        for (size_t i = 0; i < info.States.size(); ++i)
            indices.emplace(info.States[i].Name, i);

        const auto initial = indices[info.InitialState];
        const auto skeleton = info.States.front().Node->GetSkeleton();

        std::vector<size_t> targets;
        std::vector<std::vector<size_t>> outgoing(info.States.size());

        for (size_t i = 0; i < info.Transitions.size(); ++i)
        {
            const auto& transition = info.Transitions[i];
            targets.push_back(indices[transition.To]);
            outgoing[indices[transition.From]].push_back(i);
        }

        return Ref<StateMachine>(new StateMachine(
            std::move(info),
            skeleton,
            initial,
            std::move(targets),
            std::move(outgoing)
        ));
    }

    bool StateMachine::CanBind(
        const AnimationParameters& parameters,
        const std::span<const std::string> bindings
    ) const
    {
        if (!bindings.empty()) return false;

        for (const auto& state : m_CreateInfo.States)
            if (!state.Node->CanBind(parameters, state.InputParameters))
                return false;

        for (const auto& transition : m_CreateInfo.Transitions)
            for (const auto& condition : transition.Conditions)
                if (!condition.IsValid(parameters))
                    return false;

        return true;
    }
}
