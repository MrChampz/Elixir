#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Graphics/Material/MaterialDomain.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Elixir
{
    enum class EMaterialVertexFactory : uint8_t
    {
        StaticMesh,
        SkeletalMesh,
        InstancedMesh,
        ParticleSprite,
        ParticleMesh,
        FullscreenTriangle,
        UserInterface,
    };

    enum class EMaterialShaderPass : uint8_t
    {
        BasePass,
        PostProcess,
        UserInterface,
    };

    // One independently compilable renderer target. Usage says why the material
    // requested it; vertex factory and pass describe the shader contract it must fit.
    struct SMaterialShaderPermutation
    {
        EMaterialDomain Domain = EMaterialDomain::Surface;
        EMaterialUsage Usage = EMaterialUsage::StaticMesh;
        EMaterialVertexFactory VertexFactory = EMaterialVertexFactory::StaticMesh;
        EMaterialShaderPass Pass = EMaterialShaderPass::BasePass;

        bool operator==(const SMaterialShaderPermutation&) const = default;
    };

    struct SMaterialShaderPermutationHash
    {
        size_t operator()(const SMaterialShaderPermutation& permutation) const;
    };

    class ELIXIR_API MaterialShaderPermutation
    {
      public:
        [[nodiscard]] static std::optional<SMaterialShaderPermutation> ForUsage(
            EMaterialDomain domain, EMaterialUsage usage);
        [[nodiscard]] static std::vector<SMaterialShaderPermutation> Expand(
            EMaterialDomain domain, EMaterialUsage usages);
        [[nodiscard]] static bool Matches(
            EMaterialDomain domain, EMaterialUsage usages,
            const SMaterialShaderPermutation& permutation);
        [[nodiscard]] static bool IsSupported(const SMaterialShaderPermutation& permutation);
        [[nodiscard]] static const SMaterialShaderPermutation& SurfaceStaticMesh();
    };
}
