#pragma once

#include <Engine/Core/Core.h>

namespace Elixir::Aether::Rendering
{
    /** Stores immutable geometry configuration owned by an emitter. */
    struct ELIXIR_API SEmitterRenderConfig
    {
        /** @brief Releases configuration through the renderer interface. */
        virtual ~SEmitterRenderConfig() = default;

        /** @brief Reports whether the geometry configuration is valid. */
        virtual bool IsValid() const = 0;
    };
}
