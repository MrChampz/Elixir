#include "epch.h"
#include "D3D12ShaderBackend.h"

#ifdef EE_PLATFORM_WINDOWS

#include <Engine/Graphics/Shader/Shader.h>
#include <Graphics/SpirV/SpirVShaderBackend.h>

namespace Elixir::D3D12
{
    namespace
    {
        std::vector<Byte> LoadBinaryFile(const std::filesystem::path& filepath)
        {
            const auto size = std::filesystem::file_size(filepath);
            std::vector<Byte> bytecode(size);

            std::ifstream file(filepath, std::ios::binary);
            if (!file.read(reinterpret_cast<char*>(bytecode.data()), (std::streamsize)size))
            {
                EE_CORE_ASSERT(false, "Failed to read shader bytecode!")
                return {};
            }

            return bytecode;
        }
    }

    SShaderModuleCreateInfo D3D12ShaderBackend::LoadModule(
        const std::filesystem::path& filepath,
        const EShaderStage stage
    ) const
    {
        SShaderModuleCreateInfo module = {};

        auto spirvPath = filepath;
        spirvPath.replace_extension(".spirv");

        if (std::filesystem::exists(spirvPath))
        {
            module = SpirV::SpirVShaderBackend().LoadModule(spirvPath, stage);
        }
        else
        {
            module.Path = filepath;
            module.Entrypoint = "main";
            EE_CORE_WARN("D3D12 shader reflection sidecar not found: {0}", spirvPath.string())
        }

        module.Path = filepath;
        module.Bytecode = LoadBinaryFile(filepath);
        return module;
    }
}

#endif
