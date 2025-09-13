#pragma once

#include <Engine/Graphics/Shader/ShaderParameter.h>

namespace Elixir
{
    enum class EShaderStage : uint8_t
    {
        Vertex = 0,
		Hull,
		Domain,
		Geometry,
		Pixel,
		Compute,
        Count
    };

    GENERATE_ENUM_CLASS_OPERATORS(EShaderStage)

    class ELIXIR_API ShaderModule
    {
      public:
        virtual ~ShaderModule() = default;

        void AddResource(const ShaderResource* resource);
        void AddConstantBuffer(const ShaderConstantBuffer* buffer);
        void AddPushConstant(const ShaderPushConstant* constant);

        [[nodiscard]] EShaderStage GetStage() const { return m_Stage; }

        static Ref<ShaderModule> Create(
            const GraphicsContext* context,
            EShaderStage stage,
            const std::string& entrypoint,
            const std::vector<Byte>& bytecode,
            const std::filesystem::path& path = ""
        );

      protected:
        explicit ShaderModule(
            const GraphicsContext* context,
            EShaderStage stage,
            const std::string& entrypoint,
            const std::filesystem::path& path = ""
        );

        std::filesystem::path m_Path;
        std::string m_Entrypoint;
        EShaderStage m_Stage;

        // Shader resources declarations
        std::vector<const ShaderResource*> m_Resources;
        std::vector<const ShaderConstantBuffer*> m_ConstantBuffers;
        std::vector<const ShaderPushConstant*> m_PushConstants;

        const GraphicsContext* m_GraphicsContext;
    };
}
