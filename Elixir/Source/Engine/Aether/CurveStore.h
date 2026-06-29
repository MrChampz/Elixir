#pragma once

#include <Engine/Aether/ParameterStore.h>

namespace Elixir::Aether
{
    struct SGPUCurve
    {
        std::string Name;
        std::vector<float> Samples;
    };

    class ELIXIR_API CurveStore
    {
    public:
        void SetCurve(std::string name, std::vector<float> samples);

        std::vector<SGPUCurve> Compile(const std::string& prefix = "") const;

    private:
        std::map<std::string, std::vector<float>> m_Curves;
    };

    inline uint32_t FindCurveParameterIndex(
        const std::vector<SGPUParameter>& parameters,
        const std::string& emitterName,
        const std::string& curveName
    )
    {
        if (curveName.empty())
            return UINT32_MAX;

        const uint32_t localIndex = FindParameterIndex(parameters, emitterName + "." + curveName + ":0");
        if (localIndex != UINT32_MAX)
            return localIndex;

        return FindParameterIndex(parameters, curveName + ":0");
    }

    inline glm::vec4 CurveChunk(const std::vector<float>& samples, const std::size_t chunk)
    {
        glm::vec4 value{};
        const std::size_t offset = chunk * 4;

        if (offset + 0 < samples.size()) value.x = samples[offset + 0];
        if (offset + 1 < samples.size()) value.y = samples[offset + 1];
        if (offset + 2 < samples.size()) value.z = samples[offset + 2];
        if (offset + 3 < samples.size()) value.w = samples[offset + 3];

        return value;
    }
}