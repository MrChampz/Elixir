#pragma once

#ifdef EE_PLATFORM_WINDOWS

#include <Engine/Graphics/Shader/ShaderModule.h>
#include <Graphics/D3D12/D3D12Utils.h>

namespace Elixir::D3D12
{
    class ELIXIR_API D3D12ShaderModule final : public ShaderModule
    {
      public:
        D3D12ShaderModule(
            const GraphicsContext* context,
            EShaderStage stage,
            const std::string& entrypoint,
            const std::vector<Byte>& bytecode,
            const std::filesystem::path& path = ""
        );

        D3D12_SHADER_BYTECODE GetBytecode() const;

      private:
        std::vector<Byte> m_Bytecode;
    };
}

#endif
