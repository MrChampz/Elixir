#pragma once

#include <Engine/Core/Core.h>

#include <cstdint>
#include <string_view>

namespace Elixir
{
    enum class EMaterialDomain : uint8_t
    {
        Surface,
        PostProcess,
        UserInterface,
    };

    enum class EMaterialUsage : uint32_t
    {
        None = 0,
        StaticMesh = 1u << 0u,
        SkeletalMesh = 1u << 1u,
        InstancedMesh = 1u << 2u,
        ParticleSprite = 1u << 3u,
        ParticleMesh = 1u << 4u,
    };

    constexpr EMaterialUsage operator|(const EMaterialUsage left, const EMaterialUsage right)
    {
        return (EMaterialUsage)((uint32_t)left | (uint32_t)right);
    }

    constexpr EMaterialUsage operator&(const EMaterialUsage left, const EMaterialUsage right)
    {
        return (EMaterialUsage)((uint32_t)left & (uint32_t)right);
    }

    constexpr EMaterialUsage& operator|=(EMaterialUsage& left, const EMaterialUsage right)
    {
        left = left | right;
        return left;
    }

    constexpr EMaterialUsage& operator&=(EMaterialUsage& left, const EMaterialUsage right)
    {
        left = left & right;
        return left;
    }

    [[nodiscard]] constexpr bool HasMaterialUsage(
        const EMaterialUsage usages, const EMaterialUsage usage)
    {
        return ((uint32_t)usages & (uint32_t)usage) != 0;
    }

    struct SMaterialDomainDescriptor
    {
        EMaterialDomain Domain = EMaterialDomain::Surface;
        std::string_view Name;
        uint32_t AllowedChannelMask = 0;
        EMaterialUsage AllowedUsages = EMaterialUsage::None;
        EMaterialUsage DefaultUsages = EMaterialUsage::None;
        bool ShaderContractAvailable = false;
        std::string_view PixelTemplate;
        std::string_view VertexTemplate;

        [[nodiscard]] bool AllowsChannel(const uint8_t channel) const
        {
            return channel < 32 && (AllowedChannelMask & (1u << channel)) != 0;
        }
    };

    class ELIXIR_API MaterialDomain
    {
      public:
        [[nodiscard]] static const SMaterialDomainDescriptor* Find(EMaterialDomain domain);
        [[nodiscard]] static const SMaterialDomainDescriptor& Surface();
        [[nodiscard]] static uint32_t KnownUsageMask();
    };
}
