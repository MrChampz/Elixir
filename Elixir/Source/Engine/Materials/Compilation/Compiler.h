#pragma once

#include <Engine/Graphics/Shader/ShaderLoader.h>
#include <Engine/Materials/Material.h>

namespace Elixir::Materials::Compilation
{
    /** @brief Identifies a shader variant produced for a compiled material. */
    enum class EMaterialShaderVariant : uint8_t
    {
        Surface,
        ParticleSprite,
        ParticleRibbon,
        ParticleMesh,
    };

    /**
     * @brief Describes one material parameter in compiled GPU data.
     */
    struct SCompiledParameter
    {
        /** @brief Name used by the material graph. */
        std::string Name;

        /** @brief Parameter storage category. */
        EMaterialParameterKind Kind = EMaterialParameterKind::Value;

        /** @brief Value type used when Kind is Value. */
        EMaterialValueType ValueType = EMaterialValueType::Float4;

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

        /** @brief Renderer usage compiled for this material. */
        EMaterialUsage Usage = EMaterialUsage::Surface;

        /** @brief Shader used by surface material rendering. */
        Ref<Shader> SurfaceShader;

        /** @brief Shader variant used by particle sprite rendering. */
        Ref<Shader> ParticleSpriteShader;

        /** @brief Shader variant used by particle ribbon rendering. */
        Ref<Shader> ParticleRibbonShader;

        /** @brief Shader variant used by particle mesh rendering. */
        Ref<Shader> ParticleMeshShader;

        /** @brief Parameter layout shared by the material graph and GPU data. */
        std::vector<SCompiledParameter> Parameters;

        /** @brief Returns the renderer usage compiled for this material. */
        EMaterialUsage GetUsage() const { return Usage; }

        /**
         * @brief Gets the shader for a compiled material variant.
         * @param variant Shader variant.
         * @return The matching shader, or null when the variant is unavailable.
         */
        const Ref<Shader>& GetShader(const EMaterialShaderVariant variant) const
        {
            switch (variant)
            {
                case EMaterialShaderVariant::Surface:
                    return SurfaceShader;
                case EMaterialShaderVariant::ParticleSprite:
                    return ParticleSpriteShader;
                case EMaterialShaderVariant::ParticleRibbon:
                    return ParticleRibbonShader;
                case EMaterialShaderVariant::ParticleMesh:
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
    struct SCompileResult
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
    class ELIXIR_API Compiler
    {
    public:
        /**
         * @brief Validates a material graph and builds its parameter layout.
         * @param material Material to validate and lower.
         * @return Compiled metadata, or diagnostics when validation fails.
         * @note This phase does not invoke the shader compiler.
         */
        static SCompileResult Build(const Material& material);

        /**
         * @brief Compiles a material graph into render-ready shader programs.
         * @param loader Shader loader used to load generated SPIR-V programs.
         * @param material Material to compile.
         * @return Compiled material data, or diagnostics when compilation fails.
         * @pre loader is valid.
         */
        static SCompileResult Compile(const ShaderLoader* loader, const Material& material);

    private:
        /** @brief Replaces shader-template markers with generated material code. */
        static std::string InjectBody(
            const std::string& hlsl,
            const std::string& graphBody,
            EMaterialShadingModel shadingModel = EMaterialShadingModel::Lit
        );

        /** @brief Compiles the surface shader program. */
        static SCompileResult CompileSurface(
            const ShaderLoader* loader,
            const Material& material,
            SCompileResult result
        );

        /** @brief Compiles the particle sprite shader program. */
        static SCompileResult CompileParticleSprite(
            const ShaderLoader* loader,
            const Material& material,
            SCompileResult result
        );

        /** @brief Compiles the particle ribbon shader program. */
        static SCompileResult CompileParticleRibbon(
            const ShaderLoader* loader,
            const Material& material,
            SCompileResult result
        );

        /** @brief Compiles the particle mesh shader program. */
        static SCompileResult CompileParticleMesh(
            const ShaderLoader* loader,
            const Material& material,
            SCompileResult result
        );
    };
}
