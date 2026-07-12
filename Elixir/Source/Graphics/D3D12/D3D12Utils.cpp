#include "epch.h"
#include "D3D12Utils.h"

#ifdef EE_PLATFORM_WINDOWS

namespace Elixir::D3D12
{
    namespace
    {
        const char* GetResultName(const HRESULT result)
        {
            switch (result)
            {
                case E_INVALIDARG: return "E_INVALIDARG";
                case E_OUTOFMEMORY: return "E_OUTOFMEMORY";
                case E_ACCESSDENIED: return "E_ACCESSDENIED";
                case DXGI_ERROR_DEVICE_HUNG: return "DXGI_ERROR_DEVICE_HUNG";
                case DXGI_ERROR_DEVICE_REMOVED: return "DXGI_ERROR_DEVICE_REMOVED";
                case DXGI_ERROR_DEVICE_RESET: return "DXGI_ERROR_DEVICE_RESET";
                case DXGI_ERROR_DRIVER_INTERNAL_ERROR: return "DXGI_ERROR_DRIVER_INTERNAL_ERROR";
                case DXGI_ERROR_INVALID_CALL: return "DXGI_ERROR_INVALID_CALL";
                case DXGI_ERROR_NOT_CURRENTLY_AVAILABLE: return "DXGI_ERROR_NOT_CURRENTLY_AVAILABLE";
                case DXGI_ERROR_UNSUPPORTED: return "DXGI_ERROR_UNSUPPORTED";
                case DXGI_ERROR_WAS_STILL_DRAWING: return "DXGI_ERROR_WAS_STILL_DRAWING";
                default: return "HRESULT_FAILURE";
            }
        }

        std::string GetResultDescription(const HRESULT result)
        {
            DWORD code = (DWORD)result;
            if (HRESULT_FACILITY(result) == FACILITY_WIN32)
                code = HRESULT_CODE(result);

            char* buffer = nullptr;
            const auto length = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                code,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<char*>(&buffer),
                0,
                nullptr
            );

            if (length == 0 || !buffer)
                return "No system description is available";

            std::string description(buffer, length);
            LocalFree(buffer);
            while (!description.empty() && std::isspace((unsigned char)description.back()))
                description.pop_back();
            return description;
        }
    }

    void CheckResult(const HRESULT result, const char* message)
    {
        if (FAILED(result))
        {
            EE_CORE_ERROR(
                "D3D12 call failed: {0} [HRESULT=0x{1:08X}, {2}: {3}]",
                message,
                (uint32_t)result,
                GetResultName(result),
                GetResultDescription(result)
            )
            DEBUG_BREAK()
        }
    }

    namespace Converters
    {
        DXGI_FORMAT GetFormat(const EImageFormat format)
        {
            switch (format)
            {
                case EImageFormat::Undefined: return DXGI_FORMAT_UNKNOWN;
                case EImageFormat::R8_UNORM: return DXGI_FORMAT_R8_UNORM;
                case EImageFormat::R8_SNORM: return DXGI_FORMAT_R8_SNORM;
                case EImageFormat::R8_UINT: return DXGI_FORMAT_R8_UINT;
                case EImageFormat::R8_SINT: return DXGI_FORMAT_R8_SINT;
                case EImageFormat::R16_SFLOAT: return DXGI_FORMAT_R16_FLOAT;
                case EImageFormat::R32G32B32_UINT: return DXGI_FORMAT_R32G32B32_UINT;
                case EImageFormat::R32G32B32_SINT: return DXGI_FORMAT_R32G32B32_SINT;
                case EImageFormat::R32G32B32_SFLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
                case EImageFormat::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
                case EImageFormat::R8G8B8A8_SNORM: return DXGI_FORMAT_R8G8B8A8_SNORM;
                case EImageFormat::R8G8B8A8_UINT: return DXGI_FORMAT_R8G8B8A8_UINT;
                case EImageFormat::R8G8B8A8_SINT: return DXGI_FORMAT_R8G8B8A8_SINT;
                case EImageFormat::R8G8B8A8_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                case EImageFormat::R16G16B16A16_UNORM: return DXGI_FORMAT_R16G16B16A16_UNORM;
                case EImageFormat::R16G16B16A16_SNORM: return DXGI_FORMAT_R16G16B16A16_SNORM;
                case EImageFormat::R16G16B16A16_UINT: return DXGI_FORMAT_R16G16B16A16_UINT;
                case EImageFormat::R16G16B16A16_SINT: return DXGI_FORMAT_R16G16B16A16_SINT;
                case EImageFormat::R16G16B16A16_SFLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
                case EImageFormat::R32G32B32A32_UINT: return DXGI_FORMAT_R32G32B32A32_UINT;
                case EImageFormat::R32G32B32A32_SINT: return DXGI_FORMAT_R32G32B32A32_SINT;
                case EImageFormat::R32G32B32A32_SFLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
                case EImageFormat::BC1_RGB_UNORM_BLOCK:
                case EImageFormat::BC1_RGBA_UNORM_BLOCK: return DXGI_FORMAT_BC1_UNORM;
                case EImageFormat::BC1_RGB_SRGB_BLOCK:
                case EImageFormat::BC1_RGBA_SRGB_BLOCK: return DXGI_FORMAT_BC1_UNORM_SRGB;
                case EImageFormat::BC2_UNORM_BLOCK: return DXGI_FORMAT_BC2_UNORM;
                case EImageFormat::BC2_SRGB_BLOCK: return DXGI_FORMAT_BC2_UNORM_SRGB;
                case EImageFormat::BC3_UNORM_BLOCK: return DXGI_FORMAT_BC3_UNORM;
                case EImageFormat::BC3_SRGB_BLOCK: return DXGI_FORMAT_BC3_UNORM_SRGB;
                case EImageFormat::BC4_UNORM_BLOCK: return DXGI_FORMAT_BC4_UNORM;
                case EImageFormat::BC4_SNORM_BLOCK: return DXGI_FORMAT_BC4_SNORM;
                case EImageFormat::BC5_UNORM_BLOCK: return DXGI_FORMAT_BC5_UNORM;
                case EImageFormat::BC5_SNORM_BLOCK: return DXGI_FORMAT_BC5_SNORM;
                case EImageFormat::BC6H_UFLOAT_BLOCK: return DXGI_FORMAT_BC6H_UF16;
                case EImageFormat::BC6H_SFLOAT_BLOCK: return DXGI_FORMAT_BC6H_SF16;
                case EImageFormat::BC7_UNORM_BLOCK: return DXGI_FORMAT_BC7_UNORM;
                case EImageFormat::BC7_SRGB_BLOCK: return DXGI_FORMAT_BC7_UNORM_SRGB;
                case EImageFormat::D16_UNORM: return DXGI_FORMAT_D16_UNORM;
                case EImageFormat::D24_UNORM_S8_UINT: return DXGI_FORMAT_D24_UNORM_S8_UINT;
                case EImageFormat::D32_SFLOAT: return DXGI_FORMAT_D32_FLOAT;
                case EImageFormat::D32_SFLOAT_S8_UINT: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
                case EImageFormat::X8_D24_UNORM_PACK32: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
                default:
                    EE_CORE_ASSERT(false, "Unsupported D3D12 image format!")
                    return DXGI_FORMAT_UNKNOWN;
            }
        }

        DXGI_FORMAT GetFormat(const EDataType type)
        {
            switch (type)
            {
                case EDataType::Float: return DXGI_FORMAT_R32_FLOAT;
                case EDataType::Vec2: return DXGI_FORMAT_R32G32_FLOAT;
                case EDataType::Vec3: return DXGI_FORMAT_R32G32B32_FLOAT;
                case EDataType::Vec4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
                case EDataType::Int: return DXGI_FORMAT_R32_SINT;
                case EDataType::IntVec2: return DXGI_FORMAT_R32G32_SINT;
                case EDataType::IntVec3: return DXGI_FORMAT_R32G32B32_SINT;
                case EDataType::IntVec4: return DXGI_FORMAT_R32G32B32A32_SINT;
                case EDataType::UInt: return DXGI_FORMAT_R32_UINT;
                case EDataType::UIntVec2: return DXGI_FORMAT_R32G32_UINT;
                case EDataType::UIntVec3: return DXGI_FORMAT_R32G32B32_UINT;
                case EDataType::UIntVec4: return DXGI_FORMAT_R32G32B32A32_UINT;
                default:
                    EE_CORE_ASSERT(false, "Unsupported D3D12 vertex format!")
                    return DXGI_FORMAT_UNKNOWN;
            }
        }

        DXGI_FORMAT GetIndexFormat(const EIndexType type)
        {
            switch (type)
            {
                case EIndexType::UInt16: return DXGI_FORMAT_R16_UINT;
                case EIndexType::UInt32: return DXGI_FORMAT_R32_UINT;
                default:
                    EE_CORE_ASSERT(false, "Unknown index format!")
                    return DXGI_FORMAT_UNKNOWN;
            }
        }

        D3D12_RESOURCE_STATES GetResourceState(const EImageLayout layout)
        {
            switch (layout)
            {
                case EImageLayout::Undefined: return D3D12_RESOURCE_STATE_COMMON;
                case EImageLayout::General: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                case EImageLayout::ColorAttachment: return D3D12_RESOURCE_STATE_RENDER_TARGET;
                case EImageLayout::DepthAttachment:
                case EImageLayout::StencilAttachment:
                case EImageLayout::DepthStencilAttachment:
                    return D3D12_RESOURCE_STATE_DEPTH_WRITE;
                case EImageLayout::DepthReadOnly:
                case EImageLayout::StencilReadOnly:
                case EImageLayout::DepthStencilReadOnly:
                case EImageLayout::ShaderReadOnly:
                    return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
                case EImageLayout::TransferSrc: return D3D12_RESOURCE_STATE_COPY_SOURCE;
                case EImageLayout::TransferDst: return D3D12_RESOURCE_STATE_COPY_DEST;
                case EImageLayout::PreInitialized: return D3D12_RESOURCE_STATE_COMMON;
                case EImageLayout::PresentSrc: return D3D12_RESOURCE_STATE_PRESENT;
                default:
                    EE_CORE_ASSERT(false, "Unknown image layout!")
                    return D3D12_RESOURCE_STATE_COMMON;
            }
        }

        D3D12_RESOURCE_FLAGS GetResourceFlags(const EImageUsage usage)
        {
            D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

            if (usage & EImageUsage::ColorAttachment)
                flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            if (usage & EImageUsage::DepthStencilAttachment)
                flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            if (usage & EImageUsage::Storage)
                flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            return flags;
        }

        D3D12_VIEWPORT GetViewport(const Viewport& viewport)
        {
            return {
                .TopLeftX = viewport.X,
                .TopLeftY = viewport.Y,
                .Width = viewport.Width,
                .Height = viewport.Height,
                .MinDepth = viewport.MinDepth,
                .MaxDepth = viewport.MaxDepth,
            };
        }

        D3D12_RECT GetRect(const Rect2D& rect)
        {
            return {
                .left = rect.Offset.X,
                .top = rect.Offset.Y,
                .right = rect.Offset.X + (LONG)rect.Extent.Width,
                .bottom = rect.Offset.Y + (LONG)rect.Extent.Height,
            };
        }

        D3D12_PRIMITIVE_TOPOLOGY GetPrimitiveTopology(const EPrimitiveTopology topology)
        {
            switch (topology)
            {
                case EPrimitiveTopology::PointList: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
                case EPrimitiveTopology::LineList: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
                case EPrimitiveTopology::LineStrip: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
                case EPrimitiveTopology::TriangleList: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
                case EPrimitiveTopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
                default:
                    EE_CORE_ASSERT(false, "Unsupported D3D12 primitive topology!")
                    return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
            }
        }

        D3D12_PRIMITIVE_TOPOLOGY_TYPE GetPrimitiveTopologyType(const EPrimitiveTopology topology)
        {
            switch (topology)
            {
                case EPrimitiveTopology::PointList: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
                case EPrimitiveTopology::LineList:
                case EPrimitiveTopology::LineStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
                case EPrimitiveTopology::TriangleList:
                case EPrimitiveTopology::TriangleStrip:
                case EPrimitiveTopology::TriangleFan: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
                case EPrimitiveTopology::PatchList: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
                default:
                    EE_CORE_ASSERT(false, "Unsupported D3D12 primitive topology type!")
                    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
            }
        }

        D3D12_FILL_MODE GetFillMode(const EPolygonMode mode)
        {
            switch (mode)
            {
                case EPolygonMode::Fill: return D3D12_FILL_MODE_SOLID;
                case EPolygonMode::Line: return D3D12_FILL_MODE_WIREFRAME;
                default: return D3D12_FILL_MODE_SOLID;
            }
        }

        D3D12_CULL_MODE GetCullMode(const ECullMode mode)
        {
            if (mode & ECullMode::FrontAndBack)
                return D3D12_CULL_MODE_BACK;
            if (mode & ECullMode::Front)
                return D3D12_CULL_MODE_FRONT;
            if (mode & ECullMode::Back)
                return D3D12_CULL_MODE_BACK;
            return D3D12_CULL_MODE_NONE;
        }

        BOOL GetFrontCounterClockwise(const EFrontFace face)
        {
            return face == EFrontFace::CounterClockwise;
        }

        UINT GetSampleCount(const ESampleCount count)
        {
            switch (count)
            {
                case ESampleCount::_1: return 1;
                case ESampleCount::_2: return 2;
                case ESampleCount::_4: return 4;
                case ESampleCount::_8: return 8;
                case ESampleCount::_16: return 16;
                case ESampleCount::_32: return 32;
                case ESampleCount::_64: return 64;
                default: return 1;
            }
        }

        D3D12_BLEND GetBlend(const EBlendFactor factor)
        {
            switch (factor)
            {
                case EBlendFactor::Zero: return D3D12_BLEND_ZERO;
                case EBlendFactor::One: return D3D12_BLEND_ONE;
                case EBlendFactor::SrcColor: return D3D12_BLEND_SRC_COLOR;
                case EBlendFactor::OneMinusSrcColor: return D3D12_BLEND_INV_SRC_COLOR;
                case EBlendFactor::DstColor: return D3D12_BLEND_DEST_COLOR;
                case EBlendFactor::OneMinusDstColor: return D3D12_BLEND_INV_DEST_COLOR;
                case EBlendFactor::SrcAlpha: return D3D12_BLEND_SRC_ALPHA;
                case EBlendFactor::OneMinusSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
                case EBlendFactor::DstAlpha: return D3D12_BLEND_DEST_ALPHA;
                case EBlendFactor::OneMinusDstAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
                case EBlendFactor::ConstantColor:
                case EBlendFactor::ConstantAlpha: return D3D12_BLEND_BLEND_FACTOR;
                case EBlendFactor::OneMinusConstantColor:
                case EBlendFactor::OneMinusConstantAlpha: return D3D12_BLEND_INV_BLEND_FACTOR;
                case EBlendFactor::SrcAlphaSaturate: return D3D12_BLEND_SRC_ALPHA_SAT;
                case EBlendFactor::Src1Color: return D3D12_BLEND_SRC1_COLOR;
                case EBlendFactor::OneMinusSrc1Color: return D3D12_BLEND_INV_SRC1_COLOR;
                case EBlendFactor::Src1Alpha: return D3D12_BLEND_SRC1_ALPHA;
                case EBlendFactor::OneMinusSrc1Alpha: return D3D12_BLEND_INV_SRC1_ALPHA;
                default: return D3D12_BLEND_ONE;
            }
        }

        D3D12_BLEND_OP GetBlendOp(const EBlendOp op)
        {
            switch (op)
            {
                case EBlendOp::Add: return D3D12_BLEND_OP_ADD;
                case EBlendOp::Subtract: return D3D12_BLEND_OP_SUBTRACT;
                case EBlendOp::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
                case EBlendOp::Min: return D3D12_BLEND_OP_MIN;
                case EBlendOp::Max: return D3D12_BLEND_OP_MAX;
                default: return D3D12_BLEND_OP_ADD;
            }
        }

        D3D12_COMPARISON_FUNC GetComparisonFunc(const ECompareOp op)
        {
            switch (op)
            {
                case ECompareOp::Never: return D3D12_COMPARISON_FUNC_NEVER;
                case ECompareOp::Less: return D3D12_COMPARISON_FUNC_LESS;
                case ECompareOp::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
                case ECompareOp::LessOrEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
                case ECompareOp::Greater: return D3D12_COMPARISON_FUNC_GREATER;
                case ECompareOp::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
                case ECompareOp::GreaterOrEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
                case ECompareOp::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
                default: return D3D12_COMPARISON_FUNC_ALWAYS;
            }
        }

        D3D12_STENCIL_OP GetStencilOp(const EStencilOp op)
        {
            switch (op)
            {
                case EStencilOp::Keep: return D3D12_STENCIL_OP_KEEP;
                case EStencilOp::Zero: return D3D12_STENCIL_OP_ZERO;
                case EStencilOp::Replace: return D3D12_STENCIL_OP_REPLACE;
                case EStencilOp::IncrementAndClamp: return D3D12_STENCIL_OP_INCR_SAT;
                case EStencilOp::DecrementAndClamp: return D3D12_STENCIL_OP_DECR_SAT;
                case EStencilOp::Invert: return D3D12_STENCIL_OP_INVERT;
                case EStencilOp::IncrementAndWrap: return D3D12_STENCIL_OP_INCR;
                case EStencilOp::DecrementAndWrap: return D3D12_STENCIL_OP_DECR;
                default: return D3D12_STENCIL_OP_KEEP;
            }
        }

        UINT8 GetColorWriteMask(const EColorComponent components)
        {
            UINT8 mask = 0;
            if (components & EColorComponent::R) mask |= D3D12_COLOR_WRITE_ENABLE_RED;
            if (components & EColorComponent::G) mask |= D3D12_COLOR_WRITE_ENABLE_GREEN;
            if (components & EColorComponent::B) mask |= D3D12_COLOR_WRITE_ENABLE_BLUE;
            if (components & EColorComponent::A) mask |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
            return mask ? mask : D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        D3D12_FILTER GetFilter(const Sampler* sampler)
        {
            const auto minLinear = sampler->GetMinFilter() == ESamplerFilter::Linear;
            const auto magLinear = sampler->GetMagFilter() == ESamplerFilter::Linear;
            const auto mipLinear = sampler->GetMipmapMode() == ESamplerMipmapMode::Linear;

            if (minLinear && magLinear && mipLinear) return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            if (minLinear && magLinear) return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            if (minLinear) return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
            if (magLinear) return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
            return D3D12_FILTER_MIN_MAG_MIP_POINT;
        }

        D3D12_TEXTURE_ADDRESS_MODE GetAddressMode(const ESamplerAddressMode mode)
        {
            switch (mode)
            {
                case ESamplerAddressMode::Repeat: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                case ESamplerAddressMode::MirroredRepeat: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
                case ESamplerAddressMode::ClampToEdge: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                case ESamplerAddressMode::ClampToBorder: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
                default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            }
        }

        D3D12_COMPARISON_FUNC GetSamplerComparisonFunc(const ECompareOp op)
        {
            return GetComparisonFunc(op);
        }

        void GetBorderColor(const ESamplerBorderColor color, float outColor[4])
        {
            switch (color)
            {
                case ESamplerBorderColor::FloatOpaqueWhite:
                case ESamplerBorderColor::IntOpaqueWhite:
                    outColor[0] = outColor[1] = outColor[2] = outColor[3] = 1.0f;
                    break;
                case ESamplerBorderColor::FloatOpaqueBlack:
                case ESamplerBorderColor::IntOpaqueBlack:
                    outColor[0] = outColor[1] = outColor[2] = 0.0f;
                    outColor[3] = 1.0f;
                    break;
                default:
                    outColor[0] = outColor[1] = outColor[2] = outColor[3] = 0.0f;
                    break;
            }
        }

        SInputSemantic GetInputSemantic(
            const BufferBinding& binding,
            const BufferAttribute& attribute
        )
        {
            const auto& name = attribute.GetName();
            const auto instance = binding.GetInputRate() == EInputRate::Instance;

            if (name == "Position" && instance) return { "INSTANCE_POSITION", 0 };
            if (name == "Position") return { "POSITION", 0 };
            if (name == "Pos") return { "POSITION", 0 };
            if (name == "Normal") return { "NORMAL", 0 };
            if (name == "Size") return { "INSTANCE_SIZE", 0 };
            if (name == "Border") return { "BORDER", 0 };
            if (name == "InsetShadow") return { "SHADOW", 0 };
            if (name == "DropShadow") return { "SHADOW", 1 };
            if (name == "Color") return { "COLOR", 0 };
            if (name == "OutlineColor") return { "OUTLINE", 0 };
            if (name == "OutlineThickness") return { "OUTLINE", 1 };
            if (name == "TextureIndex") return { "TEXTURE", 0 };
            if (name == "AtlasIndex") return { "TEXTURE", 0 };
            if (name == "ScissorRect") return { "SCISSOR", 0 };
            if (name == "TexCoords") return { "TEXCOORD", 0 };
            if (name == "UnitRange") return { "UNITRANGE", 0 };
            if (name == "PositionSize") return { "POSITION", instance ? 1u : 0u };
            if (name == "VelocityAge") return { "TEXCOORD", 0 };
            if (name == "Transform") return { "TEXCOORD", 1 };
            if (name == "TangentRibbonId") return { "TANGENT", 0 };
            if (name == "Metadata") return { "TEXCOORD", instance ? 2u : 3u };

            return { name, 0 };
        }
    }
}

#endif
