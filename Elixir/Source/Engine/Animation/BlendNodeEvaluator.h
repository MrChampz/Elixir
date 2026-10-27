#pragma once

#include <Engine/Animation/AnimationEvaluator.h>
#include <Engine/Animation/BlendNode.h>

namespace Elixir
{
    /** Independent pose evaluation and child state for a BlendNode resource. */
    class ELIXIR_API BlendNodeEvaluator final : public AnimationEvaluator
    {
    public:
        /**
         * @brief Borrow a resource retained by the enclosing playback.
         * @param source Resource that outlives this evaluator.
         */
        explicit BlendNodeEvaluator(const BlendNode& source);

        /** @brief Copy independent node state. */
        Scope<AnimationEvaluator> Clone() const override;

        /**
         * @brief Consume numeric inputs and shared character parameters.
         * @param values Resolved coordinates in input order.
         * @param parameters Character values available to child nodes.
         */
        bool Configure(
            std::span<const float> values,
            Ref<AnimationParameters> parameters
        ) override;

        /**
         * @brief Produce a sampled pose or a candidate child frame.
         * @param context Requested sampling position and optional frame advance.
         * @param output Receives the candidate pose and contributing sources.
         */
        bool Evaluate(
            const SAnimationEvaluationContext& context,
            SAnimationEvaluation& output
        ) override;

        /**
         * @brief Publish child state after complete frame validation.
         * @param phase Prepared normalized position.
         */
        void Apply(double phase) override;

        /**
         * @brief Reset children after playback relocates its clock.
         * @param phase New normalized position.
         */
        bool Reset(double phase) override;

        /** @brief Describe resolved timing and contributing sources. */
        SAnimationNodeState GetState() const override;

        BlendNodeEvaluator(const BlendNodeEvaluator& other) = delete;
        BlendNodeEvaluator& operator=(const BlendNodeEvaluator& other) = delete;

    private:
        /**
         * @brief Prepare child clocks and candidate poses.
         * @param context Frame timing, notification policy, and sampling defaults.
         */
        double Prepare(const SAnimationEvaluationContext& context);

        /**
         * @brief Sample committed or candidate child data.
         * @param phase Requested normalized position.
         * @param referencePose Defaults for unanimated channels.
         * @param output Receives the local pose.
         */
        bool Sample(double phase, const SLocalPose& referencePose, SLocalPose& output) const;

        /**
         * @brief Construct child controls once sampling defaults are available.
         * @param referencePose Mesh-specific defaults copied into the controls.
         */
        bool Initialize(const SLocalPose& referencePose) const;

        /**
         * @brief Compose the current or candidate poses of all connected controls.
         * @param output Receives the composed pose.
         * @param prepared Use candidate evaluation poses instead of published poses.
         */
        bool Compose(SLocalPose& output, bool prepared) const;

        Ref<AnimationParameters> m_Parameters;
        const BlendNode* m_Source;
        mutable std::vector<Scope<AnimationLayer>> m_Layers;
        std::vector<SAnimationPlayerEvaluation> m_Evaluations;
        SLocalPose m_Pose;
        bool m_Prepared = false;
        /** Default publication policy for the current candidate, not future evaluations. */
        bool m_PreparedNotifiesEnabled = true;
        mutable bool m_NotificationsConsumed = false;
    };
}
