#pragma once

#include <Engine/Core/Core.h>

namespace Elixir
{
    enum class EMemoryProperty
    {
        DeviceLocal				= 0x00000001,
        HostVisible				= 0x00000002,
        HostCoherent		    = 0x00000004,
        HostCached				= 0x00000008,
        LazilyAllocated			= 0x00000010
    };

    GENERATE_ENUM_CLASS_OPERATORS(EMemoryProperty)

    struct SAllocationInfo
    {
        EMemoryProperty RequiredFlags;
        EMemoryProperty PreferredFlags;
    };
}