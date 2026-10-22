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
        m_States.assign(layerCount, std::nullopt);

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

    bool AnimationControllerInstance::PlayState(
        const size_t machine,
        const size_t state,
        const float fadeDuration
    )
    {
        const auto& layerDefinition = m_Controller->m_Layers[machine];
        const auto& definition = layerDefinition.StateMachine.States[state];
        auto& layer = *m_Instance.GetLayer(layerDefinition.Name);

        if (definition.Sequence)
            return layer.Play(definition.Sequence, fadeDuration, definition.Looping);

        return layer.Play(
            definition.BlendSpace,
            GetBlendValues(machine, state),
            fadeDuration,
            definition.Looping
        );
    }

    std::span<const float> AnimationControllerInstance::GetBlendValues(
        const size_t machine,
        const size_t state
    )
    {
        const auto& parameters = m_Controller->m_StateMachines[machine].BlendParameters[state];
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

        for (size_t i = 0; i < m_States.size(); ++i)
        {
            if (!UpdateStateMachine(i))
                return false;
        }

        return m_Instance.Update(dt);
    }

    bool AnimationControllerInstance::UpdateStateMachine(const size_t machine)
    {
        const auto& definition = m_Controller->m_Layers[machine];
        const auto& compiled = m_Controller->m_StateMachines[machine];
        auto& layer = *m_Instance.GetLayer(definition.Name);
        auto& state = m_States[machine];
        if (!state)
        {
            const auto initial = compiled.InitialState;
            if (!PlayState(machine, initial, 0.0f))
                return false;

            state = initial;
        }

        for (const auto& transition : compiled.Transitions[*state])
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

            if (!PlayState(machine, transition.Target, transition.FadeDuration))
                return false;

            state = transition.Target;
            break;
        }

        if (definition.StateMachine.States[*state].BlendSpace &&
            !layer.SetBlendSpaceValue(GetBlendValues(machine, *state)))
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
                return m_States[i]
                    ? std::string_view(layer.StateMachine.States[*m_States[i]].Name)
                    : std::string_view{};
        }

        return {};
    }
}
