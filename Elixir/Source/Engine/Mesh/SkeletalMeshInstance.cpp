#include "epch.h"
#include "SkeletalMeshInstance.h"

namespace Elixir
{
    SkeletalMeshInstance::SkeletalMeshInstance(Ref<const SkeletalMesh> mesh)
        : m_Mesh(std::move(mesh))
    {
        const auto skeleton = m_Mesh ? m_Mesh->GetSkeleton() : nullptr;

        const auto referencePose = m_Mesh
            ? std::span(m_Mesh->GetData().ReferencePose)
            : std::span<const SBoneTransform>{};

        const auto mask = skeleton
            ? BoneMask::Create(
                "Base",
                skeleton,
                std::vector(skeleton->GetBones().size(), 1.0f)
              )
            : nullptr;

        Scope<AnimationLayer> base(new AnimationLayer(
            "Base",
            mask,
            1.0f,
            referencePose
        ));
        base->m_Base = true;

        m_Layers.push_back(std::move(base));

        Stop();
    }

    void SkeletalMeshInstance::Stop()
    {
        GetBaseLayer().Stop();
        ClearLayers();
        m_NotifyEvents.clear();

        m_Paused = false;

        m_LocalPose = GetBaseLayer().GetLocalPose();
        m_ModelPose.clear();

        if (m_Mesh)
            m_Mesh->GetSkeleton()->BuildModelPose(m_LocalPose, m_ModelPose);
    }

    float SkeletalMeshInstance::GetTime() const
    {
        return GetBaseLayer().GetTime();
    }

    float SkeletalMeshInstance::GetNormalizedTime() const
    {
        return GetBaseLayer().GetNormalizedTime();
    }

    bool SkeletalMeshInstance::IsPlaying() const
    {
        const auto someLayerPlaying = std::ranges::any_of(
            m_Layers,
            [](const auto& layer) { return layer->IsPlaying(); }
        );
        return !m_Paused && someLayerPlaying;
    }

    void SkeletalMeshInstance::Resume()
    {
        m_Paused = false;
    }

    bool SkeletalMeshInstance::ComposePose(
        const std::span<const AnimationLayer::SLayerEvaluation> evaluations,
        std::vector<SBoneTransform>& local,
        std::vector<glm::mat4>& model,
        const AnimationLayer* replacement
    ) const
    {
        if (!m_Mesh) return false;

        local = m_Mesh->GetData().ReferencePose;

        for (size_t i = 0; i < m_Layers.size(); ++i)
        {
            const auto& layer = replacement && replacement->GetName() == m_Layers[i]->GetName()
                ? *replacement
                : *m_Layers[i];

            if (layer.GetWeight() == 0.0f ||
                !layer.GetMask()->HasInfluence() ||
                !layer.HasPose())
                continue;

            const auto& pose = evaluations.empty()
                ? layer.GetLocalPose()
                : evaluations[i].LocalPose;

            const auto& weights = layer.GetMask()->GetWeights();

            for (size_t bone = 0; bone < local.size(); ++bone)
            {
                const auto weight = layer.GetWeight() * weights[bone];
                if (weight > 0.0f)
                    local[bone] = SBoneTransform::Blend(local[bone], pose[bone], weight);
            }
        }

        return m_Mesh->GetSkeleton()->BuildModelPose(local, model);
    }

    bool SkeletalMeshInstance::ApplyBaseLayer(const AnimationLayer& layer)
    {
        std::vector<SBoneTransform> local;
        std::vector<glm::mat4> model;

        if (!ComposePose({}, local, model, &layer))
            return false;

        auto& baseLayer = GetBaseLayer();
        baseLayer = layer;

        m_LocalPose = std::move(local);
        m_ModelPose = std::move(model);
        m_NotifyEvents.clear();

        return true;
    }

    bool SkeletalMeshInstance::Play(Ref<const AnimationSequence> sequence, const bool looping)
    {
        return Play(std::move(sequence), 0.0f, looping);
    }

    bool SkeletalMeshInstance::Play(
        Ref<const AnimationSequence> sequence,
        const float fadeDuration,
        const bool looping
    )
    {
        auto candidate = GetBaseLayer();
        if (!candidate.Play(std::move(sequence), fadeDuration, looping) ||
            !ApplyBaseLayer(candidate))
            return false;

        m_Paused = false;

        return true;
    }

    bool SkeletalMeshInstance::Play(
        Ref<const BlendSpace> blendSpace,
        const std::span<const float> values,
        const float fadeDuration,
        const bool looping
    )
    {
        auto candidate = GetBaseLayer();
        if (!candidate.Play(std::move(blendSpace), values, fadeDuration, looping) ||
            !ApplyBaseLayer(candidate))
            return false;

        m_Paused = false;

        return true;
    }

    bool SkeletalMeshInstance::SetBlendSpaceValue(const std::span<const float> values)
    {
        return GetBaseLayer().SetBlendSpaceValue(values);
    }

    bool SkeletalMeshInstance::Seek(const float time)
    {
        auto candidate = GetBaseLayer();
        return candidate.Seek(time) && ApplyBaseLayer(candidate);
    }

    bool SkeletalMeshInstance::Update(const float dt)
    {
        if (!std::isfinite(dt) || dt < 0.0f)
        {
            EE_CORE_ERROR("SkeletalMeshInstance requires a mesh and a valid delta time.")
            return false;
        }

        if (!m_Mesh)
        {
            m_NotifyEvents.clear();
            return true;
        }

        const auto advance = m_Paused ? 0.0f : dt;
        std::vector<AnimationLayer::SLayerEvaluation> evaluations(m_Layers.size());
        std::vector<SAnimationNotifyEvent> events;

        for (size_t i = 0; i < m_Layers.size(); ++i)
        {
            if (!m_Layers[i]->Evaluate(advance, evaluations[i]))
                return false;

            const auto& layerEvents = evaluations[i].NotifyEvents;
            const auto maxEvents = AnimationSequence::MaxNotifyEventsPerUpdate - events.size();
            if (layerEvents.size() > maxEvents)
            {
                EE_CORE_ERROR("Animation layers exceeded the combined notify event capacity.")
                return false;
            }

            events.insert(events.end(), layerEvents.begin(), layerEvents.end());
        }

        std::vector<SBoneTransform> local;
        std::vector<glm::mat4> model;

        if (!ComposePose(evaluations, local, model))
            return false;

        std::ranges::stable_sort(events, {}, &SAnimationNotifyEvent::TimeOffset);

        for (size_t i = 0; i < m_Layers.size(); ++i)
            m_Layers[i]->Apply(std::move(evaluations[i]));

        m_LocalPose = std::move(local);
        m_ModelPose = std::move(model);
        m_NotifyEvents = std::move(events);

        return true;
    }

    bool SkeletalMeshInstance::SetLocalPose(const std::span<const SBoneTransform> pose)
    {
        auto candidate = GetBaseLayer();
        std::vector<glm::mat4> model;

        if (!m_Mesh ||
            !candidate.SetLocalPose(pose) ||
            !m_Mesh->GetSkeleton()->BuildModelPose(pose, model))
            return false;

        GetBaseLayer() = candidate;
        ClearLayers();
        m_NotifyEvents.clear();
        m_Paused = false;

        m_LocalPose = GetBaseLayer().GetLocalPose();
        m_ModelPose = std::move(model);

        return true;
    }

    bool SkeletalMeshInstance::AddLayer(
        std::string name,
        Ref<const BoneMask> mask,
        const float weight
    )
    {
        if (!m_Mesh ||
            name.empty() ||
            GetLayer(name) ||
            !mask ||
            !std::isfinite(weight) ||
            weight < 0.0f ||
            weight > 1.0f ||
            (m_Mesh->GetSkeleton() != mask->GetSkeleton() &&
                !m_Mesh->GetSkeleton()->IsCompatible(*mask->GetSkeleton())))
        {
            EE_CORE_ERROR("Animation layers require a unique name, compatible mask, and valid weight.")
            return false;
        }

        Scope<AnimationLayer> layer(new AnimationLayer(
            std::move(name),
            std::move(mask),
            weight,
            m_Mesh->GetData().ReferencePose
        ));
        m_Layers.push_back(std::move(layer));

        return true;
    }

    AnimationLayer* SkeletalMeshInstance::GetLayer(const std::string_view name)
    {
        for (const auto& layer : m_Layers)
            if (layer->GetName() == name)
                return layer.get();
        return nullptr;
    }

    const AnimationLayer* SkeletalMeshInstance::GetLayer(const std::string_view name) const
    {
        for (const auto& layer : m_Layers)
            if (layer->GetName() == name)
                return layer.get();
        return nullptr;
    }

    bool SkeletalMeshInstance::SetLayerWeight(const std::string_view name, const float weight)
    {
        if (auto* layer = GetLayer(name))
            return layer->SetWeight(weight);

        EE_CORE_ERROR("Animation layer '{}' does not exist.", name)
        return false;
    }

    std::optional<float> SkeletalMeshInstance::GetLayerWeight(
        const std::string_view name
    ) const
    {
        if (const auto* layer = GetLayer(name))
            return layer->GetWeight();
        return std::nullopt;
    }

    bool SkeletalMeshInstance::RemoveLayer(const std::string_view name)
    {
        if (name == GetBaseLayer().GetName())
        {
            EE_CORE_ERROR("The base animation layer cannot be removed.")
            return false;
        }

        const auto removed = std::erase_if(
            m_Layers,
            [&](const auto& layer) { return layer->GetName() == name; }
        );

        if (!removed)
            EE_CORE_ERROR("Animation layer '{}' does not exist.", name)

        return removed != 0;
    }

    void SkeletalMeshInstance::ClearLayers()
    {
        m_Layers.erase(m_Layers.begin() + 1, m_Layers.end());
    }
}
