#include "epch.h"
#include "ShaderLoader.h"

#include "ShaderBackend.h"

namespace Elixir
{
    std::string GetShaderFileExtension(const std::filesystem::path& filepath)
    {
        const auto str = filepath.filename().string();

        // stage represents the first index: .vs, .ps, .cs...
        const auto stage = str.find(".");
        if (stage == std::string::npos)
            return {};

        // format represents the second index: .spirv, .hlsl (if exists)
        const auto format = str.find(".", stage + 1);

        // if there is format index, get only the interesting part, the stage indicator
        if (format == std::string::npos)
        {
            return str.substr(stage + 1, str.size() - stage);
        }

        // otherwise, return the whole extension
        return str.substr(stage + 1, format - stage - 1);
    }

    std::optional<EShaderStage> GetShaderStage(const std::filesystem::path& filepath)
    {
        const auto extension = GetShaderFileExtension(filepath);

        using pair = std::pair<std::string_view, EShaderStage>;
        static constexpr std::array<pair, static_cast<size_t>(EShaderStage::Count)> lookup {{
            { "vs", EShaderStage::Vertex   },
            { "hs", EShaderStage::Hull     },
            { "ds", EShaderStage::Domain   },
            { "gs", EShaderStage::Geometry },
            { "ps", EShaderStage::Pixel    },
            { "cs", EShaderStage::Compute  },
        }};

        for (auto&& [key, value] : lookup)
            if (extension == key) return value;

        return std::nullopt;
    }

    std::string RemoveFileExtension(const std::filesystem::path& filepath)
    {
        auto newFilepath = filepath.stem();
        while (newFilepath.has_extension())
        {
            newFilepath = newFilepath.stem();
        }

        return newFilepath.string();
    }

    using StagePath = std::pair<EShaderStage, std::filesystem::path>;

    std::vector<StagePath> GetShaderFilesInDirectory(
        const std::filesystem::path& directory,
        std::span<const std::string_view> filenames
    )
    {
        std::vector<StagePath> files;

        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            const auto path = entry.path();
            const auto filename = RemoveFileExtension(path);
            const auto isIncluded = std::ranges::find(filenames, filename) != filenames.end();
            if (entry.is_regular_file() && path.has_extension() && isIncluded)
            {
                if (const auto stage = GetShaderStage(path))
                    files.push_back({ stage.value(), path });
            }
        }

        return files;
    }

    void AddShaderModule(
        const EShaderStage stage,
        SShaderModuleCreateInfo&& module,
        SShaderCreateInfo& shader
    )
    {
        switch (stage)
        {
            case EShaderStage::Vertex:
                shader.Vertex = CreateScope<SShaderModuleCreateInfo>(std::move(module));
                break;
            case EShaderStage::Pixel:
                shader.Pixel = CreateScope<SShaderModuleCreateInfo>(std::move(module));
                break;
            case EShaderStage::Compute:
                shader.Compute = CreateScope<SShaderModuleCreateInfo>(std::move(module));
                break;
            default:
                EE_CORE_ASSERT(false, "Unknown Shader Stage!")
        }
    }

    ShaderLoader::ShaderLoader(const GraphicsContext* context)
        : m_GraphicsContext(context)
    {
        EE_PROFILE_ZONE_SCOPED()
    }

    Ref<Shader> ShaderLoader::LoadShader(
        const std::filesystem::path& directory,
        const std::string& name,
        const EShaderStage stages
    ) const
    {
        return LoadShader(directory, std::array<std::string_view, 1>{ name }, name, stages);
    }

    Ref<Shader> ShaderLoader::LoadShader(
        const std::filesystem::path& directory,
        const std::span<const std::string_view> filenames,
        const std::string& name,
        const EShaderStage stages
    ) const
    {
        if (is_directory(directory))
        {
            try
            {
                const auto& files = GetShaderFilesInDirectory(directory, filenames);
                if (files.empty())
                {
                    EE_CORE_ERROR("No shader files found at path! [Path = {0}]", directory.string())
                    return nullptr;
                }

                SShaderCreateInfo shaderInfo = {};
                shaderInfo.Name = name;

                for (const auto& [stage, path] : files)
                {
                    if (stages & stage)
                    {
                        auto module = LoadModule(path, stage);
                        AddShaderModule(stage, std::move(module), shaderInfo);
                    }
                }

                return Shader::Create(m_GraphicsContext, std::move(shaderInfo));
            }
            catch (const std::filesystem::filesystem_error&)
            {
                EE_CORE_ERROR("Cannot access shader directory! [Path = {0}]", directory.string())
                return nullptr;
            }
            catch (const std::exception& error)
            {
                EE_CORE_ERROR("Error trying to load shader: \"{0}\", [Path = {1}]", error.what(), directory.string())
                return nullptr;
            }
        }

        EE_CORE_ERROR("Shader path is not a directory! [Path = {0}]", directory.string())
        return nullptr;
    }

    SShaderModuleCreateInfo ShaderLoader::LoadModule(
        const std::filesystem::path& filepath,
        const EShaderStage stage
    ) const
    {
        EE_PROFILE_ZONE_SCOPED()
        const auto& backend = m_GraphicsContext->GetShaderBackend();
        return backend->LoadModule(filepath, stage);
    }
}