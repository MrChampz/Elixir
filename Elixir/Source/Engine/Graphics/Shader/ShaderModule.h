#pragma once

#include <Engine/Graphics/Shader/ShaderParameter.h>

namespace Elixir
{
    enum class EShaderStage : uint8_t
    {
        Vertex					= 0x00000001,
		Hull		            = 0x00000002,
		Domain	                = 0x00000004,
		Geometry				= 0x00000008,
		Pixel				    = 0x00000010,
		Compute					= 0x00000020
    };

    GENERATE_ENUM_CLASS_OPERATORS(EShaderStage)

    template <typename T>
    typedef std::vector<std::vector<T>> BindingTable;

    struct SShaderResources
    {
        BindingTable<ShaderResource> Resources;
        BindingTable<ShaderConstantBuffer> ConstantBuffers;
        BindingTable<ShaderPushConstant> PushConstants;
    };

    struct SShaderModuleCreateInfo
    {
        std::string Path;
        std::string Entrypoint;
        SShaderResources Resources;
        std::vector<uint32_t> Bytecode;
    };

    class ELIXIR_API ShaderModule
    {
      public:
        virtual ~ShaderModule() = default;

        [[nodiscard]] EShaderStage GetStage() const { return m_Stage; }

        static Ref<ShaderModule> Create(
            const GraphicsContext* context,
            EShaderStage stage,
            const SShaderModuleCreateInfo& info
        );

      protected:
        explicit ShaderModule(
            const GraphicsContext* context,
            EShaderStage stage,
            const SShaderModuleCreateInfo& info
        );

        std::string m_Path;
        std::string m_Entrypoint;
        EShaderStage m_Stage;

        // Shader resources declarations
        std::vector<ShaderResource*> m_Resources;
        std::vector<ShaderConstantBuffer*> m_ConstantBuffers;
        std::vector<ShaderPushConstant*> m_PushConstants;

        const GraphicsContext* m_GraphicsContext;
    };
}
