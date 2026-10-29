#include "epch.h"
#include "Animation.h"

#include <Engine/Animation/Pose.h>

namespace Elixir
{
    Ref<Animation> Animation::Create(
        Ref<const Skeleton> skeleton,
        const SLocalPose& referencePose
    )
    {
        if (!skeleton || !referencePose.IsValid(skeleton->GetBones().size()))
        {
            EE_CORE_ERROR("Animation requires a valid skeleton and reference pose.")
            return nullptr;
        }

        auto animation = Ref<Animation>(new Animation(
            std::move(skeleton),
            referencePose
        ));

        if (!animation->GetFrame().IsValid(animation->GetSkeleton()->GetBones().size()))
        {
            EE_CORE_ERROR("Animation reference pose cannot produce a valid frame.")
            return nullptr;
        }

        return animation;
    }

    bool Animation::BindLayerInputs(
        const std::string_view layerName,
        const std::span<const std::string> bindings
    )
    {
        auto* layer = GetLayer(layerName);
        return layer && layer->GetPlayer().BindParameters(m_Parameters, bindings);
    }

    bool Animation::RefreshPose()
    {
        SAnimationSample sample;
        SModelPose model;

        if (!ComposePose({}, sample, model))
            return false;

        m_Frame.Sample = std::move(sample);
        m_Frame.ModelPose = std::move(model);
        m_Frame.RootMotion = {};

        return true;
    }
    
    bool Animation::Play(Ref<const AnimationNode> source, const bool looping)
    {
        return Play(std::move(source), 0.0f, looping);
    }

    bool Animation::Play(
        Ref<const AnimationNode> source,
        const float fadeDuration,
        const bool looping,
        const EAnimationTransitionMode mode
    )
    {
        auto candidate = GetBaseLayer();
        if (!candidate.GetPlayer().Play(std::move(source), fadeDuration, looping, mode) ||
            !ApplyBaseLayer(candidate))
            return false;

        m_Paused = false;

        return true;
    }

    void Animation::Stop()
    {
        GetBaseLayer().GetPlayer().Stop();

        ClearLayers();
        m_PoseModifiers.clear();
        m_Frame.NotifyEvents.clear();
        m_Frame.RootMotion = {};

        m_Paused = false;

        m_Frame.Sample = GetBaseLayer().GetPlayer().GetSample();
        m_Frame.ModelPose.Matrices.clear();

        if (m_Skeleton)
            m_Skeleton->BuildModelPose(m_Frame.Sample.Pose, m_Frame.ModelPose);
    }

    bool Animation::Seek(const float time)
    {
        auto candidate = GetBaseLayer();
        return candidate.GetPlayer().Seek(time) && ApplyBaseLayer(candidate);
    }

    bool Animation::Update(const float dt)
    {
        if (!std::isfinite(dt) || dt < 0.0f)
        {
            EE_CORE_ERROR("Animation requires a valid delta time.")
            return false;
        }

        if (!m_Skeleton)
        {
            m_Frame.NotifyEvents = std::move(m_PendingNotifyEnds);
            m_PendingNotifyEnds.clear();
            m_Frame.RootMotion = {};
            return true;
        }

        const auto advance = m_Paused ? 0.0f : dt;
        std::vector<SAnimationPlayerEvaluation> evaluations(m_Layers.size());

        auto events = m_PendingNotifyEnds;
        size_t generatedEvents = 0;

        for (size_t i = 0; i < m_Layers.size(); ++i)
        {
            if (!m_Layers[i]->GetPlayer().Evaluate(advance, evaluations[i], m_Paused))
                return false;

            const auto& layerEvents = evaluations[i].Notifies.Events;

            const auto authoredEvents = layerEvents.size() -
                m_Layers[i]->GetPlayer().GetPendingNotifyEnds().size();

            const auto maxEvents = Sequence::MaxNotifyEventsPerUpdate - generatedEvents;

            if (authoredEvents > maxEvents)
            {
                EE_CORE_ERROR("Animation layers exceeded the combined notify event capacity.")
                return false;
            }

            generatedEvents += authoredEvents;
            events.insert(events.end(), layerEvents.begin(), layerEvents.end());
        }

        SRootMotionDelta movement;
        if (m_RootMotionEnabled)
        {
            const SRootMotionIntervalContext interval{
                .Reference = m_ReferencePose.LocalTransforms[m_RootMotionBone],
                .BoneIndex = m_RootMotionBone,
                .DeltaSeconds = advance,
            };

            const auto success = GetBaseLayer().GetPlayer().ExtractRootMotion(
                interval,
                movement,
                evaluations.front()
            );

            if (!success) return false;
        }

        if (m_RootMotionEnabled)
        {
            movement.Translation *= m_RootMotionParentScale;
            movement = m_RootMotionParent * movement * m_RootMotionParent.Inverse();
            if (!movement.IsValid())
            {
                EE_CORE_ERROR("Root motion produced a nonfinite mesh-local movement delta.")
                return false;
            }
        }

        SAnimationSample sample;
        SModelPose model;

        if (!ComposePose(evaluations, sample, model))
            return false;

        std::ranges::stable_sort(events, {}, &SAnimationNotifyEvent::TimeOffset);

        SAnimationFrame frame{
            std::move(sample),
            std::move(model),
            std::move(events),
            movement
        };

        for (size_t i = 0; i < m_Layers.size(); ++i)
            m_Layers[i]->GetPlayer().Apply(std::move(evaluations[i]));

        m_Frame = std::move(frame);
        m_PendingNotifyEnds.clear();

        return true;
    }

    bool Animation::SetRootMotionEnabled(
        const bool enabled,
        const std::string_view boneName
    )
    {
        if (!m_Skeleton)
        {
            EE_CORE_ERROR("Root motion requires a valid skeletal mesh.")
            return false;
        }

        const auto& bones = m_Skeleton->GetBones();
        const auto selected = boneName.empty()
            ? 0
            : m_Skeleton->FindBone(boneName);

        if (selected < 0 || size_t(selected) >= bones.size())
        {
            EE_CORE_ERROR("Root-motion bone '{}' does not exist.", boneName)
            return false;
        }

        glm::mat4 parentMatrix(1);
        glm::quat parentRotation(1, 0, 0, 0);
        float scale = 1;

        const auto& reference = m_ReferencePose;

        for (auto parent = bones[size_t(selected)].ParentIndex;
            parent >= 0;
            parent = bones[size_t(parent)].ParentIndex)
        {
            const auto& transform = reference.LocalTransforms[size_t(parent)];

            if (std::abs(transform.Scale.x - transform.Scale.y) > 1e-5f ||
                std::abs(transform.Scale.x - transform.Scale.z) > 1e-5f)
            {
                EE_CORE_ERROR("Root motion requires uniformly scaled reference ancestors.")
                return false;
            }

            parentMatrix = transform.ToMatrix() * parentMatrix;
            parentRotation = glm::normalize(transform.Rotation * parentRotation);
            scale *= transform.Scale.x;
        }

        const SRootMotionDelta parentFrame{glm::vec3(parentMatrix[3]), parentRotation};
        if (!parentFrame.IsValid() || !std::isfinite(scale) || scale == 0)
        {
            EE_CORE_ERROR("Root motion requires a finite, invertible reference parent transform.")
            return false;
        }

        const auto previous = m_RootMotionEnabled;
        const auto previousBone = m_RootMotionBone;
        const auto previousParent = m_RootMotionParent;
        const auto previousScale = m_RootMotionParentScale;

        m_RootMotionEnabled = enabled;
        m_RootMotionBone = uint32_t(selected);
        m_RootMotionParent = parentFrame;
        m_RootMotionParentScale = scale;

        SAnimationSample sample;
        SModelPose model;

        if (!ComposePose({}, sample, model))
        {
            m_RootMotionEnabled = previous;
            m_RootMotionBone = previousBone;
            m_RootMotionParent = previousParent;
            m_RootMotionParentScale = previousScale;
            return false;
        }

        m_Frame.Sample = std::move(sample);
        m_Frame.ModelPose = std::move(model);
        m_Frame.RootMotion = {};

        return true;
    }

    bool Animation::SetParameters(std::vector<SAnimationParameter> definitions)
    {
        if (GetLayerCount() != 1 || GetBaseLayer().GetPlayer().HasPose())
            return false;

        auto parameters = CreateRef<AnimationParameters>(std::move(definitions));
        if (!parameters->IsValid() || !GetBaseLayer().GetPlayer().BindParameters(parameters))
            return false;

        m_Parameters = std::move(parameters);
        return true;
    }

    std::string_view Animation::GetCurrentState(const std::string_view layerName) const
    {
        const auto* layer = GetLayer(layerName);
        return layer
            ? layer->GetPlayer().GetCurrentState()
            : std::string_view{};
    }

    bool Animation::SetLocalPose(const SLocalPose& pose)
    {
        auto candidate = GetBaseLayer();
        if (!m_Skeleton || !candidate.GetPlayer().SetLocalPose(pose))
            return false;

        SAnimationSample sample{ pose };

        if (m_RootMotionEnabled && !LockRootMotion(sample.Pose))
            return false;

        SModelPose model;

        if (!ApplyPoseModifiers(sample, model))
            return false;

        GetBaseLayer() = candidate;
        ClearLayers();

        m_Frame.NotifyEvents.clear();
        m_Frame.RootMotion = {};
        m_Paused = false;

        m_Frame.Sample = std::move(sample);
        m_Frame.ModelPose = std::move(model);

        return true;
    }

    bool Animation::IsPlaying() const
    {
        const auto someLayerPlaying = std::ranges::any_of(
            m_Layers,
            [](const auto& layer) { return layer->GetPlayer().IsPlaying(); }
        );
        return !m_Paused && someLayerPlaying;
    }

    bool Animation::AddLayer(
        std::string name,
        Ref<const BoneMask> mask,
        const float weight
    )
    {
        if (!m_Skeleton ||
            name.empty() ||
            GetLayer(name) ||
            !mask ||
            !std::isfinite(weight) ||
            weight < 0.0f ||
            weight > 1.0f ||
            (m_Skeleton != mask->GetSkeleton() &&
                !m_Skeleton->IsCompatible(*mask->GetSkeleton())))
        {
            EE_CORE_ERROR("Animation layers require a unique name, compatible mask, and valid weight.")
            return false;
        }

        Scope<AnimationLayer> layer(new AnimationLayer(
            std::move(name),
            std::move(mask),
            weight,
            m_ReferencePose
        ));

        if (!layer->GetPlayer().BindParameters(m_Parameters))
            return false;

        m_Layers.push_back(std::move(layer));
        return true;
    }

    AnimationLayer* Animation::GetLayer(const std::string_view name)
    {
        for (const auto& layer : m_Layers)
            if (layer->GetName() == name)
                return layer.get();

        AnimationLayer* result = nullptr;
        
        for (const auto& layer : m_Layers)
        {
            if (auto* nested = layer->GetPlayer().GetPlayback().GetLayer(name))
            {
                if (result) return nullptr;
                result = nested;
            }
        }

        return result;
    }

    const AnimationLayer* Animation::GetLayer(const std::string_view name) const
    {
        for (const auto& layer : m_Layers)
            if (layer->GetName() == name)
                return layer.get();

        const AnimationLayer* result = nullptr;

        for (const auto& layer : m_Layers)
        {
            if (const auto* nested = layer->GetPlayer().GetPlayback().GetLayer(name))
            {
                if (result) return nullptr;
                result = nested;
            }
        }

        return result;
    }

    size_t Animation::GetLayerCount() const
    {
        size_t count = 0;
        for (const auto& layer : m_Layers)
            count += std::max(size_t(1), layer->GetPlayer().GetPlayback().GetLayerCount());

        return count;
    }

    bool Animation::SetLayerWeight(const std::string_view name, const float weight)
    {
        if (auto* layer = GetLayer(name))
            return layer->SetWeight(weight);

        EE_CORE_ERROR("Animation layer '{}' does not exist.", name)
        return false;
    }

    std::optional<float> Animation::GetLayerWeight(const std::string_view name) const
    {
        if (const auto* layer = GetLayer(name))
            return layer->GetWeight();

        return std::nullopt;
    }

    bool Animation::RemoveLayer(const std::string_view name)
    {
        if (name == GetBaseLayer().GetName())
        {
            EE_CORE_ERROR("The base animation layer cannot be removed.")
            return false;
        }

        const auto layer = std::ranges::find_if(
            m_Layers,
            [&](const auto& entry) { return entry->GetName() == name; }
        );

        if (layer == m_Layers.end())
        {
            EE_CORE_ERROR("Animation layer '{}' does not exist.", name)
            return false;
        }

        QueueLayerNotifyEnds(**layer);
        m_Layers.erase(layer);

        return true;
    }

    void Animation::ClearLayers()
    {
        for (size_t i = 1; i < m_Layers.size(); ++i)
            QueueLayerNotifyEnds(*m_Layers[i]);

        m_Layers.erase(m_Layers.begin() + 1, m_Layers.end());
    }

    bool Animation::SetPoseModifiers(const std::span<const Ref<const PoseModifier>> modifiers)
    {
        for (const auto& modifier : modifiers)
        {
            if (!modifier)
            {
                EE_CORE_ERROR("Animation pose modifiers must not be null.")
                return false;
            }
        }

        std::vector replacement(modifiers.begin(), modifiers.end());
        m_PoseModifiers = std::move(replacement);

        return true;
    }

    Animation::Animation(
        Ref<const Skeleton> skeleton,
        const SLocalPose& referencePose
    ) : m_Skeleton(std::move(skeleton)),
        m_ReferencePose(referencePose)
    {
        const auto mask = BoneMask::Create(
            "Base",
            m_Skeleton,
            std::vector(m_Skeleton->GetBones().size(), 1.0f)
        );

        Scope<AnimationLayer> base(new AnimationLayer(
            "Base",
            mask,
            1.0f,
            referencePose
        ));

        base->m_Base = true;
        base->GetPlayer().BindParameters(m_Parameters);

        m_Layers.push_back(std::move(base));

        Stop();
    }

    bool Animation::ComposePose(
        const std::span<const SAnimationPlayerEvaluation> evaluations,
        SAnimationSample& sample,
        SModelPose& model,
        const AnimationLayer* replacement
    ) const
    {
        if (!m_Skeleton) return false;

        sample = { m_ReferencePose };

        for (size_t i = 0; i < m_Layers.size(); ++i)
        {
            const auto& layer = replacement && replacement->GetName() == m_Layers[i]->GetName()
                ? *replacement
                : *m_Layers[i];

            const auto& layerSample = evaluations.empty()
                ? layer.GetPlayer().GetSample()
                : evaluations[i].Sample;

            if (!layer.BlendSample(sample, layerSample))
                return false;

            if (i == 0 && m_RootMotionEnabled && !LockRootMotion(sample.Pose))
                return false;
        }

        return ApplyPoseModifiers(sample, model);
    }

    bool Animation::ApplyBaseLayer(const AnimationLayer& layer)
    {
        SAnimationSample sample;
        SModelPose model;

        if (!ComposePose({}, sample, model, &layer))
            return false;

        auto& baseLayer = GetBaseLayer();
        baseLayer = layer;

        m_Frame.NotifyEvents.clear();
        m_Frame.Sample = std::move(sample);
        m_Frame.ModelPose = std::move(model);
        m_Frame.RootMotion = {};

        return true;
    }

    bool Animation::LockRootMotion(SLocalPose& pose) const
    {
        const auto& reference = m_ReferencePose;
        const auto& bones = m_Skeleton->GetBones();

        for (auto parent = bones[m_RootMotionBone].ParentIndex;
            parent >= 0;
            parent = bones[size_t(parent)].ParentIndex)
        {
            const auto& actual = pose.LocalTransforms[size_t(parent)];
            const auto& expected = reference.LocalTransforms[size_t(parent)];

            if (!actual.IsNearlyEqual(expected))
            {
                EE_CORE_ERROR("Root motion requires ancestors to stay in their reference pose.")
                return false;
            }
        }

        pose.LocalTransforms[m_RootMotionBone].Translation =
            reference.LocalTransforms[m_RootMotionBone].Translation;

        pose.LocalTransforms[m_RootMotionBone].Rotation =
            reference.LocalTransforms[m_RootMotionBone].Rotation;

        return true;
    }

    void Animation::QueueLayerNotifyEnds(AnimationLayer& layer)
    {
        layer.GetPlayer().CancelNotifies();
        const auto& pending = layer.GetPlayer().GetPendingNotifyEnds();

        m_PendingNotifyEnds.insert(
            m_PendingNotifyEnds.end(),
            pending.begin(),
            pending.end()
        );
    }

    bool Animation::ApplyPoseModifiers(
        SAnimationSample& sample,
        SModelPose& model
    ) const
    {
        if (!m_Skeleton->BuildModelPose(sample.Pose, model))
            return false;

        const SPoseModifierContext context{
            .Rig = *m_Skeleton,
            .RootMotionBone = m_RootMotionEnabled
                ? std::optional(m_RootMotionBone)
                : std::nullopt,
            .Curves = &sample.Curves
        };

        for (const auto& modifier : m_PoseModifiers)
        {
            const auto lockedRoot = m_RootMotionEnabled
                ? model.Matrices[m_RootMotionBone]
                : glm::mat4(1);

            if (!modifier->Apply(context, sample.Pose) ||
                !m_Skeleton->BuildModelPose(sample.Pose, model))
                return false;

            if (m_RootMotionEnabled)
            {
                for (int column = 0; column < 4; ++column)
                {
                    for (int row = 0; row < 4; ++row)
                    {
                        const auto matrix = model.Matrices[m_RootMotionBone][column][row];
                        const auto delta = matrix - lockedRoot[column][row];

                        if (std::abs(delta) > SBoneTransform::BoneTransformTolerance)
                        {
                            EE_CORE_ERROR("A pose modifier changed the extracted root frame.")
                            return false;
                        }
                    }
                }
            }
        }

        return true;
    }
}
