#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/Texture.h>
#include <Engine/Graphics/Material/Material.h>

#include <filesystem>
#include <mutex>
#include <optional>

namespace Elixir
{
    struct SMeshMaterialSlot;

    struct SModelVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec4 Tangent; // xyz = tangent, w = handedness (bitangent sign)
        glm::vec2 TexCoord;
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
        [[nodiscard]] const std::vector<Ref<MaterialInstance>>& GetMaterials() const { return m_Materials; }
        [[nodiscard]] const std::filesystem::path& GetPath() const { return m_Path; }
        [[nodiscard]] const std::filesystem::path& GetSourcePath() const { return m_SourcePath; }
        [[nodiscard]] const std::filesystem::path& GetMaterialAsset(uint32_t slot) const
        {
            static const std::filesystem::path empty;
            return slot < m_MaterialAssets.size() ? m_MaterialAssets[slot] : empty;
        }
        bool SetMaterialAsset(uint32_t slot, const std::filesystem::path& material);
        bool SaveAsset() const;

        // Call after editing a material instance so the renderer repacks the GPU
        // material buffer on the next frame.
        void MarkMaterialsDirty() { m_MaterialsDirty = true; }
        [[nodiscard]] bool ConsumeMaterialsDirty()
        {
            const bool dirty = m_MaterialsDirty;
            m_MaterialsDirty = false;
            return dirty;
        }

        // Rendering runs on a separate thread from UI/editing, so material edits
        // (main thread) race the renderer's material-buffer pack (render thread).
        // Lock this around both sides.
        [[nodiscard]] std::mutex& MaterialsMutex() const { return m_MaterialsMutex; }

      private:
        // Maps a saved slot entry onto a slot of the freshly imported source, or
        // nullopt when the source no longer has it.
        [[nodiscard]] std::optional<uint32_t> ResolveMaterialSlot(const SMeshMaterialSlot& entry) const;

        std::vector<SModelPrimitive> m_Primitives;
        std::vector<Ref<MaterialInstance>> m_Materials;
        std::filesystem::path m_Path;
        std::filesystem::path m_SourcePath;
        std::vector<std::filesystem::path> m_MaterialAssets;
        bool m_MaterialsDirty = false;
        mutable std::mutex m_MaterialsMutex;
    };
}
