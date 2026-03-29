#pragma once

#include <Engine/Graphics/Shader/Shader.h>

#include <filesystem>

namespace Elixir
{
    class ELIXIR_API ShaderLoader final
    {
      public:
        explicit ShaderLoader(const GraphicsContext* context);

        Ref<Shader> LoadShader(
            const std::filesystem::path& directory,
            const std::string& name
        ) const;

        Ref<Shader> LoadShader(
            const std::filesystem::path& directory,
            std::span<const std::string_view> filenames,
            const std::string& name
        ) const;

      private:
        SShaderModuleCreateInfo LoadModule(
            const std::filesystem::path& filepath,
            EShaderStage stage
        ) const;

        const GraphicsContext* m_GraphicsContext;
    };
}