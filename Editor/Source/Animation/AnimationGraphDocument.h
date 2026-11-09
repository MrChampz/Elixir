#pragma once

#include <Engine/Animation/AnimationGraph.h>
#include <Engine/Animation/StateMachine.h>
#include "AnimationGraphLibrary.h"

#include <iosfwd>
#include <map>

/** Editable pose output, state graph, and animation resource library. */
class AnimationGraphDocument
{
  public:
    /** Sources are resolved by name when loading a document. */
    using Sources = std::map<std::string, Elixir::Ref<const Elixir::AnimationNode>>;

    std::string Name = "Animation Graph";
    Elixir::SStateMachineCreateInfo Machine;
    std::vector<Elixir::SAnimationParameter> Parameters;
    /** Canvas positions in the same order as Machine.States. */
    std::vector<glm::vec2> Positions;
    AnimationGraphLibrary Library;
    /**
     * Stable source names in state order; empty retains application-provided node pointers.
     */
    std::vector<std::string> StateSources;
    /** Empty selects the state machine; otherwise selects a library pose source. */
    std::string OutputSource;
    bool OutputLooping = true;

    /** @brief Resolve current library definitions into the authored state connections. */
    Elixir::SStateMachineCreateInfo ResolveMachine() const;

    /**
     * @brief Check authoring errors before creating runtime resources.
     * @return Empty when the graph can be compiled.
     */
    std::string Validate() const;

    /** @brief Compile a fresh immutable runtime graph, or return null on error. */
    Elixir::Ref<Elixir::AnimationGraph> Compile() const;

    /**
     * @brief Rename a state and update its transition endpoints and entry reference.
     * @param index State index.
     * @param name Unique nonempty replacement name.
     */
    bool RenameState(size_t index, const std::string& name);

    /**
     * @brief Remove a state and its transitions, selecting a new entry when needed.
     * @param index State index.
     */
    void RemoveState(size_t index);

    /**
     * @brief Write a versioned document without serializing runtime pointers.
     * @param output Destination stream.
     * @param sources Registered source names.
     */
    bool Write(std::ostream& output, const Sources& sources) const;

    /**
     * @brief Load and validate a document, preserving this document on failure.
     * @param input Serialized document.
     * @param sources Sources available in this Editor session.
     */
    bool Read(std::istream& input, const Sources& sources);
};
