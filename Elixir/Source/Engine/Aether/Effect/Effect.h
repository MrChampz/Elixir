#pragma once

#include <Engine/Aether/System.h>
#include <string_view>

namespace Elixir::Aether::Effect
{
    /**
     * @brief Loads an Aether effect asset into mutable authoring data.
     *
     * The function parses an effect file and creates its System, emitters, modules,
     * parameters, curves, and serialized material descriptions. It does not resolve
     * materials, compile the system, allocate GPU resources, or create a runtime
     * SystemInstance.
     *
     * @param filepath Path to the effect asset to load.
     * @return The parsed System when loading succeeds.
     * @return Null when the file cannot be read or contains invalid effect data.
     */
    ELIXIR_API Ref<System> LoadEffectFile(const std::filesystem::path& filepath);

    /** @brief Parses effect authoring data from memory and optionally returns the validation error. */
    ELIXIR_API Ref<System> LoadEffectText(std::string_view source, std::string* error = nullptr);
}
