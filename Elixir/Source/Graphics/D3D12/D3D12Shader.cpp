#include "epch.h"
#include "D3D12Shader.h"

#ifdef EE_PLATFORM_WINDOWS

#include <Graphics/D3D12/D3D12Buffer.h>
#include <Graphics/D3D12/D3D12CommandBuffer.h>
#include <Graphics/D3D12/D3D12Image.h>
#include <Graphics/D3D12/D3D12Sampler.h>
#include <Graphics/D3D12/D3D12TextureSet.h>

namespace Elixir::D3D12
{
    D3D12Shader::D3D12Shader(const GraphicsContext* context, SShaderCreateInfo&& info)
        : Shader(context, std::move(info))
    {
        m_D3D12Context = static_cast<const D3D12GraphicsContext*>(context);
        m_SRVTable = m_D3D12Context->AllocateShaderResourceDescriptor(SRVTableSize);
        m_CBVTable = m_D3D12Context->AllocateShaderResourceDescriptor(CBVTableSize);
        m_UAVTable = m_D3D12Context->AllocateShaderResourceDescriptor(UAVTableSize);
        m_SamplerTable = m_D3D12Context->AllocateSamplerDescriptor(SamplerTableSize);
        CreateRootSignature();
    }

    void D3D12Shader::Bind(const Ref<CommandBuffer>& cmd, const Pipeline* pipeline)
    {
        const auto d3dCmd = std::static_pointer_cast<D3D12CommandBuffer>(cmd);
        auto* list = d3dCmd->GetD3D12CommandList();
        const auto graphics = pipeline->IsGraphics();

        RefreshTextureSets();

        ID3D12DescriptorHeap* heaps[] = {
            m_D3D12Context->GetResourceDescriptorHeap().GetHeap(),
            m_D3D12Context->GetSamplerDescriptorHeap().GetHeap()
        };
        list->SetDescriptorHeaps((UINT)(sizeof(heaps) / sizeof(heaps[0])), heaps);

        BindDescriptorTables(list, graphics);
        BindPushConstants(list, graphics);
    }

    void D3D12Shader::SetPushConstant(const std::string& name, void* data, const size_t size)
    {
        if (const auto binding = GetShaderBinding(name))
            m_PushConstants[*binding] = PushConstantBuffer::Create(m_GraphicsContext, size, data);
    }

    void D3D12Shader::SetPushConstant(
        const Ref<CommandBuffer>& cmd,
        const std::string& name,
        void* data,
        const size_t size
    )
    {
        SetPushConstant(name, data, size);
        if (const auto binding = GetShaderBinding(name))
            cmd->SetPushConstant(m_PushConstants[*binding], this, EShaderStage::All);
    }

    void D3D12Shader::SetConstantBuffer(const std::string& name, void* data, const size_t size)
    {
        auto buffer = UniformBuffer::Create(m_GraphicsContext, size, data);
        BindConstantBuffer(name, buffer);
    }

    void D3D12Shader::BindTexture(const std::string& name, const Ref<Texture>& texture)
    {
        if (const auto binding = GetShaderBinding(name))
        {
            m_Textures[*binding] = texture;
            WriteTextureDescriptor(*binding, texture);
        }
    }

    void D3D12Shader::BindTextureSet(const std::string& name, const Ref<TextureSet>& set)
    {
        if (const auto binding = GetShaderBinding(name))
            m_TextureSets[*binding] = set;
    }

    void D3D12Shader::BindSampler(const std::string& name, const Ref<Sampler>& sampler)
    {
        if (const auto binding = GetShaderBinding(name))
        {
            m_Samplers[*binding] = sampler;
            WriteSamplerDescriptor(*binding, sampler);
        }
    }

    void D3D12Shader::BindStorageBuffer(const std::string& name, const Ref<StorageBuffer>& buffer)
    {
        if (const auto binding = GetShaderBinding(name))
        {
            m_StorageBuffers[*binding] = buffer;
            WriteStorageBufferDescriptors(*binding, buffer.get());
        }
    }

    void D3D12Shader::BindStorageBuffer(
        const std::string& name,
        const Ref<DynamicStorageBuffer>& buffer
    )
    {
        if (const auto binding = GetShaderBinding(name))
        {
            m_DynStorageBuffers[*binding] = buffer;
            WriteStorageBufferDescriptors(*binding, buffer.get());
        }
    }

    void D3D12Shader::BindConstantBuffer(const std::string& name, const Ref<UniformBuffer>& buffer)
    {
        if (const auto binding = GetShaderBinding(name))
        {
            m_ConstantBuffers[*binding] = buffer;
            WriteConstantBufferDescriptor(*binding, buffer);
        }
    }

    void D3D12Shader::CreateRootSignature()
    {
        std::array<D3D12_DESCRIPTOR_RANGE, 4> ranges = {};
        ranges[0] = {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
            .NumDescriptors = CBVTableSize,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
        };
        ranges[1] = {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            .NumDescriptors = UINT_MAX,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
        };
        ranges[2] = {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            .NumDescriptors = UAVTableSize,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
        };
        ranges[3] = {
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
            .NumDescriptors = SamplerTableSize,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
        };

        std::array<D3D12_ROOT_PARAMETER, RootParameterCount> params = {};
        params[RootCBVTable].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[RootCBVTable].DescriptorTable.NumDescriptorRanges = 1;
        params[RootCBVTable].DescriptorTable.pDescriptorRanges = &ranges[0];
        params[RootCBVTable].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        params[RootSRVTable].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[RootSRVTable].DescriptorTable.NumDescriptorRanges = 1;
        params[RootSRVTable].DescriptorTable.pDescriptorRanges = &ranges[1];
        params[RootSRVTable].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        params[RootUAVTable].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[RootUAVTable].DescriptorTable.NumDescriptorRanges = 1;
        params[RootUAVTable].DescriptorTable.pDescriptorRanges = &ranges[2];
        params[RootUAVTable].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        params[RootSamplerTable].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[RootSamplerTable].DescriptorTable.NumDescriptorRanges = 1;
        params[RootSamplerTable].DescriptorTable.pDescriptorRanges = &ranges[3];
        params[RootSamplerTable].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        params[RootPushConstants].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[RootPushConstants].Constants.ShaderRegister = PushConstantShaderRegister;
        params[RootPushConstants].Constants.RegisterSpace = PushConstantRegisterSpace;
        params[RootPushConstants].Constants.Num32BitValues = MaxPushConstantDWords;
        params[RootPushConstants].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC desc = {};
        desc.NumParameters = (UINT)params.size();
        desc.pParameters = params.data();
        desc.NumStaticSamplers = 0;
        desc.pStaticSamplers = nullptr;
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> blob;
        ComPtr<ID3DBlob> error;
        const auto result = D3D12SerializeRootSignature(
            &desc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &blob,
            &error
        );

        if (FAILED(result) && error)
        {
            EE_CORE_ERROR("D3D12 root signature error: {0}", (const char*)error->GetBufferPointer())
        }
        DX_CHECK_RESULT(result);

        DX_CHECK_RESULT(
            m_D3D12Context->GetDevice()->CreateRootSignature(
                0,
                blob->GetBufferPointer(),
                blob->GetBufferSize(),
                IID_PPV_ARGS(&m_RootSignature)
            )
        );
    }

    void D3D12Shader::BindDescriptorTables(
        ID3D12GraphicsCommandList* commandList,
        const bool graphics
    ) const
    {
        if (graphics)
        {
            commandList->SetGraphicsRootDescriptorTable(
                RootCBVTable,
                m_CBVTable.GPU
            );
            commandList->SetGraphicsRootDescriptorTable(
                RootSRVTable,
                m_SRVTable.GPU
            );
            commandList->SetGraphicsRootDescriptorTable(
                RootUAVTable,
                m_UAVTable.GPU
            );
            commandList->SetGraphicsRootDescriptorTable(
                RootSamplerTable,
                m_SamplerTable.GPU
            );
            return;
        }

        commandList->SetComputeRootDescriptorTable(
            RootCBVTable,
            m_CBVTable.GPU
        );
        commandList->SetComputeRootDescriptorTable(
            RootSRVTable,
            m_SRVTable.GPU
        );
        commandList->SetComputeRootDescriptorTable(
            RootUAVTable,
            m_UAVTable.GPU
        );
        commandList->SetComputeRootDescriptorTable(
            RootSamplerTable,
            m_SamplerTable.GPU
        );
    }

    void D3D12Shader::RefreshTextureSets() const
    {
        for (const auto& set : m_TextureSets | std::views::values)
        {
            const auto d3dSet = std::dynamic_pointer_cast<D3D12TextureSet>(set);
            EE_CORE_ASSERT(d3dSet, "TextureSet is not a D3D12 texture set!")

            for (const auto& [index, texture] : d3dSet->GetTexturesByIndex())
            {
                EE_CORE_ASSERT(index < SRVTableSize, "D3D12 shader SRV table exhausted!")
                WriteTextureDescriptor({.Set = 0, .Binding = index}, texture);
            }
        }
    }

    void D3D12Shader::BindPushConstants(
        ID3D12GraphicsCommandList* commandList,
        const bool graphics
    ) const
    {
        for (const auto& buffer : m_PushConstants | std::views::values)
        {
            const auto dwords = std::min<UINT>(
                MaxPushConstantDWords,
                (buffer->GetSize() + sizeof(uint32_t) - 1) / sizeof(uint32_t)
            );
            if (graphics)
            {
                commandList->SetGraphicsRoot32BitConstants(
                    RootPushConstants,
                    dwords,
                    buffer->GetBuffer().Data,
                    0
                );
            }
            else
            {
                commandList->SetComputeRoot32BitConstants(
                    RootPushConstants,
                    dwords,
                    buffer->GetBuffer().Data,
                    0
                );
            }
        }
    }

    void D3D12Shader::WriteTextureDescriptor(
        const SShaderBinding binding,
        const Ref<Texture>& texture
    ) const
    {
        EE_CORE_ASSERT(binding.Binding < SRVTableSize, "D3D12 shader SRV table exhausted!")
        const auto cpu = m_D3D12Context->GetResourceCPUHandle(
            m_SRVTable.Index + binding.Binding
        );

        if (const auto texture2D = dynamic_cast<const D3D12BaseImage<Texture2D>*>(texture.get()))
            texture2D->CreateShaderResourceView(cpu);
        else if (const auto texture1D = dynamic_cast<const D3D12BaseImage<Texture>*>(texture.get()))
            texture1D->CreateShaderResourceView(cpu);
        else if (const auto texture3D = dynamic_cast<const D3D12BaseImage<Texture3D>*>(texture.get()))
            texture3D->CreateShaderResourceView(cpu);
    }

    void D3D12Shader::WriteSamplerDescriptor(
        const SShaderBinding binding,
        const Ref<Sampler>& sampler
    ) const
    {
        const auto d3dSampler = std::dynamic_pointer_cast<D3D12Sampler>(sampler);
        EE_CORE_ASSERT(d3dSampler, "Sampler is not a D3D12 sampler!")
        // Vulkan's combined-image binding is metadata only; current HLSL samplers use s0.
        d3dSampler->CreateSampler(m_D3D12Context->GetSamplerCPUHandle(m_SamplerTable.Index));
    }

    void D3D12Shader::WriteConstantBufferDescriptor(
        const SShaderBinding binding,
        const Ref<UniformBuffer>& buffer
    ) const
    {
        const auto d3dBuffer = std::dynamic_pointer_cast<D3D12UniformBuffer>(buffer);
        EE_CORE_ASSERT(d3dBuffer, "UniformBuffer is not a D3D12 buffer!")
        d3dBuffer->CreateConstantBufferView(
            m_D3D12Context->GetResourceCPUHandle(m_CBVTable.Index + binding.Binding)
        );
    }

    void D3D12Shader::WriteStorageBufferDescriptors(
        const SShaderBinding binding,
        const Buffer* buffer
    ) const
    {
        const auto index = binding.Binding;
        EE_CORE_ASSERT(index < SRVTableSize, "D3D12 shader SRV table exhausted!")
        EE_CORE_ASSERT(index < UAVTableSize, "D3D12 shader UAV table exhausted!")
        const auto srv = m_D3D12Context->GetResourceCPUHandle(m_SRVTable.Index + index);
        const auto uav = m_D3D12Context->GetResourceCPUHandle(m_UAVTable.Index + index);
        const auto reflected = m_Resources.StorageBuffers.find(binding);
        const auto stride = reflected == m_Resources.StorageBuffers.end()
            ? 0u
            : reflected->second.GetSize();

        if (const auto staticBuffer = dynamic_cast<const D3D12BaseBuffer<StorageBuffer>*>(buffer))
        {
            staticBuffer->CreateShaderResourceView(srv, stride);
            staticBuffer->CreateUnorderedAccessView(uav, stride);
            return;
        }

        if (const auto dynamicBuffer = dynamic_cast<const D3D12DynamicBuffer<DynamicStorageBuffer>*>(buffer))
        {
            dynamicBuffer->CreateShaderResourceView(srv, stride);
        }
    }
}

#endif
