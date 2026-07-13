#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/Texture.h>

namespace Elixir
{
    struct SModelVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec4 Tangent; // xyz = tangent, w = handedness (bitangent sign)
        glm::vec2 TexCoord;
    };

    // A flattened glTF metallic-roughness material. Textures are already loaded
    // in the correct colour space (base-color/emissive sRGB, the rest linear).
    struct SModelMaterial
    {
        glm::vec4 BaseColorFactor{ 1.0f };
        glm::vec3 EmissiveFactor{ 0.0f };
        float MetallicFactor = 1.0f;
        float RoughnessFactor = 1.0f;
        float OcclusionStrength = 1.0f;
        float NormalScale = 1.0f;
        float AlphaCutoff = 0.5f;
        int AlphaMode = 0; // 0 = opaque, 1 = mask, 2 = blend

        // KHR_materials_clearcoat: a smooth glossy coat over the base layer.
        float ClearcoatFactor = 0.0f;
        float ClearcoatRoughness = 0.0f;

        // KHR_materials_specular: scales/tints the dielectric specular reflection.
        float SpecularFactor = 1.0f;
        glm::vec3 SpecularColorFactor{ 1.0f };

        Ref<Texture> BaseColorTexture;
        Ref<Texture> MetallicRoughnessTexture;
        Ref<Texture> NormalTexture;
        Ref<Texture> EmissiveTexture;
        Ref<Texture> OcclusionTexture;
        Ref<Texture> SpecularTexture;      // alpha = specular strength (specularFactor)
        Ref<Texture> SpecularColorTexture; // rgb = specular color

        // KHR_texture_transform per texture: xy = uv scale, zw = uv offset.
        glm::vec4 BaseColorTransform{ 1.0f, 1.0f, 0.0f, 0.0f };
        glm::vec4 MetallicRoughnessTransform{ 1.0f, 1.0f, 0.0f, 0.0f };
        glm::vec4 NormalTransform{ 1.0f, 1.0f, 0.0f, 0.0f };
        glm::vec4 EmissiveTransform{ 1.0f, 1.0f, 0.0f, 0.0f };
        glm::vec4 OcclusionTransform{ 1.0f, 1.0f, 0.0f, 0.0f };
    };

    // One drawable chunk: a vertex/index buffer pair with the world transform of
    // its node and an index into the model's material table.
    struct SModelPrimitive
    {
        Ref<VertexBuffer> Vertices;
        Ref<IndexBuffer> Indices;
        uint32_t IndexCount = 0;
        glm::mat4 Transform{ 1.0f };
        uint32_t MaterialIndex = 0;
    };

    // A glTF/glb model loaded into GPU buffers. Loading flattens the node
    // hierarchy into a flat list of world-transformed primitives.
    class ELIXIR_API Model
    {
      public:
        static Ref<Model> Load(const GraphicsContext* context, const std::filesystem::path& path);

        [[nodiscard]] const std::vector<SModelPrimitive>& GetPrimitives() const { return m_Primitives; }
        [[nodiscard]] const std::vector<SModelMaterial>& GetMaterials() const { return m_Materials; }

      private:
        std::vector<SModelPrimitive> m_Primitives;
        std::vector<SModelMaterial> m_Materials;
    };
}
