#include "epch.h"
#include "MaterialDomain.h"

#include <array>
#include <initializer_list>

namespace Elixir
{
    namespace
    {
        constexpr uint32_t Channels(std::initializer_list<uint8_t> channels)
        {
            uint32_t mask = 0;
            for (const uint8_t channel : channels)
                mask |= 1u << channel;
            return mask;
        }

        constexpr EMaterialUsage SURFACE_USAGES = EMaterialUsage::StaticMesh
            | EMaterialUsage::SkeletalMesh | EMaterialUsage::InstancedMesh
            | EMaterialUsage::ParticleSprite | EMaterialUsage::ParticleMesh;

        constexpr std::array DOMAINS{
            SMaterialDomainDescriptor{
                EMaterialDomain::Surface,
                "Surface",
                Channels({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }),
                SURFACE_USAGES,
                EMaterialUsage::StaticMesh,
                true,
                "GraphMaterial.ps.hlsl",
                "GraphMaterial.vs.hlsl",
            },
            SMaterialDomainDescriptor{
                EMaterialDomain::PostProcess,
                "Post Process",
                Channels({ 3, 6 }), // Emissive carries colour; Opacity carries alpha.
                EMaterialUsage::None,
                EMaterialUsage::None,
                false,
                {},
                {},
            },
            SMaterialDomainDescriptor{
                EMaterialDomain::UserInterface,
                "User Interface",
                Channels({ 0, 6 }), // Base Color + Opacity.
                EMaterialUsage::None,
                EMaterialUsage::None,
                false,
                {},
                {},
            },
        };
    }

    const SMaterialDomainDescriptor* MaterialDomain::Find(const EMaterialDomain domain)
    {
        for (const auto& descriptor : DOMAINS)
            if (descriptor.Domain == domain)
                return &descriptor;
        return nullptr;
    }

    const SMaterialDomainDescriptor& MaterialDomain::Surface()
    {
        return DOMAINS[0];
    }

    uint32_t MaterialDomain::KnownUsageMask()
    {
        return (uint32_t)SURFACE_USAGES;
    }
}
