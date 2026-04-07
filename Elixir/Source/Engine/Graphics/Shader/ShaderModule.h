#pragma once

#include <Engine/Graphics/GraphicsTypes.h>
#include <Engine/Graphics/Shader/ShaderParameter.h>

namespace Elixir
{
    class ELIXIR_API ShaderModule
    {
      public:
        virtual ~ShaderModule() = default;

        void AddResource(const ShaderResource* resource);
        void AddStorageBuffer(const ShaderStorageBuffer* buffer);
        void AddConstantBuffer(const ShaderConstantBuffer* buffer);
        void AddPushConstant(const ShaderPushConstant* constant);

        [[nodiscard]] EShaderStage GetStage() const { return m_Stage; }
        [[nodiscard]] const std::string& GetEntrypoint() const { return m_Entrypoint; }
        [[nodiscard]] const std::filesystem::path& GetPath() const { return m_Path; }

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
        std::vector<const ShaderStorageBuffer*> m_StorageBuffers;
        std::vector<const ShaderConstantBuffer*> m_ConstantBuffers;
        std::vector<const ShaderPushConstant*> m_PushConstants;

        const GraphicsContext* m_GraphicsContext;
    };
}
