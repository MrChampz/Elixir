#pragma once

#ifdef EE_PLATFORM_WINDOWS

#include <Engine/Graphics/Shader/Shader.h>
#include <Graphics/D3D12/D3D12GraphicsContext.h>

namespace Elixir::D3D12
{
    class ELIXIR_API D3D12Shader final : public Shader
    {
      public:
        enum ERootParameter : UINT
        {
            RootCBVTable = 0,
            RootSRVTable,
            RootUAVTable,
            RootSamplerTable,
            RootPushConstants,
            RootParameterCount
        };

        static constexpr UINT PushConstantShaderRegister = 0;
        static constexpr UINT PushConstantRegisterSpace = 1;
        static constexpr UINT MaxPushConstantDWords = 32;
        static constexpr UINT SRVTableSize = 128;
        static constexpr UINT CBVTableSize = 16;
        static constexpr UINT UAVTableSize = 16;
        static constexpr UINT SamplerTableSize = 8;

        D3D12Shader(const GraphicsContext* context, SShaderCreateInfo&& info);
        ~D3D12Shader() override = default;

        void Bind(const Ref<CommandBuffer>& cmd, const Pipeline* pipeline) override;

        void SetPushConstant(const std::string& name, void* data, size_t size) override;
        void SetPushConstant(
            const Ref<CommandBuffer>& cmd,
            const std::string& name,
            void* data,
            size_t size
        ) override;

        void SetConstantBuffer(const std::string& name, void* data, size_t size) override;

        void BindTexture(const std::string& name, const Ref<Texture>& texture) override;
        void BindTextureSet(const std::string& name, const Ref<TextureSet>& set) override;
        void BindSampler(const std::string& name, const Ref<Sampler>& sampler) override;
        void BindStorageBuffer(const std::string& name, const Ref<StorageBuffer>& buffer) override;
        void BindStorageBuffer(const std::string& name, const Ref<DynamicStorageBuffer>& buffer) override;
        void BindConstantBuffer(const std::string& name, const Ref<UniformBuffer>& buffer) override;

        ID3D12RootSignature* GetRootSignature() const { return m_RootSignature.Get(); }

      private:
        void CreateRootSignature();
        void BindDescriptorTables(ID3D12GraphicsCommandList* commandList, bool graphics) const;
        void BindPushConstants(ID3D12GraphicsCommandList* commandList, bool graphics) const;
        void RefreshTextureSets() const;

        void WriteTextureDescriptor(SShaderBinding binding, const Ref<Texture>& texture) const;
        void WriteSamplerDescriptor(SShaderBinding binding, const Ref<Sampler>& sampler) const;
        void WriteConstantBufferDescriptor(SShaderBinding binding, const Ref<UniformBuffer>& buffer) const;
        void WriteStorageBufferDescriptors(SShaderBinding binding, const Buffer* buffer) const;

        const D3D12GraphicsContext* m_D3D12Context = nullptr;
        ComPtr<ID3D12RootSignature> m_RootSignature;
        SD3D12DescriptorAllocation m_SRVTable;
        SD3D12DescriptorAllocation m_CBVTable;
        SD3D12DescriptorAllocation m_UAVTable;
        SD3D12DescriptorAllocation m_SamplerTable;
    };
}

#endif
