#pragma once

#include <Engine/Animation/AnimationMontage.h>
#include <Engine/Animation/BlendSpace1D.h>
#include <Engine/Animation/BlendSpace2D.h>
#include <Engine/Animation/AnimationNode.h>

#include <iosfwd>
#include <map>

/** Selects the editable resource represented by a graph node. */
enum class EAnimationAssetKind
{
    Sequence,
    BlendSpace,
    Montage
};

/** Editable sequence data retained independently of compiled runtime resources. */
struct SAnimationSequenceAsset
{
    std::string Name;
    Elixir::Ref<const Elixir::Skeleton> Skeleton;
    float Duration = 1;
    std::vector<Elixir::SBoneAnimationTrack> Tracks;
    std::vector<Elixir::SAnimationSyncMarker> Markers;
    std::vector<Elixir::SAnimationNotify> Notifies;
    glm::vec2 Position{0, 0};
};

/** Associates a named sequence with one point in a blend space. */
struct SAnimationBlendSample
{
    std::string Sequence;
    glm::vec2 Position{};
};

/** Editable one- or two-axis blend space and its default parameter bindings. */
struct SAnimationBlendAsset
{
    std::string Name;
    bool TwoDimensional = false;
    std::string XParameter = "Speed";
    std::string YParameter = "Direction";
    std::vector<SAnimationBlendSample> Samples;
    glm::vec2 Position{300, 0};
};

/** Editable montage action layered over the graph's continuing pose output. */
struct SAnimationMontageAsset
{
    std::string Name;
    std::string Sequence;
    float BlendIn = 0.15f;
    float BlendOut = 0.15f;
    bool Interruptible = true;
    std::vector<Elixir::SAnimationMontageSection> Sections;
    glm::vec2 Position{300, 300};
};

/** Compiles authored resources and persists their complete definitions. */
class AnimationGraphLibrary
{
  public:
    using Sources = std::map<std::string, Elixir::Ref<const Elixir::AnimationNode>>;
    std::vector<SAnimationSequenceAsset> Sequences;
    std::vector<SAnimationBlendAsset> BlendSpaces;
    std::vector<SAnimationMontageAsset> Montages;

    /**
     * @brief Compile sequences and blend spaces without modifying authored data.
     * @param error Receives a resource-specific failure description, or empty on success.
     */
    Sources Build(std::string& error) const;
    /**
     * @brief Compile a montage action against this library's sequences.
     * @param index Montage index.
     */
    Elixir::Ref<Elixir::AnimationMontage> BuildMontage(size_t index) const;
    /**
     * @brief Compile the sequence at an authored index.
     * @param index Sequence index.
     */
    Elixir::Ref<Elixir::Sequence> BuildSequence(size_t index) const;
    /**
     * @brief Get default numeric input names for a pose source.
     * @param name Sequence or blend-space name.
     */
    std::vector<std::string> Bindings(std::string_view name) const;
    /**
     * @brief Rename a resource and update sequence references in blends and montages.
     * @param kind Resource category.
     * @param index Resource index in its category.
     * @param name Unique nonempty resource name.
     */
    bool Rename(EAnimationAssetKind kind, size_t index, const std::string& name);
    /**
     * @brief Write complete skeletons, tracks, markers, notifies, samples, and sections.
     * @param output Destination stream.
     */
    bool Write(std::ostream& output) const;
    /**
     * @brief Read resource definitions, preserving this library on failure.
     * @param input Serialized library stream.
     */
    bool Read(std::istream& input);
};
