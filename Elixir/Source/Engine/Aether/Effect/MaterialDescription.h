#pragma once

#include <glm/glm.hpp>

namespace Elixir::Aether::Effect
{
    /**
     * @brief Stores material authoring data serialized in an Aether effect.
     *
     * This structure represents the material data embedded in an effect asset. It
     * is not a runtime Material, MaterialInstance, or GPU render proxy.
     *
     * Effect::MaterialResolver converts this description into a material when the
     * owning System is compiled.
     */
    struct SMaterialDescription
    {
        glm::vec3 BaseColor{ 1.0f };
        float Opacity = 1.0f;
        glm::vec3 Emissive{ 0.0f };
        std::string BaseColorTexturePath;
    };
}
