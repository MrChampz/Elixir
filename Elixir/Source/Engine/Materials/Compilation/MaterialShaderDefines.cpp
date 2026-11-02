#include "epch.h"
#include "MaterialShaderDefines.h"

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
         * @brief Resolve the input space shared by material normal channels.
         * @param material Material whose normal space setting is inspected.
         */
        std::string TangentSpaceNormalValue(const Material& material)
        {
            return material.UsesTangentSpaceNormal() ? "1" : "0";
        }

        constexpr SShaderDefineRule s_Rules[]{
            {
                "MATERIAL_TANGENT_SPACE_NORMAL",
                EMaterialUsage::Surface,
                EShaderStage::Pixel,
                TangentSpaceNormalValue
            },
            {
                "MATERIAL_TANGENT_SPACE_NORMAL",
                EMaterialUsage::Particle,
                EShaderStage::Pixel,
                TangentSpaceNormalValue
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
