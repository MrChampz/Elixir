#pragma once

#include <Engine/Core/Types.h>
#include <Engine/Graphics/Memory.h>
#include <Engine/Graphics/Image.h>

#include <vulkan/vulkan.h>

namespace Elixir::Vulkan::Converters
{
    static VkOffset2D GetOffset2D(const Offset2D& src)
	{
		return { .x = src.X, .y = src.Y };
	}

	static VkOffset3D GetOffset3D(const Offset3D& src)
	{
		return { .x = src.X, .y = src.Y, .z = src.Z };
	}

	static VkExtent2D GetExtent2D(const Extent2D& src)
	{
		return { .width = src.Width, .height = src.Height };
	}

	static VkExtent3D GetExtent3D(const Extent3D& src)
	{
		return { .width = src.Width, .height = src.Height, .depth = src.Depth };
	}

	static VkRect2D GetRect2D(const Rect2D& src)
	{
		return { .offset = GetOffset2D(src.Offset), .extent = GetExtent2D(src.Extent) };
	}

	static VkViewport GetViewport(const Viewport& viewport)
	{
		return {
			.x			= viewport.X,
			.y			= viewport.Y,
			.width		= viewport.Width,
			.height		= viewport.Height,
			.minDepth	= viewport.MinDepth,
			.maxDepth	= viewport.MaxDepth
		};
	}

    static VkMemoryPropertyFlags GetMemoryProperties(const EMemoryProperty& properties)
    {
        VkMemoryPropertyFlags flags = 0;

        if (properties & EMemoryProperty::DeviceLocal)
            flags = flags | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if (properties & EMemoryProperty::HostVisible)
            flags = flags | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        if (properties & EMemoryProperty::HostCoherent)
            flags = flags | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        if (properties & EMemoryProperty::HostCached)
            flags = flags | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

        if (properties & EMemoryProperty::LazilyAllocated)
            flags = flags | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;

        return flags;
    }

    static VkBufferUsageFlags GetBufferUsage(const EBufferUsage& usage)
    {
        VkBufferUsageFlags flags = 0;

        if (usage & EBufferUsage::TransferSrc)
            flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        if (usage & EBufferUsage::TransferDst)
            flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        if (usage & EBufferUsage::UniformTexelBuffer)
            flags |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;

        if (usage & EBufferUsage::StorageTexelBuffer)
            flags |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;

        if (usage & EBufferUsage::UniformBuffer)
            flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        if (usage & EBufferUsage::StorageBuffer)
            flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        if (usage & EBufferUsage::IndexBuffer)
            flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        if (usage & EBufferUsage::VertexBuffer)
            flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        if (usage & EBufferUsage::IndirectBuffer)
            flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

        return flags;
    }

    static VkFormat GetFormat(const EImageFormat& format)
    {
        switch (format)
        {
            case EImageFormat::R16_SFLOAT:
                return VK_FORMAT_R16_SFLOAT;
            case EImageFormat::R8G8B8_UNORM:
                return VK_FORMAT_R8G8B8_UNORM;
            case EImageFormat::R8G8B8_SNORM:
                return VK_FORMAT_R8G8B8_SNORM;
            case EImageFormat::R8G8B8_USCALED:
                return VK_FORMAT_R8G8B8_USCALED;
            case EImageFormat::R8G8B8_SSCALED:
                return VK_FORMAT_R8G8B8_SSCALED;
            case EImageFormat::R8G8B8_UINT:
                return VK_FORMAT_R8G8B8_UINT;
            case EImageFormat::R8G8B8_SINT:
                return VK_FORMAT_R8G8B8_SINT;
            case EImageFormat::R8G8B8_SRGB:
                return VK_FORMAT_R8G8B8_SRGB;
            case EImageFormat::R16G16B16_UNORM:
                return VK_FORMAT_R16G16B16_UNORM;
            case EImageFormat::R16G16B16_SNORM:
                return VK_FORMAT_R16G16B16_SNORM;
            case EImageFormat::R16G16B16_USCALED:
                return VK_FORMAT_R16G16B16_USCALED;
            case EImageFormat::R16G16B16_SSCALED:
                return VK_FORMAT_R16G16B16_SSCALED;
            case EImageFormat::R16G16B16_UINT:
                return VK_FORMAT_R16G16B16_UINT;
            case EImageFormat::R16G16B16_SINT:
                return VK_FORMAT_R16G16B16_SINT;
            case EImageFormat::R16G16B16_SFLOAT:
                return VK_FORMAT_R16G16B16_SFLOAT;
            case EImageFormat::R32G32B32_UINT:
                return VK_FORMAT_R32G32B32_UINT;
            case EImageFormat::R32G32B32_SINT:
                return VK_FORMAT_R32G32B32_SINT;
            case EImageFormat::R32G32B32_SFLOAT:
                return VK_FORMAT_R32G32B32_SFLOAT;
            case EImageFormat::R64G64B64_UINT:
                return VK_FORMAT_R64G64B64_UINT;
            case EImageFormat::R64G64B64_SINT:
                return VK_FORMAT_R64G64B64_SINT;
            case EImageFormat::R64G64B64_SFLOAT:
                return VK_FORMAT_R64G64B64_SFLOAT;
            case EImageFormat::R8G8B8A8_UNORM:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case EImageFormat::R8G8B8A8_SNORM:
                return VK_FORMAT_R8G8B8A8_SNORM;
            case EImageFormat::R8G8B8A8_USCALED:
                return VK_FORMAT_R8G8B8A8_USCALED;
            case EImageFormat::R8G8B8A8_SSCALED:
                return VK_FORMAT_R8G8B8A8_SSCALED;
            case EImageFormat::R8G8B8A8_UINT:
                return VK_FORMAT_R8G8B8A8_UINT;
            case EImageFormat::R8G8B8A8_SINT:
                return VK_FORMAT_R8G8B8A8_SINT;
            case EImageFormat::R8G8B8A8_SRGB:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case EImageFormat::R16G16B16A16_UNORM:
                return VK_FORMAT_R16G16B16A16_UNORM;
            case EImageFormat::R16G16B16A16_SNORM:
                return VK_FORMAT_R16G16B16A16_SNORM;
            case EImageFormat::R16G16B16A16_USCALED:
                return VK_FORMAT_R16G16B16A16_USCALED;
            case EImageFormat::R16G16B16A16_SSCALED:
                return VK_FORMAT_R16G16B16A16_SSCALED;
            case EImageFormat::R16G16B16A16_UINT:
                return VK_FORMAT_R16G16B16A16_UINT;
            case EImageFormat::R16G16B16A16_SINT:
                return VK_FORMAT_R16G16B16A16_SINT;
            case EImageFormat::R16G16B16A16_SFLOAT:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case EImageFormat::R32G32B32A32_UINT:
                return VK_FORMAT_R32G32B32A32_UINT;
            case EImageFormat::R32G32B32A32_SINT:
                return VK_FORMAT_R32G32B32A32_SINT;
            case EImageFormat::R32G32B32A32_SFLOAT:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case EImageFormat::R64G64B64A64_UINT:
                return VK_FORMAT_R64G64B64A64_UINT;
            case EImageFormat::R64G64B64A64_SINT:
                return VK_FORMAT_R64G64B64A64_SINT;
            case EImageFormat::R64G64B64A64_SFLOAT:
                return VK_FORMAT_R64G64B64A64_SFLOAT;
            case EImageFormat::BC1_RGB_UNORM_BLOCK:
                return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
            case EImageFormat::BC1_RGB_SRGB_BLOCK:
                return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
            case EImageFormat::BC1_RGBA_UNORM_BLOCK:
                return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
            case EImageFormat::BC1_RGBA_SRGB_BLOCK:
                return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
            case EImageFormat::BC2_UNORM_BLOCK:
                return VK_FORMAT_BC2_UNORM_BLOCK;
            case EImageFormat::BC2_SRGB_BLOCK:
                return VK_FORMAT_BC2_SRGB_BLOCK;
            case EImageFormat::BC3_UNORM_BLOCK:
                return VK_FORMAT_BC3_UNORM_BLOCK;
            case EImageFormat::BC3_SRGB_BLOCK:
                return VK_FORMAT_BC3_SRGB_BLOCK;
            case EImageFormat::BC4_UNORM_BLOCK:
                return VK_FORMAT_BC4_UNORM_BLOCK;
            case EImageFormat::BC4_SNORM_BLOCK:
                return VK_FORMAT_BC4_SNORM_BLOCK;
            case EImageFormat::BC5_UNORM_BLOCK:
                return VK_FORMAT_BC5_UNORM_BLOCK;
            case EImageFormat::BC5_SNORM_BLOCK:
                return VK_FORMAT_BC5_SNORM_BLOCK;
            case EImageFormat::BC6H_UFLOAT_BLOCK:
                return VK_FORMAT_BC6H_UFLOAT_BLOCK;
            case EImageFormat::BC6H_SFLOAT_BLOCK:
                return VK_FORMAT_BC6H_SFLOAT_BLOCK;
            case EImageFormat::BC7_UNORM_BLOCK:
                return VK_FORMAT_BC7_UNORM_BLOCK;
            case EImageFormat::BC7_SRGB_BLOCK:
                return VK_FORMAT_BC7_SRGB_BLOCK;
            case EImageFormat::D16_UNORM:
                return VK_FORMAT_D16_UNORM;
            case EImageFormat::D24_UNORM_S8_UINT:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case EImageFormat::D32_SFLOAT:
                return VK_FORMAT_D32_SFLOAT;
            case EImageFormat::D32_SFLOAT_S8_UINT:
                return VK_FORMAT_D32_SFLOAT_S8_UINT;
            case EImageFormat::X8_D24_UNORM_PACK32:
                return VK_FORMAT_X8_D24_UNORM_PACK32;
            case EImageFormat::Undefined:
                return VK_FORMAT_UNDEFINED;
            default:
                EE_CORE_ASSERT(false, "Unknown ImageFormat!")
                break;
        }

        return VK_FORMAT_UNDEFINED;
    }

    static VkImageLayout GetImageLayout(const EImageLayout& layout)
    {
        switch (layout)
        {
            case EImageLayout::Undefined:
				return VK_IMAGE_LAYOUT_UNDEFINED;
			case EImageLayout::General:
				return VK_IMAGE_LAYOUT_GENERAL;
			case EImageLayout::ColorAttachment:
				return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			case EImageLayout::DepthAttachment:
				return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			case EImageLayout::StencilAttachment:
				return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
			case EImageLayout::DepthStencilAttachment:
				return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			case EImageLayout::DepthReadOnly:
				return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
			case EImageLayout::StencilReadOnly:
				return VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
			case EImageLayout::DepthStencilReadOnly:
				return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			case EImageLayout::ShaderReadOnly:
				return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			case EImageLayout::TransferSrc:
				return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			case EImageLayout::TransferDst:
				return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			case EImageLayout::PreInitialized:
				return VK_IMAGE_LAYOUT_PREINITIALIZED;
			case EImageLayout::PresentSrc:
				return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            default:
                EE_CORE_ASSERT(false, "Unknown ImageLayout!")
                break;
        }

        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    static VkImageType GetImageType(const EImageType& type)
	{
		switch (type)
		{
		case EImageType::_1D:
			return VK_IMAGE_TYPE_1D;
		case EImageType::_2D:
			return VK_IMAGE_TYPE_2D;
		case EImageType::_3D:
			return VK_IMAGE_TYPE_3D;
		default:
			EE_CORE_ASSERT(false, "Unknown ImageType!")
		    break;
		}

        return VK_IMAGE_TYPE_MAX_ENUM;
	}

    static VkImageUsageFlags GetImageUsage(const EImageUsage usage)
	{
		VkImageUsageFlags flags = 0;

		if (usage & EImageUsage::TransferSrc)
			flags = flags | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		if (usage & EImageUsage::TransferDst)
			flags = flags | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		if (usage & EImageUsage::Sampled)
			flags = flags | VK_IMAGE_USAGE_SAMPLED_BIT;

		if (usage & EImageUsage::Storage)
			flags = flags | VK_IMAGE_USAGE_STORAGE_BIT;

		if (usage & EImageUsage::ColorAttachment)
			flags = flags | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		if (usage & EImageUsage::DepthStencilAttachment)
			flags = flags | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		if (usage & EImageUsage::TransientAttachment)
			flags = flags | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

		if (usage & EImageUsage::InputAttachment)
			flags = flags | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		return flags;
	}

    static VkImageAspectFlags GetImageAspect(const EImageAspect& aspect)
	{
		VkImageAspectFlags flags = VK_IMAGE_ASPECT_NONE;

		if (aspect & EImageAspect::Color)
			flags = flags | VK_IMAGE_ASPECT_COLOR_BIT;

		if (aspect & EImageAspect::Depth)
			flags = flags | VK_IMAGE_ASPECT_DEPTH_BIT;

		if (aspect & EImageAspect::Stencil)
			flags = flags | VK_IMAGE_ASPECT_STENCIL_BIT;

		if (aspect & EImageAspect::Metadata)
			flags = flags | VK_IMAGE_ASPECT_METADATA_BIT;

		if (aspect & EImageAspect::Plane0)
			flags = flags | VK_IMAGE_ASPECT_PLANE_0_BIT;

		if (aspect & EImageAspect::Plane1)
			flags = flags | VK_IMAGE_ASPECT_PLANE_1_BIT;

		if (aspect & EImageAspect::Plane2)
			flags = flags | VK_IMAGE_ASPECT_PLANE_2_BIT;

		return flags;
	}

    static VkImageViewType GetImageViewType(const EImageType& type, const uint32_t layers)
	{
		switch (type)
		{
		case EImageType::_1D:
			if (layers > 1) return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
			return VK_IMAGE_VIEW_TYPE_1D;
		case EImageType::_2D:
			if (layers > 1) return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			return VK_IMAGE_VIEW_TYPE_2D;
		case EImageType::_3D:
			return VK_IMAGE_VIEW_TYPE_3D;
		default:
		    EE_CORE_ASSERT(false, "Unknown ImageViewType!")
		    break;
		}

        return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
	}
}