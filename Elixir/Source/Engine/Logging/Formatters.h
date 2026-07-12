#pragma once

#include <Engine/Graphics/Image.h>
#include <Engine/Graphics/Utils.h>

/**
 * Formatter for EImageFormat output in the logging system.
 */
template <>
struct std::formatter<Elixir::EImageFormat> : std::formatter<std::string>
{
    auto format(const Elixir::EImageFormat format, std::format_context& ctx) const
    {
        return std::formatter<std::string>::format(Elixir::Graphics::Utils::GetFormatString(format), ctx);
    }
};

/**
 * Formatter for Extent2D output in the logging system.
 */
template <>
struct std::formatter<Elixir::Extent2D> : std::formatter<std::string>
{
    auto format(const Elixir::Extent2D& extent, std::format_context& ctx) const
    {
        auto ss = std::stringstream();
        ss << "[Width = " << extent.Width << ", Height = " << extent.Height << "]";
        return std::formatter<std::string>::format(ss.str(), ctx);
    }
};

/**
 * Formatter for Extent3D output in the logging system.
 */
template <>
struct std::formatter<Elixir::Extent3D> : std::formatter<std::string>
{
    auto format(const Elixir::Extent3D& extent, std::format_context& ctx) const
    {
        auto ss = std::stringstream();
        ss << "[Width = " << extent.Width << ", Height = " << extent.Height << ", Depth = " << extent.Depth << "]";
        return std::formatter<std::string>::format(ss.str(), ctx);
    }
};
