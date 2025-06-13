#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Logging/Log.h>
#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/Image.h>

#include <magic_enum/magic_enum.hpp>
#include <glm/glm.hpp>

namespace Elixir::Graphics::Utils
{
    inline size_t GetDataTypeSize(const EDataType type)
    {
        switch (type)
        {
            case EDataType::Bool:		    return 1;
            case EDataType::Float:		    return 4;
            case EDataType::Vec2:	        return 8;
            case EDataType::Vec3:	        return 12;
            case EDataType::Vec4:	        return 16;
            case EDataType::Int:		    return 4;
            case EDataType::IntVec2:		return 8;
            case EDataType::IntVec3:		return 12;
            case EDataType::IntVec4:		return 16;
            case EDataType::Mat3:		    return 36;
            case EDataType::Mat4:		    return 64;
            default:
                EE_CORE_ASSERT(false, "Unknown DataType!")
        }

        return 0;
    }

    inline uint32_t GetDataTypeComponentCount(const EDataType type)
    {
        switch (type)
        {
            case EDataType::Bool:		    return 1;
            case EDataType::Float:		    return 1;
            case EDataType::Vec2:	        return 2;
            case EDataType::Vec3:	        return 3;
            case EDataType::Vec4:	        return 4;
            case EDataType::Int:		    return 1;
            case EDataType::IntVec2:		return 2;
            case EDataType::IntVec3:		return 3;
            case EDataType::IntVec4:		return 4;
            case EDataType::Mat3:		    return 9;
            case EDataType::Mat4:		    return 16;
            default:
                EE_CORE_ASSERT(false, "Unknown DataType!")
        }

        return 0;
    }

    inline std::string GetFormatString(const EImageFormat format)
    {
        return std::string(magic_enum::enum_name(format));
    }

    inline uint32_t GetFormatBlockSizeBits(const Image* image)
    {
        switch (image->GetFormat())
        {
            case EImageFormat::R16_SFLOAT:
                return 16;
            case EImageFormat::R8G8B8_UNORM:
            case EImageFormat::R8G8B8_SNORM:
            case EImageFormat::R8G8B8_USCALED:
            case EImageFormat::R8G8B8_SSCALED:
            case EImageFormat::R8G8B8_UINT:
            case EImageFormat::R8G8B8_SINT:
            case EImageFormat::R8G8B8_SRGB:
                return 3 * 8;
            case EImageFormat::R16G16B16_UNORM:
            case EImageFormat::R16G16B16_SNORM:
            case EImageFormat::R16G16B16_USCALED:
            case EImageFormat::R16G16B16_SSCALED:
            case EImageFormat::R16G16B16_UINT:
            case EImageFormat::R16G16B16_SINT:
            case EImageFormat::R16G16B16_SFLOAT:
                return 6 * 8;
            case EImageFormat::R32G32B32_UINT:
            case EImageFormat::R32G32B32_SINT:
            case EImageFormat::R32G32B32_SFLOAT:
                return 12 * 8;
            case EImageFormat::R64G64B64_UINT:
            case EImageFormat::R64G64B64_SINT:
            case EImageFormat::R64G64B64_SFLOAT:
                return 24 * 8;
            case EImageFormat::R8G8B8A8_UNORM:
            case EImageFormat::R8G8B8A8_SNORM:
            case EImageFormat::R8G8B8A8_USCALED:
            case EImageFormat::R8G8B8A8_SSCALED:
            case EImageFormat::R8G8B8A8_UINT:
            case EImageFormat::R8G8B8A8_SINT:
            case EImageFormat::R8G8B8A8_SRGB:
                return 4 * 8;
            case EImageFormat::R16G16B16A16_UNORM:
            case EImageFormat::R16G16B16A16_SNORM:
            case EImageFormat::R16G16B16A16_USCALED:
            case EImageFormat::R16G16B16A16_SSCALED:
            case EImageFormat::R16G16B16A16_UINT:
            case EImageFormat::R16G16B16A16_SINT:
            case EImageFormat::R16G16B16A16_SFLOAT:
                return 8 * 8;
            case EImageFormat::R32G32B32A32_UINT:
            case EImageFormat::R32G32B32A32_SINT:
            case EImageFormat::R32G32B32A32_SFLOAT:
                return 16 * 8;
            case EImageFormat::R64G64B64A64_UINT:
            case EImageFormat::R64G64B64A64_SINT:
            case EImageFormat::R64G64B64A64_SFLOAT:
                return 32 * 8;
            case EImageFormat::D16_UNORM:
                return 16;
            case EImageFormat::D24_UNORM_S8_UINT:
            case EImageFormat::D32_SFLOAT:
            case EImageFormat::X8_D24_UNORM_PACK32:
                return 32;
            case EImageFormat::D32_SFLOAT_S8_UINT:
                return 40;
            case EImageFormat::BC1_RGB_UNORM_BLOCK:
            case EImageFormat::BC1_RGB_SRGB_BLOCK:
            case EImageFormat::BC1_RGBA_UNORM_BLOCK:
            case EImageFormat::BC1_RGBA_SRGB_BLOCK:
                return 8 * 8;
            case EImageFormat::BC2_UNORM_BLOCK:
            case EImageFormat::BC2_SRGB_BLOCK:
            case EImageFormat::BC3_UNORM_BLOCK:
            case EImageFormat::BC3_SRGB_BLOCK:
                return 16 * 8;
            case EImageFormat::BC4_UNORM_BLOCK:
            case EImageFormat::BC4_SNORM_BLOCK:
                return 8 * 8;
            case EImageFormat::BC5_UNORM_BLOCK:
            case EImageFormat::BC5_SNORM_BLOCK:
            case EImageFormat::BC6H_UFLOAT_BLOCK:
            case EImageFormat::BC6H_SFLOAT_BLOCK:
            case EImageFormat::BC7_UNORM_BLOCK:
            case EImageFormat::BC7_SRGB_BLOCK:
                return 16 * 8;
            default:
                EE_CORE_ASSERT(false, "Unknown ImageFormat!")
        }

        return 0;
    }

    inline glm::ivec3 GetFormatBlockExtent(const Image* image)
    {
        switch (image->GetFormat())
        {
            case EImageFormat::R16_SFLOAT:
            case EImageFormat::R8G8B8_UNORM:
            case EImageFormat::R8G8B8_SNORM:
            case EImageFormat::R8G8B8_USCALED:
            case EImageFormat::R8G8B8_SSCALED:
            case EImageFormat::R8G8B8_UINT:
            case EImageFormat::R8G8B8_SINT:
            case EImageFormat::R8G8B8_SRGB:
            case EImageFormat::R16G16B16_UNORM:
            case EImageFormat::R16G16B16_SNORM:
            case EImageFormat::R16G16B16_USCALED:
            case EImageFormat::R16G16B16_SSCALED:
            case EImageFormat::R16G16B16_UINT:
            case EImageFormat::R16G16B16_SINT:
            case EImageFormat::R16G16B16_SFLOAT:
            case EImageFormat::R32G32B32_UINT:
            case EImageFormat::R32G32B32_SINT:
            case EImageFormat::R32G32B32_SFLOAT:
            case EImageFormat::R64G64B64_UINT:
            case EImageFormat::R64G64B64_SINT:
            case EImageFormat::R64G64B64_SFLOAT:
            case EImageFormat::R8G8B8A8_UNORM:
            case EImageFormat::R8G8B8A8_SNORM:
            case EImageFormat::R8G8B8A8_USCALED:
            case EImageFormat::R8G8B8A8_SSCALED:
            case EImageFormat::R8G8B8A8_UINT:
            case EImageFormat::R8G8B8A8_SINT:
            case EImageFormat::R8G8B8A8_SRGB:
            case EImageFormat::R16G16B16A16_UNORM:
            case EImageFormat::R16G16B16A16_SNORM:
            case EImageFormat::R16G16B16A16_USCALED:
            case EImageFormat::R16G16B16A16_SSCALED:
            case EImageFormat::R16G16B16A16_UINT:
            case EImageFormat::R16G16B16A16_SINT:
            case EImageFormat::R16G16B16A16_SFLOAT:
            case EImageFormat::R32G32B32A32_UINT:
            case EImageFormat::R32G32B32A32_SINT:
            case EImageFormat::R32G32B32A32_SFLOAT:
            case EImageFormat::R64G64B64A64_UINT:
            case EImageFormat::R64G64B64A64_SINT:
            case EImageFormat::R64G64B64A64_SFLOAT:
            case EImageFormat::D16_UNORM:
            case EImageFormat::D24_UNORM_S8_UINT:
            case EImageFormat::D32_SFLOAT:
            case EImageFormat::X8_D24_UNORM_PACK32:
            case EImageFormat::D32_SFLOAT_S8_UINT:
                return glm::vec3{1, 1, 1};
            case EImageFormat::BC1_RGB_UNORM_BLOCK:
            case EImageFormat::BC1_RGB_SRGB_BLOCK:
            case EImageFormat::BC1_RGBA_UNORM_BLOCK:
            case EImageFormat::BC1_RGBA_SRGB_BLOCK:
            case EImageFormat::BC2_UNORM_BLOCK:
            case EImageFormat::BC2_SRGB_BLOCK:
            case EImageFormat::BC3_UNORM_BLOCK:
            case EImageFormat::BC3_SRGB_BLOCK:
            case EImageFormat::BC4_UNORM_BLOCK:
            case EImageFormat::BC4_SNORM_BLOCK:
            case EImageFormat::BC5_UNORM_BLOCK:
            case EImageFormat::BC5_SNORM_BLOCK:
            case EImageFormat::BC6H_UFLOAT_BLOCK:
            case EImageFormat::BC6H_SFLOAT_BLOCK:
            case EImageFormat::BC7_UNORM_BLOCK:
            case EImageFormat::BC7_SRGB_BLOCK:
                return glm::vec3{4, 4, 1};
            default:
                EE_CORE_ASSERT(false, "Unknown ImageFormat!")
        }

        return glm::vec3{};
    }

    static EImageAspect CalculateImageAspect(const EImageUsage usage, const EImageFormat format)
	{
		auto aspect = EImageAspect::None;

		if (usage & EImageUsage::ColorAttachment)
			aspect = aspect | EImageAspect::Color;

		if (usage & EImageUsage::Sampled)
			aspect = aspect | EImageAspect::Color;

		if (usage & EImageUsage::Storage)
			aspect = aspect | EImageAspect::Color;

		if (usage & EImageUsage::DepthStencilAttachment)
		{
			switch (format)
			{
			case EImageFormat::D16_UNORM:
			case EImageFormat::D32_SFLOAT:
			case EImageFormat::X8_D24_UNORM_PACK32:
				aspect = aspect | EImageAspect::Depth;
				break;
			case EImageFormat::D24_UNORM_S8_UINT:
			case EImageFormat::D32_SFLOAT_S8_UINT:
				aspect = aspect | EImageAspect::Depth | EImageAspect::Stencil;
				break;
			default:
				break;
			}
		}

		return aspect;
	}
}