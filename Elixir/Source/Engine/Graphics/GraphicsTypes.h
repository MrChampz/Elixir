#pragma once

namespace Elixir
{
    class Image;
    class DepthStencilImage;

    enum class EPrimitiveTopology : uint8_t
    {
        PointList = 0,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        TriangleFan,
        LineListWithAdjacency,
        LineStripWithAdjacency,
        TriangleListWithAdjacency,
        TriangleStripWithAdjacency,
        PatchList
    };

    enum class EPolygonMode : uint8_t
    {
        Fill = 0,
        Line,
        Point
    };

    enum class ECullMode : uint8_t
    {
        None = 0,
        Front,
        Back,
        FrontAndBack
    };

    GENERATE_ENUM_CLASS_OPERATORS(ECullMode)

    enum class EFrontFace : uint8_t
    {
        CounterClockwise = 0,
        Clockwise
    };

    enum class ESampleCount : uint8_t
    {
        _1 = 0, _2, _4, _8, _16, _32, _64
    };

    GENERATE_ENUM_CLASS_OPERATORS(ESampleCount)

    typedef uint32_t SampleMask;

    enum class ELogicOp : uint8_t
    {
        Clear = 0,
        And,
        AndReverse,
        Copy,
        AndInverted,
        NoOp,
        Xor,
        Or,
        Nor,
        Equivalent,
        Invert,
        OrReverse,
        CopyInverted,
        OrInverted,
        Nand,
        Set
    };

    enum class EBlendOp : uint8_t
    {
        Add = 0,
        Subtract,
        ReverseSubtract,
        Min,
        Max
    };

    enum class EBlendFactor : uint8_t
    {
        Zero = 0,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha,
        ConstantColor,
        OneMinusConstantColor,
        ConstantAlpha,
        OneMinusConstantAlpha,
        SrcAlphaSaturate,
        Src1Color,
        OneMinusSrc1Color,
        Src1Alpha,
        OneMinusSrc1Alpha,
    };

    enum class EColorComponent : uint8_t
    {
        R = 0, G, B, A
    };

    GENERATE_ENUM_CLASS_OPERATORS(EColorComponent)

    enum class ECompareOp : uint8_t
    {
        Never = 0,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    enum class EStencilOp : uint8_t
    {
        Keep = 0,
        Zero,
        Replace,
        IncrementAndClamp,
        DecrementAndClamp,
        Invert,
        IncrementAndWrap,
        DecrementAndWrap
    };

    enum class EShaderStage : uint8_t
    {
        None        = 0,
        Vertex      = 1 << 0,
        Hull        = 1 << 1,
        Domain      = 1 << 2,
        Geometry    = 1 << 3,
        Pixel       = 1 << 4,
        Compute     = 1 << 5,
        All         = Vertex | Hull | Domain | Geometry | Pixel,
        Count       = 6,
    };

    GENERATE_ENUM_CLASS_OPERATORS(EShaderStage)

    enum class EPipelineStage : uint32_t
    {
        None = 0,
        TopOfPipe           = 1 << 0,
        DrawIndirect        = 1 << 1,
        VertexInput         = 1 << 2,
        VertexShader        = 1 << 3,
        HullShader          = 1 << 4,
        DomainShader        = 1 << 5,
        GeometryShader      = 1 << 6,
        PixelShader         = 1 << 7,
        ComputeShader       = 1 << 8,
        AllTransfer         = 1 << 9,
        Transfer            = 1 << 10,
        BottomOfPipe        = 1 << 11
    };

    GENERATE_ENUM_CLASS_OPERATORS(EPipelineStage)

    enum class EPipelineAccess : uint32_t
    {
        None = 0,
        IndirectCommandRead             = 1 << 0,
        IndexRead                       = 1 << 1,
        VertexAttributeRead             = 1 << 2,
        UniformRead                     = 1 << 3,
        InputAttachmentRead             = 1 << 4,
        ShaderRead                      = 1 << 5,
        ShaderWrite                     = 1 << 6,
        ColorAttachmentRead             = 1 << 7,
        ColorAttachmentWrite            = 1 << 8,
        DepthStencilAttachmentRead      = 1 << 9,
        DepthStencilAttachmentWrite     = 1 << 10,
        TransferRead                    = 1 << 11,
        TransferWrite                   = 1 << 12,
        HostRead                        = 1 << 13,
        HostWrite                       = 1 << 14,
        MemoryRead                      = 1 << 15,
        MemoryWrite                     = 1 << 16,
        ShaderSampledRead               = 1 << 17,
        ShaderStorageRead               = 1 << 18,
        ShaderStorageWrite              = 1 << 19,
    };

    GENERATE_ENUM_CLASS_OPERATORS(EPipelineAccess)

    struct SRenderingInfo
    {
        Ref<Image> ColorAttachment;
        Ref<DepthStencilImage> DepthStencilAttachment = nullptr;
        Extent2D RenderArea;
    };
}

