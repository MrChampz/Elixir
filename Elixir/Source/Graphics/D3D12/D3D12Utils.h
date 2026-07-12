#pragma once

#ifdef EE_PLATFORM_WINDOWS

#include <Engine/Graphics/BufferLayout.h>
#include <Engine/Graphics/GraphicsTypes.h>
#include <Engine/Graphics/Image.h>
#include <Engine/Graphics/Sampler.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace Elixir::D3D12
{
    using Microsoft::WRL::ComPtr;

    void CheckResult(HRESULT result, const char* message);

    constexpr uint64_t AlignUp(const uint64_t value, const uint64_t alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    namespace Converters
    {
        DXGI_FORMAT GetFormat(EImageFormat format);
        DXGI_FORMAT GetFormat(EDataType type);
        DXGI_FORMAT GetIndexFormat(EIndexType type);

        D3D12_RESOURCE_STATES GetResourceState(EImageLayout layout);
        D3D12_RESOURCE_FLAGS GetResourceFlags(EImageUsage usage);

        D3D12_VIEWPORT GetViewport(const Viewport& viewport);
        D3D12_RECT GetRect(const Rect2D& rect);

        D3D12_PRIMITIVE_TOPOLOGY GetPrimitiveTopology(EPrimitiveTopology topology);
        D3D12_PRIMITIVE_TOPOLOGY_TYPE GetPrimitiveTopologyType(EPrimitiveTopology topology);
        D3D12_FILL_MODE GetFillMode(EPolygonMode mode);
        D3D12_CULL_MODE GetCullMode(ECullMode mode);
        BOOL GetFrontCounterClockwise(EFrontFace face);
        UINT GetSampleCount(ESampleCount count);

        D3D12_BLEND GetBlend(EBlendFactor factor);
        D3D12_BLEND_OP GetBlendOp(EBlendOp op);
        D3D12_COMPARISON_FUNC GetComparisonFunc(ECompareOp op);
        D3D12_STENCIL_OP GetStencilOp(EStencilOp op);
        UINT8 GetColorWriteMask(EColorComponent components);

        D3D12_FILTER GetFilter(const Sampler* sampler);
        D3D12_TEXTURE_ADDRESS_MODE GetAddressMode(ESamplerAddressMode mode);
        D3D12_COMPARISON_FUNC GetSamplerComparisonFunc(ECompareOp op);
        void GetBorderColor(ESamplerBorderColor color, float outColor[4]);

        struct SInputSemantic
        {
            std::string Name;
            UINT Index = 0;
        };

        SInputSemantic GetInputSemantic(
            const BufferBinding& binding,
            const BufferAttribute& attribute
        );
    }
}

#define DX_CHECK_RESULT(x) ::Elixir::D3D12::CheckResult((x), #x)

#endif
