#pragma once

#include <cstdint>

namespace Elixir::Materials
{
    /** Describes a row-major texture atlas independently of particle simulation. */
    struct STextureAtlas
    {
        /** Number of horizontal cells. */
        uint32_t Columns = 1;
        /** Number of vertical cells. */
        uint32_t Rows = 1;
        /** Number of usable cells, starting in the top-left corner. */
        uint32_t Frames = 1;
        /** Interpolates adjacent cells for fractional frame inputs. */
        bool Interpolate = false;

        /**
         * @brief Compares atlas layouts and interpolation policy.
         * @param other Layout to compare.
         */
        bool operator==(const STextureAtlas& other) const = default;

        /** @brief Checks cell counts and the supported frame-index range. */
        bool IsValid() const
        {
            const uint64_t cells = uint64_t(Columns) * Rows;
            return Columns > 0 && Rows > 0 && cells <= 16'777'216 && Frames > 0 &&
                   Frames <= cells;
        }
    };
}
