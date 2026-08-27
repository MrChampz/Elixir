#pragma once

#include <Engine/Icon/Icon.h>

namespace Elixir
{
    /** @brief Imports and rasterizes SVG icons with LunaSVG. */
    class ELIXIR_API LunaSVGIconLoader final : public IconLoader
    {
      public:
        /** @brief Get the SVG format supported by this loader. */
        EIconFormat GetFormat() const override { return EIconFormat::SVG; }

        /**
         * @brief Import one SVG document.
         * @param source Source file to import.
         * @return Rasterizable icon content, or nullptr when the SVG is invalid.
         */
        Ref<IconContent> Load(const SIconSource& source) const override;
    };
}
