#include "epch.h"
#include "MeshLoaderRegistry.h"

#include <Engine/Mesh/GeometryPool.h>
#include <Platform/GLTF/GLTFMeshLoader.h>

namespace Elixir
{
    namespace
    {
        const GraphicsContext* s_Context = nullptr;
        Scope<MeshLoader> s_Loader;
        Scope<GeometryPool> s_GeometryPool;
    }

    void MeshLoaderRegistry::Initialize(const GraphicsContext& context)
    {
        if (s_Context == &context) return;
        if (s_Context)
        {
            EE_CORE_ERROR("MeshLoaderRegistry must be shut down before changing graphics contexts.")
            return;
        }
        s_Context = &context;
        s_GeometryPool = CreateScope<GeometryPool>(context);
        if (!s_Loader) s_Loader = CreateScope<GLTFMeshLoader>();
    }

    void MeshLoaderRegistry::Shutdown()
    {
        s_Loader.reset();
        s_GeometryPool.reset();
        s_Context = nullptr;
    }

    bool MeshLoaderRegistry::RegisterLoader(Scope<MeshLoader> loader)
    {
        if (!loader || s_Loader) return false;
        s_Loader = std::move(loader);
        return true;
    }

    bool MeshLoaderRegistry::ReplaceLoader(Scope<MeshLoader> loader)
    {
        if (!loader) return false;
        s_Loader = std::move(loader);
        return true;
    }

    std::optional<Ref<StaticMesh>> MeshLoaderRegistry::LoadStaticMesh(const std::filesystem::path& path)
    {
        if (!s_Context || !s_Loader || !s_GeometryPool)
        {
            EE_CORE_ERROR("MeshLoaderRegistry must be initialized before loading meshes.")
            return std::nullopt;
        }

        auto data = s_Loader->LoadStaticMesh(*s_Context, path);
        if (!data) return std::nullopt;

        auto geometry = s_GeometryPool->Upload(*data);
        if (!geometry.IsValid()) return std::nullopt;

        auto mesh = StaticMesh::Create(
            std::move(data->Name),
            std::move(geometry),
            std::move(data->Sections),
            std::move(data->Materials),
            data->LocalBounds
        );
        if (!mesh) return std::nullopt;

        return mesh;
    }

    std::optional<Ref<SkeletalMesh>> MeshLoaderRegistry::LoadSkeletalMesh(
        const std::filesystem::path& path, Ref<const Skeleton> skeleton)
    {
        if (!s_Context || !s_Loader)
        {
            EE_CORE_ERROR("MeshLoaderRegistry must be initialized before loading meshes.")
            return std::nullopt;
        }

        auto data = s_Loader->LoadSkeletalMesh(
            *s_Context,
            path,
            std::move(skeleton)
        );
        if (!data) return std::nullopt;

        auto mesh = SkeletalMesh::Create(std::move(*data));
        if (!mesh) return std::nullopt;

        return mesh;
    }

    const GeometryPool* MeshLoaderRegistry::GetGeometryPool()
    {
        return s_GeometryPool.get();
    }
}
