#include "epch.h"

#include <Engine/Materials/Compilation/MaterialShaderDefines.h>

namespace Elixir::Materials::Compilation
{
    namespace
    {
        /** Describe where a feature definition applies and how its value is resolved. */
        struct SShaderDefineRule
        {
            std::string_view Name;
            EMaterialUsage Usage;
            EShaderStage Stages;
            std::string (*Evaluate)(const Material&);
        };

        /**
         * @brief Report whether the material supplies a vertex displacement output.
         * @param material Graph whose output channels are inspected.
         */
        std::string WorldPositionOffsetValue(const Material& material)
        {
            return material.GetGraph().HasChannel(EMaterialChannel::WorldPositionOffset)
                ? "1"
                : "0";
        }

        constexpr SShaderDefineRule s_Rules[]{
            {
                "GRAPH_HAS_WPO",
                EMaterialUsage::Surface,
                EShaderStage::Vertex,
                WorldPositionOffsetValue
            }
        };
    }

    std::vector<SMaterialShaderDefine> MaterialShaderDefines::Resolve(
        const Material& material,
        const EShaderStage stage
    )
    {
        std::vector<SMaterialShaderDefine> defines;

        for (const auto& rule : s_Rules)
        {
            if (rule.Usage != material.GetUsage() || !(rule.Stages & stage))
                continue;

            defines.push_back({ std::string(rule.Name), rule.Evaluate(material) });
        }

        return defines;
    }
}
