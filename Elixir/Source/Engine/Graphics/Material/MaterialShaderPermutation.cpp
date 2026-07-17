#include "epch.h"
#include "MaterialShaderPermutation.h"

#include <array>

namespace Elixir
{
    namespace
    {
        constexpr std::array SURFACE_USAGES{
            EMaterialUsage::StaticMesh,
            EMaterialUsage::SkeletalMesh,
            EMaterialUsage::InstancedMesh,
            EMaterialUsage::ParticleSprite,
            EMaterialUsage::ParticleMesh,
        };

        constexpr SMaterialShaderPermutation SURFACE_STATIC_MESH{
            EMaterialDomain::Surface,
            EMaterialUsage::StaticMesh,
            EMaterialVertexFactory::StaticMesh,
            EMaterialShaderPass::BasePass,
        };
    }

    size_t SMaterialShaderPermutationHash::operator()(
        const SMaterialShaderPermutation& permutation) const
    {
        size_t hash = 1469598103934665603ull;
        const auto mix = [&](const uint8_t value)
        {
            hash ^= value;
            hash *= 1099511628211ull;
        };
        mix((uint8_t)permutation.Domain);
        const uint32_t usage = (uint32_t)permutation.Usage;
        for (size_t byte = 0; byte < sizeof(usage); ++byte)
            mix((uint8_t)(usage >> (byte * 8u)));
        mix((uint8_t)permutation.VertexFactory);
        mix((uint8_t)permutation.Pass);
        return hash;
    }

    std::optional<SMaterialShaderPermutation> MaterialShaderPermutation::ForUsage(
        const EMaterialDomain domain, const EMaterialUsage usage)
    {
        if (domain == EMaterialDomain::Surface)
        {
            EMaterialVertexFactory vertexFactory;
            switch (usage)
            {
                case EMaterialUsage::StaticMesh:
                    vertexFactory = EMaterialVertexFactory::StaticMesh;
                    break;
                case EMaterialUsage::SkeletalMesh:
                    vertexFactory = EMaterialVertexFactory::SkeletalMesh;
                    break;
                case EMaterialUsage::InstancedMesh:
                    vertexFactory = EMaterialVertexFactory::InstancedMesh;
                    break;
                case EMaterialUsage::ParticleSprite:
                    vertexFactory = EMaterialVertexFactory::ParticleSprite;
                    break;
                case EMaterialUsage::ParticleMesh:
                    vertexFactory = EMaterialVertexFactory::ParticleMesh;
                    break;
                default:
                    return std::nullopt;
            }
            return SMaterialShaderPermutation{
                domain, usage, vertexFactory, EMaterialShaderPass::BasePass };
        }
        if (domain == EMaterialDomain::PostProcess && usage == EMaterialUsage::None)
            return SMaterialShaderPermutation{
                domain, usage, EMaterialVertexFactory::FullscreenTriangle,
                EMaterialShaderPass::PostProcess };
        if (domain == EMaterialDomain::UserInterface && usage == EMaterialUsage::None)
            return SMaterialShaderPermutation{
                domain, usage, EMaterialVertexFactory::UserInterface,
                EMaterialShaderPass::UserInterface };
        return std::nullopt;
    }

    std::vector<SMaterialShaderPermutation> MaterialShaderPermutation::Expand(
        const EMaterialDomain domain, const EMaterialUsage usages)
    {
        std::vector<SMaterialShaderPermutation> permutations;
        if (domain == EMaterialDomain::Surface)
        {
            for (const EMaterialUsage usage : SURFACE_USAGES)
                if (HasMaterialUsage(usages, usage))
                    if (const auto permutation = ForUsage(domain, usage))
                        permutations.push_back(*permutation);
            return permutations;
        }
        if (const auto permutation = ForUsage(domain, usages))
            permutations.push_back(*permutation);
        return permutations;
    }

    bool MaterialShaderPermutation::Matches(
        const EMaterialDomain domain, const EMaterialUsage usages,
        const SMaterialShaderPermutation& permutation)
    {
        if (permutation.Domain != domain)
            return false;
        const auto expected = ForUsage(domain, permutation.Usage);
        if (!expected || *expected != permutation)
            return false;
        return domain == EMaterialDomain::Surface ? HasMaterialUsage(usages, permutation.Usage)
                                                  : usages == EMaterialUsage::None;
    }

    bool MaterialShaderPermutation::IsSupported(const SMaterialShaderPermutation& permutation)
    {
        // The current MeshRenderer/GraphMaterial contract consumes static-mesh
        // vertices in the Surface base pass. Other identities are intentionally
        // representable now, but become compilable only with their renderer contract.
        return permutation == SURFACE_STATIC_MESH;
    }

    const SMaterialShaderPermutation& MaterialShaderPermutation::SurfaceStaticMesh()
    {
        return SURFACE_STATIC_MESH;
    }
}
