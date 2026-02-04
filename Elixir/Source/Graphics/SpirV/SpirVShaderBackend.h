                        #pragma once

#include "spirv_cross.hpp"

#include <Engine/Graphics/Shader/ShaderBackend.h>

namespace spirv_cross
{
	class Compiler;
	struct ShaderResources;
	struct SPIRType;
}

namespace spv
{
    enum Dim;
}

namespace Elixir::SpirV
{
    class ELIXIR_API SpirVShaderBackend final : public ShaderBackend
    {
      public:
        SpirVShaderBackend() = default;

        SShaderModuleCreateInfo LoadModule(
            const std::filesystem::path& filepath,
            EShaderStage stage
        ) const override;

      private:
        static SShaderModuleCreateInfo Load(
            const std::vector<uint32_t>& spirv,
            const std::filesystem::path& filepath
        );

        static std::string GetEntrypoint(const spirv_cross::Compiler& spirv);

        static std::vector<ShaderResource> GetResources(
            const spirv_cross::Compiler& spirv,
            spirv_cross::ShaderResources& resources
        );

        static std::vector<ShaderConstantBuffer> GetConstantBuffers(
            const spirv_cross::Compiler& spirv,
            spirv_cross::ShaderResources& resources
        );

        static std::vector<ShaderPushConstant> GetPushConstants(
            const spirv_cross::Compiler& spirv,
            spirv_cross::ShaderResources& resources
        );

        static uint32_t CalculateResourceCount(spirv_cross::SPIRType type);
        static EResourceDimension GetResourceDimension(spv::Dim dimension);

        static Scope<ShaderConstantStruct> ParseConstantStruct(
            const spirv_cross::Compiler& spirv,
            spirv_cross::SPIRType type,
            const std::string& name
        );
    };
}