#pragma once

#include <map>

namespace Elixir::Aether
{
    struct SGPUColorCurve
    {
        std::string Name;
        std::vector<glm::vec4> Samples;
    };

    class ELIXIR_API ColorCurveStore
    {
    public:
        void SetCurve(std::string name, std::vector<glm::vec4> samples);

        std::vector<SGPUColorCurve> Compile(const std::string& prefix = "") const;

    private:
        std::map<std::string, std::vector<glm::vec4>> m_Curves;
    };

    inline std::vector<glm::vec4> BakeColorCurve(const std::vector<glm::vec4>& samples)
    {
        std::vector<glm::vec4> baked = samples;

        if (baked.empty())
            baked.push_back(glm::vec4{ 1.0f });

        if (baked.size() < 8)
            baked.resize(8, baked.back());

        return baked;
    }
}
