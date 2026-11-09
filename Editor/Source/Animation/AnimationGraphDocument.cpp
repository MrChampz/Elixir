#include "AnimationGraphDocument.h"

#include <Engine/Animation/StateMachineNode.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <istream>
#include <ostream>
#include <set>

using namespace Elixir;

std::string AnimationGraphDocument::Validate() const
{
    std::string libraryError;
    const auto sources = Library.Build(libraryError);
    if (!libraryError.empty())
        return libraryError;
    if (Name.empty())
        return "Enter a graph name.";
    if (Machine.States.empty() && OutputSource.empty())
        return "Add at least one state.";
    if (Positions.size() != Machine.States.size())
        return "Missing state positions.";
    for (const auto& position : Positions)
    {
        if (!std::isfinite(position.x) || !std::isfinite(position.y))
            return "State positions must be finite.";
    }
    const AnimationParameters parameters(Parameters);
    if (!parameters.IsValid())
        return "Parameter names must be unique and values finite.";
    if (!OutputSource.empty())
    {
        const auto source = sources.find(OutputSource);
        if (source == sources.end())
            return "Missing graph output source: " + OutputSource;
        if (!source->second->CanBind(parameters, Library.Bindings(OutputSource)))
            return "Invalid parameter bindings for graph output: " + OutputSource;
        if (!source->second->IsValidPlayback(OutputLooping))
            return "Invalid output looping policy.";
    }
    const auto machine = ResolveMachine();
    if (!machine.States.empty() && !machine.IsValid())
        return "Check entry state, unique state names, sources, and transition endpoints.";
    for (const auto& state : machine.States)
    {
        if (!state.Node->CanBind(parameters, state.InputParameters))
            return "Invalid input bindings in state: " + state.Name;
    }
    for (const auto& transition : Machine.Transitions)
    {
        for (const auto& condition : transition.Conditions)
        {
            if (!condition.IsValid(parameters))
                return "Invalid condition parameter or type: " + condition.Parameter;
        }
    }
    return {};
}

Ref<AnimationGraph> AnimationGraphDocument::Compile() const
{
    if (!Validate().empty())
        return nullptr;
    SAnimationGraphCreateInfo info;
    info.Name = Name;
    info.Parameters = Parameters;
    if (OutputSource.empty())
    {
        auto machine = StateMachine::Create(ResolveMachine());
        if (!machine)
            return nullptr;
        info.Output = StateMachineNode::Create(std::move(machine));
    }
    else
    {
        std::string error;
        const auto sources = Library.Build(error);
        info.Output = sources.at(OutputSource);
        info.InputParameters = Library.Bindings(OutputSource);
        info.Looping = OutputLooping;
    }
    return AnimationGraph::Create(std::move(info));
}

SStateMachineCreateInfo AnimationGraphDocument::ResolveMachine() const
{
    auto machine = Machine;
    if (StateSources.empty())
        return machine;
    std::string error;
    const auto sources = Library.Build(error);
    for (size_t i = 0; i < machine.States.size() && i < StateSources.size(); ++i)
    {
        const auto source = sources.find(StateSources[i]);
        if (source != sources.end())
            machine.States[i].Node = source->second;
    }
    return machine;
}

bool AnimationGraphDocument::RenameState(const size_t index, const std::string& name)
{
    if (index >= Machine.States.size() || name.empty())
        return false;
    for (size_t i = 0; i < Machine.States.size(); ++i)
    {
        if (i != index && Machine.States[i].Name == name)
            return false;
    }
    const auto previous = Machine.States[index].Name;
    Machine.States[index].Name = name;
    if (Machine.InitialState == previous)
        Machine.InitialState = name;
    for (auto& transition : Machine.Transitions)
    {
        if (transition.From == previous)
            transition.From = name;
        if (transition.To == previous)
            transition.To = name;
    }
    return true;
}

void AnimationGraphDocument::RemoveState(const size_t index)
{
    if (index >= Machine.States.size())
        return;
    const auto name = Machine.States[index].Name;
    std::erase_if(
        Machine.Transitions,
        [&name](const auto& transition)
        {
            return transition.From == name || transition.To == name;
        }
    );
    Machine.States.erase(Machine.States.begin() + static_cast<std::ptrdiff_t>(index));
    Positions.erase(Positions.begin() + static_cast<std::ptrdiff_t>(index));
    if (index < StateSources.size())
        StateSources.erase(StateSources.begin() + static_cast<std::ptrdiff_t>(index));
    if (Machine.InitialState == name)
        Machine.InitialState = Machine.States.empty() ? "" : Machine.States.front().Name;
}

bool AnimationGraphDocument::Write(std::ostream& output, const Sources& sources) const
{
    if (!Validate().empty())
        return false;
    std::vector<std::string> names;
    for (size_t i = 0; i < Machine.States.size(); ++i)
    {
        if (i < StateSources.size() && !StateSources[i].empty())
        {
            names.push_back(StateSources[i]);
            continue;
        }
        const auto& state = Machine.States[i];
        const auto source = std::ranges::find_if(
            sources,
            [&state](const auto& entry)
            {
                return entry.second == state.Node;
            }
        );
        if (source == sources.end())
            return false;
        names.push_back(source->first);
    }
    output << "ELIXIR_ANIMATION_GRAPH 2\n";
    if (!Library.Write(output))
        return false;
    output << std::quoted(OutputSource) << ' ' << OutputLooping << '\n';
    output << std::setprecision(9) << std::quoted(Name) << ' '
           << std::quoted(Machine.InitialState) << '\n'
           << Parameters.size() << '\n';
    for (const auto& parameter : Parameters)
    {
        const bool boolean = std::holds_alternative<bool>(parameter.DefaultValue);
        const float value = boolean ? float(std::get<bool>(parameter.DefaultValue))
                                    : std::get<float>(parameter.DefaultValue);
        output << std::quoted(parameter.Name) << ' ' << boolean << ' ' << value << '\n';
    }
    output << Machine.States.size() << '\n';
    for (size_t i = 0; i < Machine.States.size(); ++i)
    {
        const auto& state = Machine.States[i];
        output << std::quoted(state.Name) << ' ' << std::quoted(names[i]) << ' '
               << state.Looping << ' ' << Positions[i].x << ' ' << Positions[i].y << ' '
               << state.InputParameters.size();
        for (const auto& binding : state.InputParameters)
            output << ' ' << std::quoted(binding);
        output << '\n';
    }
    output << Machine.Transitions.size() << '\n';
    for (const auto& transition : Machine.Transitions)
    {
        output << std::quoted(transition.From) << ' ' << std::quoted(transition.To) << ' '
               << transition.FadeDuration << ' ' << transition.WaitForCompletion << ' '
               << transition.Conditions.size() << '\n';
        for (const auto& condition : transition.Conditions)
        {
            const bool boolean = std::holds_alternative<bool>(condition.Value);
            const float value = boolean ? float(std::get<bool>(condition.Value))
                                        : std::get<float>(condition.Value);
            output << std::quoted(condition.Parameter) << ' ' << int(condition.Comparison)
                   << ' ' << boolean << ' ' << value << '\n';
        }
    }
    return bool(output);
}

bool AnimationGraphDocument::Read(std::istream& input, const Sources& sources)
{
    AnimationGraphDocument candidate;
    std::string magic;
    int version = 0;
    input >> magic >> version;
    if (magic != "ELIXIR_ANIMATION_GRAPH" || (version != 1 && version != 2))
        return false;
    Sources available = sources;
    if (version == 2)
    {
        if (!candidate.Library.Read(input))
            return false;
        input >> std::quoted(candidate.OutputSource) >> candidate.OutputLooping;
        std::string error;
        for (const auto& [name, source] : candidate.Library.Build(error))
            available.insert_or_assign(name, source);
    }
    input >> std::quoted(candidate.Name) >> std::quoted(candidate.Machine.InitialState);
    // Bound counts before allocating or following nested records in an untrusted file.
    const auto readCount = [&input](size_t& count)
    {
        return bool(input >> count) && count <= 4096;
    };
    const auto readValue = [&input](TAnimationParameterValue& result)
    {
        int boolean = 0;
        float value = 0;
        if (!(input >> boolean >> value) || !std::isfinite(value))
            return false;
        if (boolean == 0)
            result = value;
        else if (boolean == 1 && (value == 0 || value == 1))
            result = value != 0;
        else
            return false;
        return true;
    };
    size_t count = 0;
    if (!readCount(count))
        return false;
    for (size_t i = 0; i < count; ++i)
    {
        SAnimationParameter parameter;
        input >> std::quoted(parameter.Name);
        if (!readValue(parameter.DefaultValue))
            return false;
        candidate.Parameters.push_back(std::move(parameter));
    }
    if (!readCount(count))
        return false;
    for (size_t i = 0; i < count; ++i)
    {
        SAnimationState state;
        std::string source;
        glm::vec2 position;
        input >> std::quoted(state.Name) >> std::quoted(source) >> state.Looping >>
            position.x >> position.y;
        const auto found = available.find(source);
        if (found == available.end())
            return false;
        state.Node = found->second;
        size_t bindings = 0;
        if (!readCount(bindings))
            return false;
        for (size_t j = 0; j < bindings; ++j)
        {
            std::string binding;
            input >> std::quoted(binding);
            state.InputParameters.push_back(std::move(binding));
        }
        candidate.Machine.States.push_back(std::move(state));
        candidate.StateSources.push_back(source);
        candidate.Positions.push_back(position);
    }
    if (!readCount(count))
        return false;
    for (size_t i = 0; i < count; ++i)
    {
        SAnimationTransition transition;
        input >> std::quoted(transition.From) >> std::quoted(transition.To) >>
            transition.FadeDuration >> transition.WaitForCompletion;
        size_t conditions = 0;
        if (!readCount(conditions))
            return false;
        for (size_t j = 0; j < conditions; ++j)
        {
            SAnimationCondition condition;
            int comparison = 0;
            input >> std::quoted(condition.Parameter) >> comparison;
            if (comparison < 0 || comparison > int(EAnimationComparison::GreaterEqual))
                return false;
            condition.Comparison = EAnimationComparison(comparison);
            if (!readValue(condition.Value))
                return false;
            transition.Conditions.push_back(std::move(condition));
        }
        candidate.Machine.Transitions.push_back(std::move(transition));
    }
    if (!input || !candidate.Validate().empty())
        return false;
    input >> std::ws;
    if (!input.eof())
        return false;
    *this = std::move(candidate);
    return true;
}
