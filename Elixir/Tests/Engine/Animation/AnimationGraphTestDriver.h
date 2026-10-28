#pragma once

#include "AnimationTestCharacter.h"

#include <Engine/Animation/AnimationGraph.h>
#include <Engine/Animation/StateMachine.h>

/** Test authoring fixture for the previous ordered-layer regression cases. */
struct SAnimationGraphTestInfo
{
    std::string Name;
    std::vector<Elixir::SAnimationParameter> Parameters;
    std::vector<Elixir::SBlendNodeInput> Layers;
};

/**
 * @brief Build an explicit composition node for existing regression fixtures.
 * @param info Parameters and ordered input connections used by the fixture.
 */
Elixir::Ref<Elixir::AnimationGraph> CreateTestGraph(SAnimationGraphTestInfo info);

/**
 * @brief Inspect the composition inputs used by a regression fixture.
 * @param graph Graph whose output is an BlendNode node.
 */
const std::vector<Elixir::SBlendNodeInput>& GetTestGraphInputs(
    const Elixir::AnimationGraph& graph);

/** Test harness for the single character update entry, without a second engine controller. */
class AnimationGraphTestDriver
{
public:
    /**
     * @brief Configure a test character with a generated animation.
     * @param controller Shared immutable definition.
     * @param mesh Character under test.
     */
    AnimationGraphTestDriver(Elixir::Ref<const Elixir::AnimationGraph> controller,
        Elixir::AnimationTestCharacter& mesh);

    /**
     * @brief Set a numeric input on the character.
     * @param name Numeric parameter name.
     * @param value New finite value.
     */
    bool SetFloat(std::string_view name, float value);

    /**
     * @brief Set a boolean input on the character.
     * @param name Boolean parameter name.
     * @param value New value.
     */
    bool SetBool(std::string_view name, bool value);

    /**
     * @brief Read a numeric character parameter.
     * @param name Parameter name.
     */
    std::optional<float> GetFloat(std::string_view name) const;

    /**
     * @brief Read a boolean character parameter.
     * @param name Parameter name.
     */
    std::optional<bool> GetBool(std::string_view name) const;

    /**
     * @brief Evaluate the character through its sole update entry.
     * @param dt Elapsed seconds.
     */
    bool Update(float dt);

    /** @brief Restart the character configuration. */
    void Reset();

    /**
     * @brief Inspect a committed machine state.
     * @param name Layer name.
     */
    std::string_view GetCurrentState(std::string_view name = "Base") const;

private:
    Elixir::AnimationTestCharacter& m_Mesh;
    Elixir::Ref<const Elixir::AnimationGraph> m_Controller;
    Elixir::Ref<Elixir::Animation> m_Animation;
};

namespace AnimationTestAuthoring
{
using namespace Elixir;
    /**
     * @brief Rebuild an immutable graph after editing a local authoring copy.
     * @tparam TEdit Callable receiving the local graph definition.
     * @param source Source to replace with the validated edited graph.
     * @param edit Changes to apply before graph validation.
     */
    template<typename TEdit>
    void EditMachine(Ref<const AnimationNode>& source, TEdit edit)
    {
        auto machine = std::dynamic_pointer_cast<const StateMachineNode>(source);
        auto definition = machine ? machine->GetStateMachine()->GetCreateInfo() : SStateMachineCreateInfo{};
        edit(definition);
        source = MakeNode(StateMachine::Create(std::move(definition)));
    }

}

using AnimationTestAuthoring::EditMachine;
