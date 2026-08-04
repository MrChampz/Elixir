#pragma once

#include <glm/glm.hpp>

namespace Elixir::Aether
{
    // Serialized authoring data local to the Aether effect format.
    struct SParticleMaterialDefinition
    {
        glm::vec3 BaseColor{ 1.0f };
        float Opacity = 1.0f;
        glm::vec3 Emissive{ 0.0f };
        std::string BaseColorTexturePath;
    };
}
