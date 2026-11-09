#include "AnimationGraphLibrary.h"

#include <Engine/Animation/SequenceNode.h>
#include <Engine/Animation/BlendSpaceNode.h>

#include <cmath>
#include <iomanip>
#include <istream>
#include <ostream>
#include <set>

using namespace Elixir;

namespace
{
    /**
     * @brief Reject unbounded serialized collection counts before allocating storage.
     * @param input Source stream.
     * @param count Receives a bounded count.
     */
    bool Count(std::istream& input, size_t& count)
    {
        return bool(input >> count) && count <= 4096;
    }

    /**
     * @brief Write vector or quaternion keys without changing their authored order.
     * @param output Destination stream.
     * @param keys Ordered animation keys.
     */
    template <typename T>
    void WriteKeys(std::ostream& output, const std::vector<T>& keys)
    {
        output << keys.size() << '\n';
        for (const auto& key : keys)
        {
            output << key.Time << ' ' << key.Value.x << ' ' << key.Value.y << ' '
                   << key.Value.z;
            if constexpr (std::is_same_v<T, SRotationAnimationKey>)
                output << ' ' << key.Value.w;
            output << '\n';
        }
    }

    /**
     * @brief Read vector or quaternion keys with bounded allocation.
     * @param input Source stream.
     * @param keys Receives authored keys.
     */
    template <typename T>
    bool ReadKeys(std::istream& input, std::vector<T>& keys)
    {
        size_t count;
        if (!Count(input, count))
            return false;
        keys.resize(count);
        for (auto& key : keys)
        {
            input >> key.Time >> key.Value.x >> key.Value.y >> key.Value.z;
            if constexpr (std::is_same_v<T, SRotationAnimationKey>)
                input >> key.Value.w;
        }
        return bool(input);
    }
}

Ref<Sequence> AnimationGraphLibrary::BuildSequence(size_t index) const
{
    if (index >= Sequences.size())
        return nullptr;
    const auto& sequence = Sequences[index];
    return Sequence::Create(
        sequence.Name,
        sequence.Skeleton,
        sequence.Duration,
        sequence.Tracks,
        sequence.Markers,
        sequence.Notifies
    );
}

AnimationGraphLibrary::Sources AnimationGraphLibrary::Build(std::string& error) const
{
    error.clear();
    Sources sources;
    std::map<std::string, Ref<const Sequence>> clips;
    std::set<std::string> names;
    const auto unique = [&names](const std::string& name)
    {
        return !name.empty() && names.insert(name).second;
    };
    for (size_t i = 0; i < Sequences.size(); ++i)
    {
        const auto& sequence = Sequences[i];
        if (!std::isfinite(sequence.Position.x) || !std::isfinite(sequence.Position.y))
        {
            error = "Invalid canvas position: " + sequence.Name;
            return {};
        }
        const auto clip = BuildSequence(i);
        if (!clip || !unique(sequence.Name))
        {
            error = "Invalid sequence: " + sequence.Name;
            return {};
        }
        clips.emplace(sequence.Name, clip);
        sources.emplace(sequence.Name, SequenceNode::Create(clip));
    }
    for (const auto& blend : BlendSpaces)
    {
        if (!std::isfinite(blend.Position.x) || !std::isfinite(blend.Position.y))
        {
            error = "Invalid canvas position: " + blend.Name;
            return {};
        }
        if (!unique(blend.Name))
        {
            error = "Duplicate or empty source name: " + blend.Name;
            return {};
        }
        std::vector<SBlendSpace1DSample> one;
        std::vector<SBlendSpace2DSample> two;
        for (const auto& sample : blend.Samples)
        {
            const auto clip = clips.find(sample.Sequence);
            if (clip == clips.end())
            {
                error = "Missing sequence in " + blend.Name + ": " + sample.Sequence;
                return {};
            }
            one.push_back({sample.Position.x, clip->second});
            two.push_back({sample.Position, clip->second});
        }
        Ref<const BlendSpace> resource;
        if (blend.TwoDimensional)
            resource = BlendSpace2D::Create(blend.Name, std::move(two));
        else
            resource = BlendSpace1D::Create(blend.Name, std::move(one));
        if (!resource)
        {
            error =
                "Invalid samples in " + blend.Name + ". Check unique coordinates and 2D area.";
            return {};
        }
        sources.emplace(blend.Name, BlendSpaceNode::Create(resource));
    }
    for (size_t i = 0; i < Montages.size(); ++i)
    {
        if (!std::isfinite(Montages[i].Position.x) || !std::isfinite(Montages[i].Position.y))
        {
            error = "Invalid canvas position: " + Montages[i].Name;
            return {};
        }
        if (!unique(Montages[i].Name) || !BuildMontage(i))
        {
            error = "Invalid montage: " + Montages[i].Name;
            return {};
        }
    }
    return sources;
}

Ref<AnimationMontage> AnimationGraphLibrary::BuildMontage(size_t index) const
{
    if (index >= Montages.size())
        return nullptr;
    const auto& montage = Montages[index];
    for (size_t i = 0; i < Sequences.size(); ++i)
    {
        if (Sequences[i].Name != montage.Sequence)
            continue;
        SAnimationMontageCreateInfo info;
        info.Name = montage.Name;
        info.Sequence = BuildSequence(i);
        info.BlendIn = montage.BlendIn;
        info.BlendOut = montage.BlendOut;
        info.Interruptible = montage.Interruptible;
        info.Sections = montage.Sections;
        return AnimationMontage::Create(std::move(info));
    }
    return nullptr;
}

std::vector<std::string> AnimationGraphLibrary::Bindings(std::string_view name) const
{
    for (const auto& blend : BlendSpaces)
    {
        if (blend.Name != name)
            continue;
        if (blend.TwoDimensional)
            return {blend.XParameter, blend.YParameter};
        return {blend.XParameter};
    }
    return {};
}

bool AnimationGraphLibrary::Rename(
    EAnimationAssetKind kind,
    size_t index,
    const std::string& name
)
{
    if (name.empty())
        return false;
    std::string* current = nullptr;
    if (kind == EAnimationAssetKind::Sequence && index < Sequences.size())
        current = &Sequences[index].Name;
    if (kind == EAnimationAssetKind::BlendSpace && index < BlendSpaces.size())
        current = &BlendSpaces[index].Name;
    if (kind == EAnimationAssetKind::Montage && index < Montages.size())
        current = &Montages[index].Name;
    if (!current)
        return false;
    for (const auto& entry : Sequences)
        if (&entry.Name != current && entry.Name == name)
            return false;
    for (const auto& entry : BlendSpaces)
        if (&entry.Name != current && entry.Name == name)
            return false;
    for (const auto& entry : Montages)
        if (&entry.Name != current && entry.Name == name)
            return false;
    const auto previous = *current;
    *current = name;
    if (kind == EAnimationAssetKind::Sequence)
    {
        for (auto& blend : BlendSpaces)
            for (auto& sample : blend.Samples)
                if (sample.Sequence == previous)
                    sample.Sequence = name;
        for (auto& montage : Montages)
            if (montage.Sequence == previous)
                montage.Sequence = name;
    }
    return true;
}

bool AnimationGraphLibrary::Write(std::ostream& output) const
{
    output << std::setprecision(9) << Sequences.size() << '\n';
    for (const auto& sequence : Sequences)
    {
        if (!sequence.Skeleton)
            return false;
        output << std::quoted(sequence.Name) << ' ' << sequence.Duration << ' '
               << sequence.Position.x << ' ' << sequence.Position.y << '\n'
               << std::quoted(sequence.Skeleton->GetName()) << ' '
               << sequence.Skeleton->GetBones().size() << '\n';
        for (const auto& bone : sequence.Skeleton->GetBones())
        {
            const auto& value = bone.ReferenceTransform;
            output << std::quoted(bone.Name) << ' ' << bone.ParentIndex << ' '
                   << value.Translation.x << ' ' << value.Translation.y << ' '
                   << value.Translation.z << ' ' << value.Rotation.x << ' ' << value.Rotation.y
                   << ' ' << value.Rotation.z << ' ' << value.Rotation.w << ' '
                   << value.Scale.x << ' ' << value.Scale.y << ' ' << value.Scale.z << '\n';
        }
        output << sequence.Tracks.size() << '\n';
        for (const auto& track : sequence.Tracks)
        {
            output << track.BoneIndex << ' ' << int(track.TranslationInterpolation) << ' '
                   << int(track.RotationInterpolation) << ' ' << int(track.ScaleInterpolation)
                   << '\n';
            WriteKeys(output, track.Translations);
            WriteKeys(output, track.Rotations);
            WriteKeys(output, track.Scales);
        }
        output << sequence.Markers.size() << '\n';
        for (const auto& marker : sequence.Markers)
            output << std::quoted(marker.Name) << ' ' << marker.Time << '\n';
        output << sequence.Notifies.size() << '\n';
        for (const auto& notify : sequence.Notifies)
        {
            if (notify.Payload.has_value())
                return false;
            output << std::quoted(notify.Name) << ' ' << notify.Time << ' ' << notify.Duration
                   << '\n';
        }
    }
    output << BlendSpaces.size() << '\n';
    for (const auto& blend : BlendSpaces)
    {
        output << std::quoted(blend.Name) << ' ' << blend.TwoDimensional << ' '
               << std::quoted(blend.XParameter) << ' ' << std::quoted(blend.YParameter) << ' '
               << blend.Position.x << ' ' << blend.Position.y << ' ' << blend.Samples.size()
               << '\n';
        for (const auto& sample : blend.Samples)
            output << std::quoted(sample.Sequence) << ' ' << sample.Position.x << ' '
                   << sample.Position.y << '\n';
    }
    output << Montages.size() << '\n';
    for (const auto& montage : Montages)
    {
        output << std::quoted(montage.Name) << ' ' << std::quoted(montage.Sequence) << ' '
               << montage.BlendIn << ' ' << montage.BlendOut << ' ' << montage.Interruptible
               << ' ' << montage.Position.x << ' ' << montage.Position.y << ' '
               << montage.Sections.size() << '\n';
        for (const auto& section : montage.Sections)
            output << std::quoted(section.Name) << ' ' << section.StartTime << ' '
                   << bool(section.NextSection) << ' '
                   << std::quoted(section.NextSection.value_or("")) << '\n';
    }
    return bool(output);
}

bool AnimationGraphLibrary::Read(std::istream& input)
{
    AnimationGraphLibrary candidate;
    size_t count;
    if (!Count(input, count))
        return false;
    for (size_t i = 0; i < count; ++i)
    {
        SAnimationSequenceAsset sequence;
        std::string skeletonName;
        input >> std::quoted(sequence.Name) >> sequence.Duration >> sequence.Position.x >>
            sequence.Position.y >> std::quoted(skeletonName);
        size_t bones;
        if (!Count(input, bones))
            return false;
        std::vector<SBone> skeleton(bones);
        for (auto& bone : skeleton)
        {
            auto& value = bone.ReferenceTransform;
            input >> std::quoted(bone.Name) >> bone.ParentIndex >> value.Translation.x >>
                value.Translation.y >> value.Translation.z >> value.Rotation.x >>
                value.Rotation.y >> value.Rotation.z >> value.Rotation.w >> value.Scale.x >>
                value.Scale.y >> value.Scale.z;
        }
        sequence.Skeleton = Skeleton::Create(skeletonName, std::move(skeleton));
        size_t tracks;
        if (!Count(input, tracks))
            return false;
        sequence.Tracks.resize(tracks);
        for (auto& track : sequence.Tracks)
        {
            int translation, rotation, scale;
            input >> track.BoneIndex >> translation >> rotation >> scale;
            if (translation < 0 || translation > 1 || rotation < 0 || rotation > 1 ||
                scale < 0 || scale > 1)
                return false;
            track.TranslationInterpolation = EBoneInterpolation(translation);
            track.RotationInterpolation = EBoneInterpolation(rotation);
            track.ScaleInterpolation = EBoneInterpolation(scale);
            if (!ReadKeys(input, track.Translations) || !ReadKeys(input, track.Rotations) ||
                !ReadKeys(input, track.Scales))
                return false;
        }
        size_t markers;
        if (!Count(input, markers))
            return false;
        sequence.Markers.resize(markers);
        for (auto& marker : sequence.Markers)
            input >> std::quoted(marker.Name) >> marker.Time;
        size_t notifies;
        if (!Count(input, notifies))
            return false;
        sequence.Notifies.resize(notifies);
        for (auto& notify : sequence.Notifies)
            input >> std::quoted(notify.Name) >> notify.Time >> notify.Duration;
        candidate.Sequences.push_back(std::move(sequence));
    }
    if (!Count(input, count))
        return false;
    for (size_t i = 0; i < count; ++i)
    {
        SAnimationBlendAsset blend;
        input >> std::quoted(blend.Name) >> blend.TwoDimensional >>
            std::quoted(blend.XParameter) >> std::quoted(blend.YParameter) >>
            blend.Position.x >> blend.Position.y;
        size_t samples;
        if (!Count(input, samples))
            return false;
        blend.Samples.resize(samples);
        for (auto& sample : blend.Samples)
            input >> std::quoted(sample.Sequence) >> sample.Position.x >> sample.Position.y;
        candidate.BlendSpaces.push_back(std::move(blend));
    }
    if (!Count(input, count))
        return false;
    for (size_t i = 0; i < count; ++i)
    {
        SAnimationMontageAsset montage;
        input >> std::quoted(montage.Name) >> std::quoted(montage.Sequence) >>
            montage.BlendIn >> montage.BlendOut >> montage.Interruptible >>
            montage.Position.x >> montage.Position.y;
        size_t sections;
        if (!Count(input, sections))
            return false;
        montage.Sections.resize(sections);
        for (auto& section : montage.Sections)
        {
            bool hasNext;
            std::string next;
            input >> std::quoted(section.Name) >> section.StartTime >> hasNext >>
                std::quoted(next);
            if (hasNext)
                section.NextSection = next;
        }
        candidate.Montages.push_back(std::move(montage));
    }
    if (!input)
        return false;
    std::string error;
    candidate.Build(error);
    if (!error.empty())
        return false;
    *this = std::move(candidate);
    return true;
}
