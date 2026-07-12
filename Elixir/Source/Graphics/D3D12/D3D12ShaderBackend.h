#pragma once

#ifdef EE_PLATFORM_WINDOWS

#include <Engine/Graphics/Shader/ShaderBackend.h>

namespace Elixir::D3D12
{
    class ELIXIR_API D3D12ShaderBackend final : public ShaderBackend
    {
      public:
        SShaderModuleCreateInfo LoadModule(
            const std::filesystem::path& filepath,
            EShaderStage stage
        ) const override;

        std::string_view GetModuleFileExtension() const override { return ".dxil"; }
    };
}

#endif
