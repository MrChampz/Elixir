#pragma once

#include <Engine/Materials/Material.h>

namespace Elixir::Materials::Compilation
{
    /** Stores one preprocessor definition required by a material shader. */
    struct SMaterialShaderDefine
    {
        /** Preprocessor identifier used by the shader template. */
        std::string Name;

        /** Preprocessor expression written after the identifier. */
        std::string Value;
    };

    /** Resolve material feature rules into definitions for one shader stage. */
    class ELIXIR_API MaterialShaderDefines final
    {
    public:
        /**
         * @brief Evaluate the feature rules that apply to a material and shader stage.
         * @param material Graph and renderer usage used to evaluate feature rules.
         * @param stage Concrete shader stage receiving the definitions.
         * @return Definitions in rule order; disabled features retain their zero value.
         */
        static std::vector<SMaterialShaderDefine> Resolve(
            const Material& material,
            EShaderStage stage
        );
    };
}
