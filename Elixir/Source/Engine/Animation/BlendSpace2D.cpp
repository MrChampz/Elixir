#include "epch.h"
#include "BlendSpace2D.h"

#include <Engine/Animation/AnimationEvaluation.h>

namespace Elixir
{
    namespace
    {
        /**
         * @brief Get the signed area of the parallelogram formed by two vectors.
         * @param a First vector.
         * @param b Second vector.
         */
        double Cross(const glm::dvec2 a, const glm::dvec2 b)
        {
            return a.x * b.y - a.y * b.x;
        }

        /**
         * @brief Cancel a shared edge or append an exposed edge.
         * @param edges Oriented boundary edges.
         * @param a Start index.
         * @param b End index.
         */
        void AddEdge(std::vector<std::array<size_t, 2>>& edges, const size_t a, const size_t b)
        {
            const auto reverse = std::ranges::find(edges, std::array{ b, a });
            if (reverse == edges.end())
                edges.push_back({ a, b });
            else
                edges.erase(reverse);
        }
    }

    Ref<BlendSpace2D> BlendSpace2D::Create(
        std::string name,
        std::vector<SBlendSpace2DSample> samples
    )
    {
        if (name.empty() || samples.size() < 3)
        {
            EE_CORE_ERROR("BlendSpace2D requires a name and at least three samples.")
            return nullptr;
        }

        Ref<const Skeleton> skeleton;
        for (const auto& sample : samples)
        {
            if (!std::isfinite(sample.Position.x) || !std::isfinite(sample.Position.y) ||
                !sample.Sequence ||
                (skeleton && skeleton != sample.Sequence->GetSkeleton() &&
                    !skeleton->IsCompatible(*sample.Sequence->GetSkeleton())))
            {
                EE_CORE_ERROR("BlendSpace2D requires finite, compatible samples.")
                return nullptr;
            }

            skeleton = sample.Sequence->GetSkeleton();
        }

        std::ranges::sort(
            samples,
            [](const auto& a, const auto& b)
            {
                return a.Position.x == b.Position.x
                    ? a.Position.y < b.Position.y
                    : a.Position.x < b.Position.x;
            }
        );

        for (size_t i = 1; i < samples.size(); ++i)
        {
            if (samples[i - 1].Position == samples[i].Position)
            {
                EE_CORE_ERROR("BlendSpace2D sample coordinates must be unique.")
                return nullptr;
            }
        }

        auto result = Ref<BlendSpace2D>(new BlendSpace2D(
            std::move(name),
            std::move(samples)
        ));

        if (!result->Triangulate())
        {
            EE_CORE_ERROR("BlendSpace2D samples must form a nondegenerate triangulation.")
            return nullptr;
        }

        return result;
    }

    bool BlendSpace2D::Sample(
        const glm::vec2 value,
        const float phase,
        SLocalPose& output,
        const SLocalPose& referencePose
    ) const
    {
        return BlendSpace::Sample(
            std::array{ value.x, value.y },
            phase,
            output,
            referencePose
        );
    }

    SBlendSpace2DWeights BlendSpace2D::GetWeights(const glm::vec2 value) const
    {
        if (!std::isfinite(value.x) || !std::isfinite(value.y))
        {
            EE_CORE_ERROR("BlendSpace2D coordinates must be finite.")
            return {};
        }

        const auto point = (glm::dvec2(value) - m_Center) / m_Extent;

        for (const auto& triangle : m_Triangles)
        {
            const auto a = m_Points[triangle[0]];
            const auto b = m_Points[triangle[1]] - a;
            const auto c = m_Points[triangle[2]] - a;

            const auto area = Cross(b, c);
            const auto second = Cross(point - a, c) / area;
            const auto third = Cross(b, point - a) / area;

            auto weights = glm::dvec3(1.0 - second - third, second, third);

            if (glm::all(glm::greaterThanEqual(weights, glm::dvec3(-1e-12))))
            {
                weights = glm::max(weights, glm::dvec3(0));
                return {
                    triangle,
                    glm::vec3(weights / (weights.x + weights.y + weights.z))
                };
            }
        }

        SBlendSpace2DWeights result;

        auto distance = std::numeric_limits<double>::infinity();
        for (const auto& edge : m_Boundary)
        {
            const auto start = m_Points[edge[0]];
            const auto direction = m_Points[edge[1]] - start;

            const auto weight = std::clamp(
                glm::dot(point - start, direction) / glm::dot(direction, direction),
                0.0,
                1.0
            );

            const auto delta = point - (start + weight * direction);
            const auto squared = glm::dot(delta, delta);

            if (squared < distance)
            {
                distance = squared;
                result = {
                    { edge[0], edge[1], edge[1] },
                    { float(1 - weight), float(weight), 0 }
                };
            }
        }

        return result;
    }

    float BlendSpace2D::GetDuration(const glm::vec2 value) const
    {
        return BlendSpace::GetDuration(std::array{ value.x, value.y });
    }

    void BlendSpace2D::FindSamples(
        const std::span<const float> values,
        std::pmr::vector<SSequenceContribution>& output
    ) const
    {
        const auto blend = GetWeights({values[0], values[1]});
        output.clear();
        for (size_t i = 0; i < 3; ++i)
            output.push_back({m_Samples[blend.Samples[i]].Sequence.get(), blend.Weights[i]});
    }

    BlendSpace2D::BlendSpace2D(std::string name, std::vector<SBlendSpace2DSample> samples)
      : BlendSpace(
            std::move(name),
            samples.front().Sequence->GetSkeleton(),
            2,
            FindSynchronizationSequence(samples)
        ),
        m_Samples(std::move(samples)) {}

    bool BlendSpace2D::Triangulate()
    {
        glm::dvec2 min(m_Samples.front().Position), max(min);

        for (const auto& sample : m_Samples)
        {
            min = glm::min(min, glm::dvec2(sample.Position));
            max = glm::max(max, glm::dvec2(sample.Position));
        }

        m_Center = (min + max) * 0.5;
        m_Extent = std::max(max.x - min.x, max.y - min.y);

        if (m_Extent == 0.0) return false;

        for (const auto& sample : m_Samples)
            m_Points.push_back((glm::dvec2(sample.Position) - m_Center) / m_Extent);

        const auto count = m_Points.size();
        m_Points.insert(
            m_Points.end(),
            {{ -16, -16 }, { 16, -16 }, { 0, 16 }}
        );
        m_Triangles.push_back({ count, count + 1, count + 2 });

        for (size_t index = 0; index < count; ++index)
        {
            std::vector<std::array<size_t, 2>> edges;
            std::erase_if(
                m_Triangles,
                [&](const auto& triangle)
                {
                    const auto a = m_Points[triangle[0]] - m_Points[index];
                    const auto b = m_Points[triangle[1]] - m_Points[index];
                    const auto c = m_Points[triangle[2]] - m_Points[index];
                    const auto determinant = glm::dot(a, a) * Cross(b, c) -
                        glm::dot(b, b) * Cross(a, c) +
                        glm::dot(c, c) * Cross(a, b);

                    if (determinant <= 0.0) return false;

                    for (size_t i = 0; i < 3; ++i)
                        AddEdge(edges, triangle[i], triangle[(i + 1) % 3]);

                    return true;
                }
            );

            if (edges.empty()) return false;

            for (const auto& edge : edges)
            {
                const auto area = Cross(
                    m_Points[edge[1]] - m_Points[edge[0]],
                    m_Points[index] - m_Points[edge[0]]
                );

                if (area > 1e-14)
                    m_Triangles.push_back({ edge[0], edge[1], index });
            }
        }

        std::erase_if(
            m_Triangles,
            [count](const auto& triangle)
            {
                return triangle[0] >= count || triangle[1] >= count || triangle[2] >= count;
            }
        );

        m_Points.resize(count);

        std::vector used(count, false);
        for (auto& triangle : m_Triangles)
        {
            std::ranges::rotate(
                triangle.begin(),
                std::ranges::min_element(triangle),
                triangle.end()
            );

            for (size_t i = 0; i < 3; ++i)
            {
                used[triangle[i]] = true;
                AddEdge(m_Boundary, triangle[i], triangle[(i + 1) % 3]);
            }
        }

        std::ranges::sort(m_Triangles);
        std::ranges::sort(m_Boundary);

        const auto allTrianglesUsed = std::ranges::all_of(
            used,
            [](const bool value) { return value; }
        );

        return !m_Triangles.empty() && allTrianglesUsed;
    }
}
