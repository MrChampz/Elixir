#include "epch.h"
#include <Engine/Animation/Pose.h>
#include "BlendNodeEvaluator.h"

namespace Elixir
{
    BlendNodeEvaluator::BlendNodeEvaluator(const BlendNode& source)
        : m_Source(&source) {}

    Scope<AnimationEvaluator> BlendNodeEvaluator::Clone() const
    {
        auto copy = CreateScope<BlendNodeEvaluator>(*m_Source);
        copy->m_Parameters = m_Parameters;

        for (const auto& layer : m_Layers)
            copy->m_Layers.push_back(Scope<AnimationLayer>(new AnimationLayer(*layer)));

        return copy;
    }

    bool BlendNodeEvaluator::Configure(
        const std::span<const float> values,
        Ref<AnimationParameters> parameters
    )
    {
        if (!values.empty()) return false;

        if (parameters == m_Parameters) return true;

        if (!parameters || !m_Source->CanBind(*parameters, {}))
            return false;

        m_Parameters = std::move(parameters);
        m_Layers.clear();
        m_Evaluations.clear();
        m_Prepared = false;

        return true;
    }

    bool BlendNodeEvaluator::Evaluate(
        const SAnimationEvaluationContext& context,
        SAnimationEvaluation& output
    )
    {
        output.Phase = context.Advance
            ? Prepare(context)
            : context.Phase;

        if (!std::isfinite(output.Phase))
            return false;

        if (!Sample(output.Phase, context.ReferencePose, output.Sample))
            return false;

        output.Sources = GetState().Sources;
        return true;
    }

    void BlendNodeEvaluator::Apply(const double)
    {
        if (!m_Prepared) return;

        const auto discardNotifies = !m_PreparedNotifiesEnabled && !m_NotificationsConsumed;

        for (size_t i = 0; i < m_Layers.size(); ++i)
        {
            if (discardNotifies)
            {
                m_Evaluations[i].Notifies.Active.clear();
                m_Evaluations[i].Notifies.Events.clear();
            }

            m_Layers[i]->GetPlayer().Apply(std::move(m_Evaluations[i]));
        }

        m_Prepared = false;
    }

    bool BlendNodeEvaluator::Reset(const double phase)
    {
        for (const auto& layer : m_Layers)
        {
            auto& player = layer->GetPlayer();
            if (!player.Seek(float(phase * player.GetPlayback().GetDuration())))
                return false;
        }

        m_Prepared = false;
        return true;
    }

    SAnimationNodeState BlendNodeEvaluator::GetState() const
    {
        const auto allFinished = std::ranges::all_of(
            m_Layers,
            [](const auto& layer)
            {
                const auto& player = layer->GetPlayer();
                const auto& playback = player.GetPlayback();
                return !player.GetMontage() && !player.IsTransitioning() &&
                    (!playback.IsValid() || playback.IsFinished());
            }
        );
        const auto finished = !m_Layers.empty() && allFinished;

        return {
            .Duration = m_Layers.empty()
                ? 0
                : m_Layers.front()->GetPlayer().GetPlayback().GetDuration(),
            .Continuous = true,
            .Finished = finished,
            .Sources = SAnimationLayerSources{
                m_Layers,
                m_Evaluations,
                m_Prepared,
                &m_NotificationsConsumed
            },
            .PrimaryPlayback = m_Layers.empty()
                ? nullptr
                : &m_Layers.front()->GetPlayer().GetPlayback()
        };
    }

    double BlendNodeEvaluator::Prepare(const SAnimationEvaluationContext& context)
    {
        m_PreparedNotifiesEnabled = context.NotifiesEnabled;
        m_Prepared = false;
        m_NotificationsConsumed = false;

        if (!Initialize(context.ReferencePose))
            return std::numeric_limits<double>::quiet_NaN();

        m_Evaluations.clear();
        m_Evaluations.resize(m_Layers.size());

        for (size_t i = 0; i < m_Layers.size(); ++i)
        {
            auto& player = m_Layers[i]->GetPlayer();
            const auto evaluated = player.Evaluate(
                context.DeltaSeconds,
                m_Evaluations[i],
                false,
                context.NotifiesEnabled
            );

            if (!evaluated)
                return std::numeric_limits<double>::quiet_NaN();
        }

        if (!Compose(m_Sample, true))
            return std::numeric_limits<double>::quiet_NaN();

        m_Prepared = true;

        return m_Evaluations.front().PlaybackPhase;
    }

    bool BlendNodeEvaluator::Sample(
        const double phase,
        const SLocalPose& referencePose,
        SAnimationSample& output
    ) const
    {
        if (!std::isfinite(phase) || !Initialize(referencePose))
            return false;

        if (m_Prepared)
        {
            output = m_Sample;
            return true;
        }

        return Compose(output, false);
    }

    bool BlendNodeEvaluator::Initialize(const SLocalPose& referencePose) const
    {
        if (!m_Layers.empty()) return true;

        if (!m_Parameters) return false;

        std::vector<Scope<AnimationLayer>> layers;
        const auto& inputs = m_Source->GetInputs();

        for (size_t i = 0; i < inputs.size(); ++i)
        {
            const auto& input = inputs[i];
            auto mask = input.Mask;

            if (i == 0)
                mask = BoneMask::Create(
                    input.Name,
                    m_Source->GetSkeleton(),
                    std::vector(referencePose.LocalTransforms.size(), 1.0f)
                );

            if (!mask) return false;

            Scope<AnimationLayer> layer(new AnimationLayer(
                input.Name,
                mask,
                input.Weight,
                referencePose
            ));

            layer->m_Base = i == 0;

            if (!layer->SetBlendMode(input.BlendMode) ||
                !layer->SetAdditiveReferencePose(input.AdditiveReferencePose) ||
                !layer->GetPlayer().BindParameters(m_Parameters, input.InputParameters) ||
                !layer->GetPlayer().Play(input.Node, 0.0f, input.Looping))
                return false;

            layers.push_back(std::move(layer));
        }

        m_Layers = std::move(layers);
        return true;
    }

    bool BlendNodeEvaluator::Compose(
        SAnimationSample& output,
        const bool prepared
    ) const
    {
        if (m_Layers.empty()) return false;

        output = prepared
            ? m_Evaluations.front().Sample
            : m_Layers.front()->GetPlayer().GetSample();

        for (size_t i = 1; i < m_Layers.size(); ++i)
        {
            const auto& sample = prepared
                ? m_Evaluations[i].Sample
                : m_Layers[i]->GetPlayer().GetSample();

            if (!m_Layers[i]->BlendSample(output, sample))
                return false;
        }

        return output.Pose.IsValid(m_Source->GetSkeleton()->GetBones().size());
    }
}
