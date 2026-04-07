#pragma once

#include <filesystem>

namespace Elixir
{
    class Shader;
    struct SShaderModuleCreateInfo;
    enum class EShaderStage : uint8_t;

    class ELIXIR_API ShaderBackend
    {
      public:
        virtual ~ShaderBackend() = default;

        virtual SShaderModuleCreateInfo LoadModule(
            const std::filesystem::path& filepath,
            EShaderStage stage
        ) const = 0;
    };
}