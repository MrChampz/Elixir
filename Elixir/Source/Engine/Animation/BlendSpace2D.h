#pragma once

#include <Engine/Animation/Pose.h>
#include <Engine/Animation/BlendSpace.h>

namespace Elixir
{
    /** Places a compatible animation on two numeric axes. */
    struct SBlendSpace2DSample
    {
        glm::vec2 Position = {};
        Ref<const AnimationSequence> Sequence;
    };

    /** Identifies up to three contributing samples and their normalized weights. */
    struct SBlendSpace2DWeights
    {
        std::array<size_t, 3> Samples = {};
        glm::vec3 Weights = {};
    };

    /** Blends synchronized animation cycles on an immutable triangulated plane. */
    class ELIXIR_API BlendSpace2D final : public BlendSpace
    {
    public:
        /**
         * @brief Triangulate unique, noncollinear samples with compatible skeletons.
         * @param name Nonempty asset name.
         * @param samples At least three samples, sorted by position during creation.
         * @return Null for invalid or numerically degenerate input.
         */
        static Ref<BlendSpace2D> Create(
            std::string name,
            std::vector<SBlendSpace2DSample> samples
        );

        /**
         * @brief Sample up to three cycles at a shared normalized phase.
         * @param value Finite coordinate, projected onto the sample hull when outside it.
         * @param phase Finite cycle phase, clamped to [0, 1].
         * @param output Receives the local pose; unchanged on failure.
         * @param referencePose Optional defaults for channels without animation keys.
         */
        bool Sample(
            glm::vec2 value,
            float phase,
            SLocalPose& output,
            const SLocalPose& referencePose = {}
        ) const;

        /**
         * @brief Get contributing samples; invalid input returns zero weights.
         * @param value Finite coordinate on the blend plane.
         */
        SBlendSpace2DWeights GetWeights(glm::vec2 value) const;

        /**
         * @brief Get the weighted cycle duration in seconds.
         * @param value Finite coordinate on the blend plane.
         */
        float GetDuration(glm::vec2 value) const;

        /** @brief Get samples sorted by X, then Y. */
        const std::vector<SBlendSpace2DSample>& GetSamples() const { return m_Samples; }

        using BlendSpace::GetDuration;
        using BlendSpace::Sample;

    protected:
        /**
        * @brief Resolve weighted clips for validated coordinates.
        * @param values One finite coordinate per axis.
        * @param output Receives clips and their nonnegative weights.
        */
        void FindSamples(
            std::span<const float> values,
            std::pmr::vector<SSequenceContribution>& output
        ) const override;

    private:
        /**
         * @brief Store validated samples before triangulation.
         * @param name Asset name.
         * @param samples Sorted compatible samples.
         */
        BlendSpace2D(std::string name, std::vector<SBlendSpace2DSample> samples);

        /** @brief Build Delaunay triangles and the convex boundary once at creation. */
        bool Triangulate();

        std::vector<SBlendSpace2DSample> m_Samples;
        std::vector<glm::dvec2> m_Points;
        std::vector<std::array<size_t, 3>> m_Triangles;
        std::vector<std::array<size_t, 2>> m_Boundary;
        glm::dvec2 m_Center = {};
        double m_Extent = 1.0;
    };
}
