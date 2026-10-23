#include "epch.h"
#include "AnimationControllerInstance.h"

#include <Engine/Mesh/SkeletalMeshInstance.h>

namespace Elixir
{
    AnimationControllerInstance::AnimationControllerInstance(
        Ref<const AnimationController> controller,
        SkeletalMeshInstance& instance
    ) : m_Controller(std::move(controller)),
        m_Instance(instance)
    {
        const auto& mesh = m_Instance.GetMesh();
        const auto skeleton = mesh ? mesh->GetSkeleton() : nullptr;

        m_Compatible = m_Controller && skeleton &&
            (m_Controller->GetSkeleton() == skeleton ||
                m_Controller->GetSkeleton()->IsCompatible(*skeleton));

        if (!m_Compatible)
            EE_CORE_ERROR("Animation controller requires a compatible mesh and controller.")

        Reset();
    }

    void AnimationControllerInstance::Reset()
    {
        const auto layerCount = m_Controller ? m_Controller->m_Layers.size() : 0;
        m_Playback.assign(layerCount, {});

        m_Parameters.clear();
        m_BlendValues.clear();

        if (!m_Controller)
            return;

        m_Parameters.reserve(m_Controller->m_Parameters.size());

        for (const auto& parameter : m_Controller->m_Parameters)
            m_Parameters.push_back(parameter.DefaultValue);
    }

    std::optional<size_t> AnimationControllerInstance::FindParameter(
        const std::string_view name
    ) const
    {
        if (!m_Controller) return std::nullopt;

        const auto& definitions = m_Controller->m_Parameters;
        for (size_t i = 0; i < definitions.size(); ++i)
        {
            if (definitions[i].Name == name)
                return i;
        }

        return std::nullopt;
    }

    bool AnimationControllerInstance::SetFloat(const std::string_view name, const float value)
    {
        const auto index = FindParameter(name);

        if (!index ||
            !std::holds_alternative<float>(m_Parameters[*index]) ||
            !std::isfinite(value))
        {
            EE_CORE_ERROR("Animation controller numeric parameter '{}' is invalid.", name)
            return false;
        }

        m_Parameters[*index] = value;
        return true;
    }

    bool AnimationControllerInstance::SetBool(const std::string_view name, const bool value)
    {
        const auto index = FindParameter(name);

        if (!index || !std::holds_alternative<bool>(m_Parameters[*index]))
        {
            EE_CORE_ERROR("Animation controller boolean parameter '{}' is invalid.", name)
            return false;
        }

        m_Parameters[*index] = value;
        return true;
    }

    std::optional<float> AnimationControllerInstance::GetFloat(
        const std::string_view name
    ) const
    {
        const auto index = FindParameter(name);
        if (!index) return std::nullopt;

        const auto* value = std::get_if<float>(&m_Parameters[*index]);
        return value ? std::optional(*value) : std::nullopt;
    }

    std::optional<bool> AnimationControllerInstance::GetBool(
        const std::string_view name
    ) const
    {
        const auto index = FindParameter(name);
        if (!index) return std::nullopt;

        const auto* value = std::get_if<bool>(&m_Parameters[*index]);
        return value ? std::optional(*value) : std::nullopt;
    }

    bool AnimationControllerInstance::Matches(
        const AnimationController::SCondition& condition
    ) const
    {
        const auto& parameter = m_Parameters[condition.Parameter];

        if (const auto* value = std::get_if<bool>(&parameter))
        {
            const auto equal = *value == std::get<bool>(condition.Value);
            return condition.Comparison == EAnimationComparison::Equal ? equal : !equal;
        }

        const auto value = std::get<float>(parameter);
        const auto target = std::get<float>(condition.Value);
        switch (condition.Comparison)
        {
            case EAnimationComparison::Equal:
                return value == target;
            case EAnimationComparison::NotEqual:
                return value != target;
            case EAnimationComparison::Less:
                return value < target;
            case EAnimationComparison::LessEqual:
                return value <= target;
            case EAnimationComparison::Greater:
                return value > target;
            case EAnimationComparison::GreaterEqual:
                return value >= target;
        }

        return false;
    }

    bool AnimationControllerInstance::PlaySource(
        const size_t layerIndex,
        const AnimationController::SSequenceSource& source,
        const float fadeDuration,
        const EAnimationTransitionMode mode
    )
    {
        auto& layer = *m_Instance.GetLayer(m_Controller->m_Layers[layerIndex].Name);
        return layer.Play(source.Sequence, fadeDuration, source.Looping, mode);
    }

    bool AnimationControllerInstance::PlaySource(
        const size_t layerIndex,
        const AnimationController::SBlendSpaceSource& source,
        const float fadeDuration,
        const EAnimationTransitionMode mode
    )
    {
        auto& layer = *m_Instance.GetLayer(m_Controller->m_Layers[layerIndex].Name);
        return layer.Play(
            source.BlendSpace,
            GetBlendValues(source),
            fadeDuration,
            source.Looping,
            mode
        );
    }

    bool AnimationControllerInstance::PlaySource(
        const size_t layerIndex,
        const AnimationController::TPlaybackSource& source,
        const float fadeDuration,
        const EAnimationTransitionMode mode
    )
    {
        return std::visit(
            [&](const auto& playback)
            {
                return PlaySource(layerIndex, playback, fadeDuration, mode);
            },
            source
        );
    }

    std::span<const float> AnimationControllerInstance::GetBlendValues(
        const AnimationController::SBlendSpaceSource& source
    )
    {
        const auto& parameters = source.BlendParameters;
        m_BlendValues.resize(parameters.size());

        for (size_t axis = 0; axis < parameters.size(); ++axis)
            m_BlendValues[axis] = std::get<float>(m_Parameters[parameters[axis]]);

        return m_BlendValues;
    }

    bool AnimationControllerInstance::InitializeLayers()
    {
        if (m_Instance.GetLayerCount() != 1)
        {
            EE_CORE_ERROR("Animation controller requires a fresh mesh with only Base.")
            return false;
        }

        for (size_t i = 1; i < m_Controller->m_Layers.size(); ++i)
        {
            const auto& definition = m_Controller->m_Layers[i];
            if (!m_Instance.AddLayer(definition.Name, definition.Mask, definition.Weight))
            {
                m_Instance.ClearLayers();
                return false;
            }

            auto& layer = *m_Instance.GetLayer(definition.Name);
            if (!layer.SetBlendMode(definition.BlendMode) ||
                !layer.SetAdditiveReferencePose(definition.AdditiveReferencePose))
            {
                m_Instance.ClearLayers();
                return false;
            }
        }

        m_Initialized = true;

        return true;
    }

    bool AnimationControllerInstance::Update(const float dt)
    {
        if (!m_Compatible || !std::isfinite(dt) || dt < 0)
        {
            EE_CORE_ERROR("Animation controller requires compatible assets and valid time.")
            return false;
        }

        if (!m_Initialized && !InitializeLayers())
            return false;

        for (const auto& definition : m_Controller->m_Layers)
        {
            if (!m_Instance.GetLayer(definition.Name))
            {
                EE_CORE_ERROR("Animation controller layer '{}' was removed.", definition.Name)
                return false;
            }
        }

        for (size_t i = 0; i < m_Playback.size(); ++i)
        {
            if (!UpdateLayerSource(i))
                return false;
        }

        return m_Instance.Update(dt);
    }

    bool AnimationControllerInstance::UpdateLayerSource(const size_t layerIndex)
    {
        return std::visit(
            [&](const auto& source) { return UpdateSource(layerIndex, source); },
            m_Controller->m_LayerSources[layerIndex]
        );
    }

    bool AnimationControllerInstance::UpdateSource(
        const size_t layerIndex,
        const AnimationController::SSequenceSource& source
    )
    {
        auto& progress = m_Playback[layerIndex];
        if (progress.Started)
            return true;

        if (!PlaySource(layerIndex, source, 0.0f))
            return false;

        progress.Started = true;
        return true;
    }

    bool AnimationControllerInstance::UpdateSource(
        const size_t layerIndex,
        const AnimationController::SBlendSpaceSource& source
    )
    {
        auto& progress = m_Playback[layerIndex];
        if (!progress.Started)
        {
            if (!PlaySource(layerIndex, source, 0.0f))
                return false;

            progress.Started = true;
        }

        auto& layer = *m_Instance.GetLayer(m_Controller->m_Layers[layerIndex].Name);
        return layer.SetBlendSpaceValue(GetBlendValues(source));
    }

    bool AnimationControllerInstance::UpdateSource(
        const size_t layerIndex,
        const AnimationController::SStateMachine& machine
    )
    {
        auto& layer = *m_Instance.GetLayer(m_Controller->m_Layers[layerIndex].Name);
        auto& state = m_Playback[layerIndex].State;

        if (!state)
        {
            const auto initial = machine.InitialState;
            if (!PlaySource(layerIndex, machine.Sources[initial], 0.0f))
                return false;

            state = initial;
        }

        for (const auto& transition : machine.Transitions[*state])
        {
            if (transition.WaitForCompletion &&
                (layer.GetNormalizedTime() < 1.0f || layer.IsTransitioning()))
                continue;

            const auto allConditionsValid = std::ranges::all_of(
                transition.Conditions,
                [&](const auto& condition)
                {
                    return Matches(condition);
                }
            );

            if (!allConditionsValid) continue;

            const auto success = PlaySource(
                layerIndex,
                machine.Sources[transition.Target],
                transition.FadeDuration,
                transition.Mode
            );

            if (!success) return false;

            state = transition.Target;
            break;
        }

        const auto* blendSpace =
            std::get_if<AnimationController::SBlendSpaceSource>(&machine.Sources[*state]);

        if (blendSpace && !layer.SetBlendSpaceValue(GetBlendValues(*blendSpace)))
            return false;

        return true;
    }

    std::string_view AnimationControllerInstance::GetCurrentState(
        const std::string_view layerName
    ) const
    {
        if (!m_Controller)
            return {};

        for (size_t i = 0; i < m_Controller->m_Layers.size(); ++i)
        {
            const auto& layer = m_Controller->m_Layers[i];
            if (layer.Name == layerName)
            {
                return m_Playback[i].State
                    ? std::string_view(layer.StateMachine.States[*m_Playback[i].State].Name)
                    : std::string_view{};
            }
        }

        return {};
    }
}
