#pragma once

#include <Engine/Core/Types.h>

#include <vulkan/vulkan.h>

namespace Elixir::Vulkan::Converters
{
    static VkOffset2D GetOffset2D(const Offset2D src)
	{
		return { .x = src.X, .y = src.Y };
	}

	static VkOffset3D GetOffset3D(const Offset3D src)
	{
		return { .x = src.X, .y = src.Y, .z = src.Z };
	}

	static VkExtent2D GetExtent2D(const Extent2D src)
	{
		return { .width = src.Width, .height = src.Height };
	}

	static VkExtent3D GetExtent3D(const Extent3D src)
	{
		return { .width = src.Width, .height = src.Height, .depth = src.Depth };
	}

	static VkRect2D GetRect2D(const Rect2D src)
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
}