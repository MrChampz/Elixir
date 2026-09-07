#include "epch.h"
#include "StaticMeshLoaderRegistry.h"

#include <Engine/Graphics/GraphicsContext.h>
#include <Platform/FastGltf/GltfStaticMeshLoader.h>

namespace Elixir
{
    namespace
    {
        const GraphicsContext* s_GraphicsContext = nullptr;
        std::unordered_map<EStaticMeshFormat, Scope<StaticMeshLoader>> s_Loaders;

        SStaticMeshLoadResult MakeError(const std::string_view message)
        {
            EE_CORE_ERROR("{}", message)
            return { .Diagnostics = {{ EStaticMeshLoadDiagnosticSeverity::Error, std::string(message) }} };
        }
    }

    void StaticMeshLoaderRegistry::Initialize(const GraphicsContext* context)
    {
        EE_CORE_ASSERT(context, "StaticMeshLoaderRegistry requires a graphics context")
        if (s_GraphicsContext == context) return;

        s_GraphicsContext = context;
        s_Loaders.clear();
        RegisterLoader(CreateScope<GltfStaticMeshLoader>());
    }

    void StaticMeshLoaderRegistry::Shutdown()
    {
        s_Loaders.clear();
        s_GraphicsContext = nullptr;
    }

    bool StaticMeshLoaderRegistry::RegisterLoader(Scope<StaticMeshLoader> loader)
    {
        if (!loader) return false;

        const EStaticMeshFormat format = loader->GetFormat();
        if (s_Loaders.contains(format)) return false;

        s_Loaders.emplace(format, std::move(loader));
        return true;
    }

    bool StaticMeshLoaderRegistry::ReplaceLoader(Scope<StaticMeshLoader> loader)
    {
        if (!loader) return false;

        s_Loaders[loader->GetFormat()] = std::move(loader);
        return true;
    }

    SStaticMeshLoadResult StaticMeshLoaderRegistry::Load(const std::filesystem::path& path)
    {
        if (!s_GraphicsContext)
            return MakeError("StaticMeshLoaderRegistry must be initialized before loading meshes.");

        const auto format = InferFormat(path);
        if (!format)
            return MakeError(std::format("Unsupported static mesh format: {}.", path.string()));

        const auto loader = s_Loaders.find(*format);
        if (loader == s_Loaders.end())
            return MakeError(std::format("No static mesh loader is registered for: {}.", path.string()));

        return loader->second->Load({ .GraphicsContext = s_GraphicsContext, .Path = path });
    }

    std::optional<EStaticMeshFormat> StaticMeshLoaderRegistry::InferFormat(
        const std::filesystem::path& path
    )
    {
        std::string extension = path.extension().string();
        std::ranges::transform(extension, extension.begin(), [](const unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });

        if (extension == ".gltf" || extension == ".glb") return EStaticMeshFormat::Gltf;
        return std::nullopt;
    }
}
