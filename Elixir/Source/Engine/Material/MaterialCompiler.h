#pragma once

#include <Engine/Material/Material.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir
{
    /**
     * @brief Describes one material parameter in compiled GPU data.
     */
    struct SCompiledMaterialParameter
    {
        /** @brief Name used by the material graph. */
        std::string Name;

        /** @brief Parameter storage category. */
        EMaterialParameterKind Kind = EMaterialParameterKind::Value;

        /** @brief Value type used when Kind is Value. */
        EMaterialGraphValueType ValueType = EMaterialGraphValueType::Float4;

        /** @brief Slot in the compiled value or texture array. */
        uint32_t Slot = 0;
    };

    /**
     * @brief Stores the shader programs and parameter layout of a compiled material.
     */
    struct SCompiledMaterial
    {
        /** @brief Source material revision used during compilation. */
        uint32_t MaterialRevision = 0;

        /** @brief Bit mask of supported material usages. */
        uint32_t UsageMask = 0;

        /** @brief Shader used by surface material rendering. */
        Ref<Shader> SurfaceShader;

        /** @brief Shader used by particle sprite rendering. */
        Ref<Shader> ParticleSpriteShader;

        /** @brief Shader used by particle ribbon rendering. */
        Ref<Shader> ParticleRibbonShader;

        /** @brief Shader used by particle mesh rendering. */
        Ref<Shader> ParticleMeshShader;

        /** @brief Parameter layout shared by the material graph and GPU data. */
        std::vector<SCompiledMaterialParameter> Parameters;

        /**
         * @brief Checks whether the compiled material supports a usage.
         * @param usage Material usage to check.
         * @return True when the usage is present in UsageMask.
         */
        bool SupportsUsage(const EMaterialUsage usage) const
        {
            return (UsageMask & GetMaterialUsageMask(usage)) != 0;
        }

        /**
         * @brief Gets the particle shader for a material usage.
         *
         * Surface rendering uses SurfaceShader directly.
         *
         * @param usage Particle material usage.
         * @return The matching particle shader, or null for an unsupported usage.
         */
        const Ref<Shader>& GetShader(const EMaterialUsage usage) const
        {
            switch (usage)
            {
                case EMaterialUsage::ParticleSprite:
                    return ParticleSpriteShader;
                case EMaterialUsage::ParticleRibbon:
                    return ParticleRibbonShader;
                case EMaterialUsage::ParticleMesh:
                    return ParticleMeshShader;
                default:
                    static const Ref<Shader> unsupportedUsageShader;
                    return unsupportedUsageShader;
            }
        }
    };

    /**
     * @brief Reports the outcome of material compilation.
     */
    struct SMaterialCompileResult
    {
        /** @brief Compiled material data when compilation succeeds. */
        Ref<SCompiledMaterial> Material;

        /** @brief Human-readable diagnostic text when compilation fails. */
        std::string Diagnostics;

        /** @brief Checks whether compilation succeeded. */
        explicit operator bool() const { return Material != nullptr; }
    };

    /**
     * @brief Builds material parameter layouts and shader programs from material graphs.
     *
     * Compilation validates the graph, generates HLSL for supported usages, invokes
     * DXC, and loads the resulting shader programs.
     */
    class ELIXIR_API MaterialCompiler
    {
    public:
        /**
         * @brief Validates a material graph and builds its parameter layout.
         * @param material Material to validate and lower.
         * @return Compiled metadata, or diagnostics when validation fails.
         * @note This phase does not invoke the shader compiler.
         */
        static SMaterialCompileResult Build(const Material& material);

        /**
         * @brief Compiles a material graph into render-ready shader programs.
         * @param loader Shader loader used to load generated SPIR-V programs.
         * @param material Material to compile.
         * @return Compiled material data, or diagnostics when compilation fails.
         * @pre loader is valid.
         */
        static SMaterialCompileResult Compile(
            const ShaderLoader* loader,
            const Material& material
        );

    private:
        /** @brief Replaces the graph-body marker in a shader template. */
        static std::string InjectBody(const std::string& hlsl, const std::string& graphBody);

        /** @brief Compiles the surface shader program. */
        static SMaterialCompileResult CompileSurface(
            const ShaderLoader* loader,
            const Material& material,
            SMaterialCompileResult result
        );

        /** @brief Compiles the particle sprite shader program. */
        static SMaterialCompileResult CompileParticleSprite(
            const ShaderLoader* loader,
            const Material& material,
            SMaterialCompileResult result
        );

        /** @brief Compiles the particle ribbon shader program. */
        static SMaterialCompileResult CompileParticleRibbon(
            const ShaderLoader* loader,
            const Material& material,
            SMaterialCompileResult result
        );

        /** @brief Compiles the particle mesh shader program. */
        static SMaterialCompileResult CompileParticleMesh(
            const ShaderLoader* loader,
            const Material& material,
            SMaterialCompileResult result
        );
    };
}
