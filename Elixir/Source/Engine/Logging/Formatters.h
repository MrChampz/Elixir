#pragma once

#include <Engine/Graphics/Image.h>
#include <Engine/Graphics/Utils.h>

using namespace Elixir;
using namespace Elixir::Graphics;

/**
 * Formatter for EImageFormat output in the logging system.
 */
template <>
struct std::formatter<EImageFormat> : std::formatter<std::string>
{
    auto format(const EImageFormat format, std::format_context& ctx) const
    {
        return std::formatter<std::string>::format(Utils::GetFormatString(format), ctx);
    }
};

/**
 * Formatter for Extent2D output in the logging system.
 */
template <>
struct std::formatter<Extent2D> : std::formatter<std::string>
{
    auto format(const Extent2D& extent, std::format_context& ctx) const
    {
        auto ss = std::stringstream();
        ss << "[Width = " << extent.Width << ", Height = " << extent.Height << "]";
        return std::formatter<std::string>::format(ss.str(), ctx);
    }
};