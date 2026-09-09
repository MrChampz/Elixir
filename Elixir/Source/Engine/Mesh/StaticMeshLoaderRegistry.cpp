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

        SStaticMeshLoadResult MakeError(const std::string& message)
        {
            EE_CORE_ERROR("{}", message)
            return {
                .Diagnostics = {{ EStaticMeshLoadDiagnosticSeverity::Error, message }}
            };
        }
    }

    void StaticMeshLoaderRegistry::Initialize(const GraphicsContext& context)
    {
        if (s_GraphicsContext == &context) return;

        s_GraphicsContext = &context;
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

    SStaticMeshLoadResult StaticMeshLoaderRegistry::Load(const std::filesystem::path& path)
    {
        if (!s_GraphicsContext)
            return MakeError("StaticMeshLoaderRegistry must be initialized before loading meshes.");

        if (!s_Loader)
            return MakeError("No static mesh loader is registered.");

        return s_Loader->Load({
            .GraphicsContext = s_GraphicsContext,
            .Path = path
        });
    }
}