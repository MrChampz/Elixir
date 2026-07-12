#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/Texture.h>

namespace Elixir
{
    struct SModelVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoord;
    };

    // One drawable chunk of a model: a vertex/index buffer pair with the world
    // transform of its node and its (flattened) base-color material.
    struct SModelPrimitive
    {
        Ref<VertexBuffer> Vertices;
        Ref<IndexBuffer> Indices;
        uint32_t IndexCount = 0;
        glm::mat4 Transform{ 1.0f };
        Ref<Texture> BaseColorTexture; // nullable
        glm::vec4 BaseColorFactor{ 1.0f };
    };

    // A glTF/glb model loaded into GPU buffers. Loading flattens the node
    // hierarchy into a flat list of world-transformed primitives.
    class ELIXIR_API Model
    {
      public:
        static Ref<Model> Load(const GraphicsContext* context, const std::filesystem::path& path);

        [[nodiscard]] const std::vector<SModelPrimitive>& GetPrimitives() const { return m_Primitives; }

      private:
        std::vector<SModelPrimitive> m_Primitives;
    };
}
