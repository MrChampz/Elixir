#pragma once

#include <iostream>
#include <stdint.h>

namespace Elixir
{
    struct SResourceHandle
    {
        enum class EResourceType : uint8_t
        {
            Texture = 0,
            Sampler,
            ConstantBuffer
        };

        EResourceType Type;
        uint32_t Index = UINT32_MAX;

        static SResourceHandle Texture(const uint32_t index)
        {
            return { EResourceType::Texture, index };
        }

        static SResourceHandle Sampler(const uint32_t index)
        {
            return { EResourceType::Sampler, index };
        }

        static SResourceHandle ConstantBuffer(const uint32_t index)
        {
            return { EResourceType::ConstantBuffer, index };
        }

        bool IsValid() const { return Index != UINT32_MAX; }

        std::string ToString() const
        {
            auto typeStr = "";
            switch (Type)
            {
                case EResourceType::Texture: typeStr = "Texture"; break;
                case EResourceType::Sampler: typeStr = "Sampler"; break;
                case EResourceType::ConstantBuffer: typeStr = "ConstantBuffer"; break;
            }
            return "SResourceHandle[Type = " + std::string(typeStr) + ", Index = " + std::to_string(Index) + "]";
        }

        auto GetHashParams() const
        {
            return std::make_tuple(Type, Index);
        }

        bool operator==(const SResourceHandle& other) const
        {
            return this->Type == other.Type && this->Index == other.Index;
        }

        bool operator!=(const SResourceHandle& other) const
        {
            return !(*this == other);
        }

        std::ostream& operator<<(std::ostream& os) const
        {
            os << ToString();
            return os;
        }
    };
}

GENERATE_HASH_FUNCTION(SResourceHandle)

template <>
struct std::formatter<SResourceHandle> : std::formatter<std::string>
{
    auto format(const SResourceHandle& handle, std::format_context& ctx) const
    {
        return std::formatter<std::string>::format(handle.ToString(), ctx);
    }
};