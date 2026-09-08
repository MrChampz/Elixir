#include "epch.h"
#include "StaticMeshLoaderRegistry.h"

#include <Engine/Graphics/GraphicsContext.h>
#include <Platform/GLTF/GLTFStaticMeshLoader.h>

namespace Elixir
{
    namespace
    {
        const GraphicsContext* s_GraphicsContext;
        std::unordered_map<EStaticMeshFormat, Scope<StaticMeshLoader>> s_Loaders;

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
        s_Loaders.clear();
        RegisterLoader(CreateScope<GLTFStaticMeshLoader>());
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

        return loader->second->Load({
            .GraphicsContext = s_GraphicsContext,
            .Path = path
        });
    }

    std::optional<EStaticMeshFormat> StaticMeshLoaderRegistry::InferFormat(
        const std::filesystem::path& path
    )
    {
        std::string extension = path.extension().string();
        std::ranges::transform(extension, extension.begin(), [](const auto character)
        {
            return (char)std::tolower(character);
        });

        if (extension == ".gltf" || extension == ".glb") return EStaticMeshFormat::GLTF;
        return std::nullopt;
    }
}