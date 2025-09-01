#pragma once

#include <Engine/Graphics/Texture.h>
#include <Engine/Graphics/Sampler.h>
#include <Engine/Graphics/Shader/ShaderModule.h>

namespace Elixir
{
    struct SShaderCreateInfo
    {
        std::string Name;
        Scope<SShaderModuleCreateInfo> Vertex;
        Scope<SShaderModuleCreateInfo> Pixel;
        Scope<SShaderModuleCreateInfo> Compute;
    };

    struct SShaderBinding
    {
        uint32_t Set;
        uint32_t Binding;

        bool operator==(const SShaderBinding& other) const noexcept
        {
            return Set == other.Set && Binding == other.Binding;
        }
    };

    class ELIXIR_API Shader
    {
      public:
        virtual ~Shader() = default;

        virtual void SetPushConstant(const std::string& name, void* data, size_t size);

        virtual void SetConstantBuffer(const std::string& name, void* data, size_t size);

        virtual void BindTexture(const std::string& name, const Ref<Texture>& texture) = 0;

        [[nodiscard]] virtual Ref<Texture> GetTexture(const std::string& name) const;
        [[nodiscard]] virtual Ref<Texture> GetTexture(SShaderBinding binding) const;

        [[nodiscard]] const std::string& GetName() const { return m_Name; }
        [[nodiscard]] const Ref<ShaderModule>& GetModule(EShaderStage stage) const;

        static Ref<Shader> Create(
            const GraphicsContext* context,
            const SShaderCreateInfo& info
        );

      protected:
        explicit Shader(const GraphicsContext* context, SShaderCreateInfo& info);

        void CreateBindingTable();
        void CreateBindingLookup();

        void SaveShaderBindingToLookup(const std::string& name, SShaderBinding binding);
        const SShaderBinding* GetShaderBinding(const std::string& name) const;

        std::string m_Name;
        std::array<Ref<ShaderModule>, EShaderStage> m_Modules;

        // Shader resources declarations
        SShaderResources m_Resources;

        // Shader resources
        std::unordered_map<std::string, SShaderBinding> m_BindingLookup;
        std::unordered_map<SShaderBinding, Ref<Texture>> m_Textures;
        std::unordered_map<SShaderBinding, Ref<Sampler>> m_Samplers;
        // TODO: Rename to ConstantBuffer
        std::unordered_map<SShaderBinding, Ref<UniformBuffer>> m_ConstantBuffers;
        //std::unordered_map<std::string, Ref<PushConstantBuffer>> m_PushConstants;

        const GraphicsContext* m_GraphicsContext;
    };
}