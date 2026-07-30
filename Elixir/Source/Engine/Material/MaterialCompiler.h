#pragma once

#include <Engine/Material/Material.h>
#include <Engine/Graphics/Shader/ShaderLoader.h>

namespace Elixir
{
    struct SCompiledMaterialParameter
    {
        std::string Name;
        EMaterialParameterKind Kind = EMaterialParameterKind::Value;
        EMaterialGraphValueType ValueType = EMaterialGraphValueType::Float4;
        uint32_t Slot = 0;
    };

    struct SCompiledMaterial
    {
        uint32_t MaterialRevision = 0;
        uint32_t UsageMask = 0;
        Ref<Shader> SurfaceShader;
        Ref<Shader> ParticleSpriteShader;
        std::vector<SCompiledMaterialParameter> Parameters;

        bool SupportsUsage(const EMaterialUsage usage) const
        {
            return (UsageMask & GetMaterialUsageMask(usage)) != 0;
        }

        // A Surface shader is not a fallback particle permutation. Until a
        // renderer-specific permutation exists, Ribbon and Mesh resolve to
        // no shader and their renderer can keep its existing fallback path.
        const Ref<Shader>& GetShader(const EMaterialUsage usage) const
        {
            switch (usage)
            {
                case EMaterialUsage::ParticleSprite:
                    return ParticleSpriteShader;
                case EMaterialUsage::ParticleRibbon:
                case EMaterialUsage::ParticleMesh:
                    break;
            }

            static const Ref<Shader> unsupportedUsageShader;
            return unsupportedUsageShader;
        }
    };

    struct SMaterialCompileResult
    {
        Ref<SCompiledMaterial> Material;
        std::string Diagnostics;

        explicit operator bool() const { return Material != nullptr; }
    };

    // Turns a MaterialGraph into a usable shader: injects the graph's generated
    // body into the Material template, compiles it to SPIR-V with DXC at
    // runtime, and loads it (with the shared model vertex shader) into a Shader.
    class ELIXIR_API MaterialCompiler
    {
    public:
        // Pure phase: validates schema, assigns stable slots and lowers HLSL.
        static SMaterialCompileResult Build(const Material& material);

        // Toolchain phase: compiles the prepared material to a ready-to-bind shader.
        static SMaterialCompileResult Compile(const ShaderLoader* loader, const Material& material);

    private:
        static std::string InjectBody(const std::string& hlsl, const std::string& graphBody);

        static SMaterialCompileResult CompileSurface(
            const ShaderLoader* loader,
            const Material& material,
            SMaterialCompileResult result
        );

        static SMaterialCompileResult CompileParticleSprite(
            const ShaderLoader* loader,
            const Material& material,
            SMaterialCompileResult result
        );
    };
}
