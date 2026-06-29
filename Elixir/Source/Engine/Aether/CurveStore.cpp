#include "epch.h"
#include "CurveStore.h"

namespace Elixir::Aether
{
    void CurveStore::SetCurve(std::string name, std::vector<float> samples)
    {
        m_Curves[std::move(name)] = std::move(samples);
    }

    std::vector<SGPUCurve> CurveStore::Compile(const std::string& prefix) const
    {
        std::vector<SGPUCurve> curves;
        curves.reserve(m_Curves.size());

        for (const auto& [name, samples] : m_Curves)
        {
            curves.push_back({ prefix + name, samples });
        }

        return curves;
    }
}