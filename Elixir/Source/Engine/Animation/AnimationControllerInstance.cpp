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
        Reset();
    }

    void AnimationControllerInstance::Reset()
    {
        m_State.reset();
        m_Mesh.reset();
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

    bool AnimationControllerInstance::PlayState(const size_t state, const float fadeDuration)
    {
        const auto& definition = m_Controller->m_States[state];

        if (definition.Sequence)
            return m_Instance.Play(definition.Sequence, fadeDuration, definition.Looping);

        return m_Instance.Play(
            definition.BlendSpace,
            GetBlendValues(state),
            fadeDuration,
            definition.Looping
        );
    }

    std::span<const float> AnimationControllerInstance::GetBlendValues(const size_t state)
    {
        const auto& parameters = m_Controller->m_BlendParameters[state];
        m_BlendValues.resize(parameters.size());

        for (size_t axis = 0; axis < parameters.size(); ++axis)
            m_BlendValues[axis] = std::get<float>(m_Parameters[parameters[axis]]);

        return m_BlendValues;
    }

    bool AnimationControllerInstance::Update(const float dt)
    {
        const auto& mesh = m_Instance.GetMesh();
        const auto meshChanged = m_Mesh != mesh;

        if (!m_Controller || !mesh || !std::isfinite(dt) || dt < 0.0f ||
            (meshChanged && m_Controller->GetSkeleton() != mesh->GetSkeleton() &&
                !m_Controller->GetSkeleton()->IsCompatible(*mesh->GetSkeleton())))
        {
            EE_CORE_ERROR(
                "Animation controller requires a compatible mesh and a valid delta time."
            )
            return false;
        }

        if (meshChanged)
            m_State.reset();

        if (!m_State)
        {
            const auto initial = m_Controller->m_InitialState;
            if (!PlayState(initial, 0.0f))
                return false;

            m_State = initial;
            m_Mesh = mesh;
        }

        for (const auto& transition : m_Controller->m_Transitions[*m_State])
        {
            if (transition.WaitForCompletion &&
                (m_Instance.GetNormalizedTime() < 1.0f || m_Instance.IsTransitioning()))
                continue;

            const auto allConditionsValid = std::ranges::all_of(
                transition.Conditions,
                [&](const auto& condition)
                {
                    return Matches(condition);
                }
            );

            if (!allConditionsValid) continue;

            if (!PlayState(transition.Target, transition.FadeDuration))
                return false;

            m_State = transition.Target;
            break;
        }

        if (m_Controller->m_States[*m_State].BlendSpace &&
            !m_Instance.SetBlendSpaceValue(GetBlendValues(*m_State)))
            return false;

        return m_Instance.Update(dt);
    }

    std::string_view AnimationControllerInstance::GetCurrentState() const
    {
        return m_State
            ? std::string_view(m_Controller->m_States[*m_State].Name)
            : std::string_view{};
    }
}
