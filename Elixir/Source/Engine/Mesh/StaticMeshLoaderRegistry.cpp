#include "epch.h"
#include "StaticMeshLoaderRegistry.h"

#include <Engine/Graphics/GraphicsContext.h>
#include <Platform/GLTF/GLTFStaticMeshLoader.h>

namespace Elixir
{
    namespace
    {
        const GraphicsContext* s_GraphicsContext;
        Scope<StaticMeshLoader> s_Loader;
        Scope<GeometryPool> s_GeometryPool;

        std::optional<Ref<StaticMesh>> MakeError(const std::string_view message)
        {
            EE_CORE_ERROR("{}", message)
            return std::nullopt;
        }
    }

    void StaticMeshLoaderRegistry::Initialize(const GraphicsContext& context)
    {
        if (s_GraphicsContext == &context) return;

        s_GraphicsContext = &context;
        s_GeometryPool = CreateScope<GeometryPool>(context);
        s_Loader.reset();
        RegisterLoader(CreateScope<GLTFStaticMeshLoader>());
    }

    void StaticMeshLoaderRegistry::Shutdown()
    {
        s_Loader.reset();
        s_GraphicsContext = nullptr;
    }

    bool StaticMeshLoaderRegistry::RegisterLoader(Scope<StaticMeshLoader> loader)
    {
        if (!loader || s_Loader) return false;

        s_Loader = std::move(loader);
        return true;
    }

    bool StaticMeshLoaderRegistry::ReplaceLoader(Scope<StaticMeshLoader> loader)
    {
        if (!loader) return false;

        s_Loader = std::move(loader);
        return true;
    }

    std::optional<Ref<StaticMesh>> StaticMeshLoaderRegistry::Load(
        const std::filesystem::path& path
    )
    {
        if (!s_GraphicsContext)
            return MakeError("StaticMeshLoaderRegistry must be initialized before loading meshes.");

        if (!s_Loader)
            return MakeError("No static mesh loader is registered.");

        const auto data = s_Loader->Load(path);
        if (!data) return std::nullopt;

        auto geometry = s_GeometryPool->Upload(*data);
        if (!geometry.IsValid())
        {
            EE_CORE_ERROR("Could not upload static mesh geometry '{}'.", data->Name)
            return std::nullopt;
        }

        return StaticMesh::Create(
            std::move(data->Name),
            std::move(geometry),
            std::move(data->Sections),
            data->LocalBounds
        );
    }

    const GeometryPool& StaticMeshLoaderRegistry::GetGeometryPool()
    {
        return *s_GeometryPool;
    }
}
