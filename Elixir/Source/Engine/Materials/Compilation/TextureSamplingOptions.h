#pragma once

#include <cstdint>

namespace Elixir::Materials::Compilation
{
    /** Select texture addressing outside the unit UV rectangle. */
    enum class ETextureAddress : uint8_t
    {
        Wrap, Clamp
    };

    /** Select interpolation between texels. */
    enum class ETextureFilter : uint8_t
    {
        Linear, Point
    };

    /** Select implicit derivatives, an explicit level, or a derivative bias. */
    enum class ETextureMipMode : uint8_t
    {
        Auto, Level, Bias
    };

    /** Configure the sampler and mip selection used to emit a texture lookup. */
    struct STextureSamplingOptions
    {
        /** Addressing used outside the unit UV rectangle. */
        ETextureAddress Address = ETextureAddress::Wrap;

        /** Interpolation used between texels. */
        ETextureFilter Filter = ETextureFilter::Linear;

        /** Interpretation of the mip expression; Auto uses level zero outside Pixel. */
        ETextureMipMode MipMode = ETextureMipMode::Auto;
    };
}
