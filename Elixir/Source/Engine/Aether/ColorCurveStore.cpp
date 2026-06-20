#include "epch.h"
#include "ColorCurveStore.h"

namespace Elixir::Aether
{
    void ColorCurveStore::SetCurve(std::string name, std::vector<glm::vec4> samples)
    {
        m_Curves[std::move(name)] = std::move(samples);
    }

    std::vector<SGPUColorCurve> ColorCurveStore::Compile(const std::string& prefix) const
    {
        std::vector<SGPUColorCurve> curves;
        curves.reserve(m_Curves.size());

        for (const auto& [name, samples] : m_Curves)
        {
            curves.push_back({ prefix + name, samples });
        }

        return curves;
    }
}
