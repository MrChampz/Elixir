#pragma once

#include <Engine/Graphics/GraphicsTypes.h>

namespace Elixir::Materials::Compilation
{
    /**
     * Supplies the expressions and resources available in one shader template.
     * Empty expressions or resource names identify unavailable inputs.
     */
    struct SMaterialEmissionEnvironment
    {
        /** One concrete stage; masks are rejected by the emitter. */
        EShaderStage Stage = EShaderStage::Pixel;

        /** World position before vertex displacement or interpolated after WPO in pixels. */
        std::string WorldPosition;

        /** Two-dimensional texture-coordinate expressions for channels zero and one. */
        std::array<std::string, 2> TexCoords;

        /** Expression that provides elapsed shader time in seconds. */
        std::string Time;

        /** RGBA color expression supplied by the geometry or particle renderer. */
        std::string Color;

        /** Surface normal expression available where the graph is evaluated. */
        std::string Normal;

        /** Direction toward the camera, expressed in the same space as Normal. */
        std::string ViewDirection;

        /** HLSL name of the bindless texture array indexed by material parameters. */
        std::string TextureArray;

        /** Linear wrap, linear clamp, point wrap, and point clamp, respectively. */
        std::array<std::string, 4> TextureSamplers;
    };
}
