#pragma once

#include <Engine/Materials/Material.h>
#include <Engine/Materials/Compilation/MaterialEmissionEnvironment.h>

namespace Elixir::Materials::Compilation
{
    struct SCompiledMaterial;

    /** Stores a generated shader source or its emission diagnostic. */
    struct SShaderSourceResult
    {
        /** Complete shader source; empty when generation fails. */
        std::string Source;

        /** First generation error; empty on success. */
        std::string Diagnostics;

        /** @brief Report whether generation produced usable shader source. */
        explicit operator bool() const
        {
            return !Source.empty() && Diagnostics.empty();
        }
    };

    /** @brief Configure template inputs and assemble material shader sources. */
    class ELIXIR_API MaterialShaderSourceBuilder final
    {
      public:
        /**
         * @brief Describe inputs and resources supplied by the selected shader template.
         * @param usage Material renderer contract.
         * @param stage One concrete shader stage.
         * @return Empty input expressions where the template does not expose an input.
         */
        static SMaterialEmissionEnvironment CreateEnvironment(
            EMaterialUsage usage,
            EShaderStage stage
        );

        /**
         * @brief Generate graph code and fill one shader template.
         * @param shaderTemplate HLSL containing the supported injection markers.
         * @param material Source graph and shading settings.
         * @param compiled Parameter layout and static choices for this variant.
         * @param stage Concrete stage to generate.
         * @return Generated source on success, or a diagnostic on failure.
         */
        static SShaderSourceResult Build(
            const std::string& shaderTemplate,
            const Material& material,
            const SCompiledMaterial& compiled,
            EShaderStage stage
        );
    };
}
