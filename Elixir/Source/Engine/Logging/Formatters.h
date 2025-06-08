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