#include "epch.h"
#include "D3D12ShaderModule.h"

#ifdef EE_PLATFORM_WINDOWS

namespace Elixir::D3D12
{
    D3D12ShaderModule::D3D12ShaderModule(
        const GraphicsContext* context,
        const EShaderStage stage,
        const std::string& entrypoint,
        const std::vector<Byte>& bytecode,
        const std::filesystem::path& path
    ) : ShaderModule(context, stage, entrypoint, path), m_Bytecode(bytecode)
    {
    }

    D3D12_SHADER_BYTECODE D3D12ShaderModule::GetBytecode() const
    {
        return {
            .pShaderBytecode = m_Bytecode.data(),
            .BytecodeLength = m_Bytecode.size(),
        };
    }
}

#endif
