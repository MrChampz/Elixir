#pragma once

#include <Engine/Graphics/Buffer.h>
#include <Engine/Graphics/Texture.h>
#include <Engine/Graphics/Material/Material.h>
#include <Engine/Graphics/Material/MaterialRenderProxy.h>

#include <filesystem>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>

namespace Elixir
{
    class MeshSceneProxy;
    struct SMeshSceneLifetime;
    struct SMeshMaterialSlot;

    // Stable identity for renderer-owned model state. IDs may be recycled after a
    // Model is destroyed, while Generation prevents a new model from aliasing stale
    // GPU caches that still await render-thread retirement.
    struct SModelRenderHandle
    {
        static constexpr uint64_t INVALID_ID = std::numeric_limits<uint64_t>::max();

        uint64_t Id = INVALID_ID;
        uint32_t Generation = 0;

        [[nodiscard]] bool IsValid() const { return Id != INVALID_ID; }
        bool operator==(const SModelRenderHandle&) const = default;
    };

    struct SModelRenderHandleHash
    {
        size_t operator()(const SModelRenderHandle& handle) const
        {
            size_t hash = std::hash<uint64_t>{}(handle.Id);
            hash ^= std::hash<uint32_t>{}(handle.Generation)
                + 0x9e3779b9u + (hash << 6) + (hash >> 2);
            return hash;
        }
    };

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
        struct SBounds
        {
            glm::vec3 Min{ 0.0f };
            glm::vec3 Max{ 0.0f };
        };

        Ref<VertexBuffer> Vertices;
        Ref<IndexBuffer> Indices;
        uint32_t IndexCount = 0;
        glm::mat4 Transform{ 1.0f };
        SBounds LocalBounds;
        uint32_t MaterialIndex = 0;
    };

    // A glTF/glb model loaded into GPU buffers. Loading flattens the node
    // hierarchy into a flat list of world-transformed primitives.
    class ELIXIR_API Model
    {
      public:
        using MaterialRenderProxyList = std::vector<Ref<const MaterialRenderProxy>>;

        Model();
        ~Model();
        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;
        Model(Model&&) = delete;
        Model& operator=(Model&&) = delete;

        static Ref<Model> Load(const GraphicsContext* context, const std::filesystem::path& path);

        [[nodiscard]] const std::vector<SModelPrimitive>& GetPrimitives() const { return m_Primitives; }
        [[nodiscard]] const std::vector<Ref<MaterialInstance>>& GetMaterials() const { return m_Materials; }
        [[nodiscard]] const std::filesystem::path& GetPath() const { return m_Path; }
        [[nodiscard]] const std::filesystem::path& GetSourcePath() const { return m_SourcePath; }
        [[nodiscard]] SModelRenderHandle GetRenderHandle() const { return m_RenderHandle; }
        [[nodiscard]] Ref<const MeshSceneProxy> CreateSceneProxy() const;
        [[nodiscard]] const std::filesystem::path& GetMaterialAsset(uint32_t slot) const
        {
            static const std::filesystem::path empty;
            return slot < m_MaterialAssets.size() ? m_MaterialAssets[slot] : empty;
        }
        bool SetMaterialAsset(uint32_t slot, const std::filesystem::path& material);
        bool SaveAsset() const;

        // Snapshot the authoring instances and publish them atomically. Renderers only
        // consume this immutable list; they never lock or read the live instances.
        void PublishMaterialRenderProxies();
        [[nodiscard]] Ref<const MaterialRenderProxyList> GetMaterialRenderProxies() const
        {
            return std::atomic_load_explicit(
                &m_MaterialRenderProxies, std::memory_order_acquire);
        }

        // Authoring data is edited on the main thread. Hold this while changing or
        // serialising MaterialInstance objects, then publish a new render snapshot.
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
        mutable std::mutex m_MaterialsMutex;
        Ref<const MaterialRenderProxyList> m_MaterialRenderProxies;
        Ref<const std::vector<SModelPrimitive>> m_RenderPrimitives;
        Ref<SMeshSceneLifetime> m_RenderLifetime;
        SModelRenderHandle m_RenderHandle;
    };
}
