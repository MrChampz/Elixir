#pragma once

#include <Engine/Graphics/Image.h>

namespace Elixir::Graphics::Converters
{
    using namespace Elixir;

    static EImageFormat GetImageFormat(EDepthStencilImageFormat format)
    {
        return static_cast<EImageFormat>(format);
    }
}