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
        Vertex = 0,
        Hull,
        Domain,
        Geometry,
        Pixel,
        Compute,
        Count
    };

    GENERATE_ENUM_CLASS_OPERATORS(EShaderStage)

    struct SRenderingInfo
    {
        Ref<Image> ColorAttachment;
        Ref<DepthStencilImage> DepthStencilAttachment = nullptr;
        Extent2D RenderArea;
    };
}

