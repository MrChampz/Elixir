#pragma once

namespace Elixir
{
    typedef struct Offset2D
    {
        int32_t X, Y;

        Offset2D() = default;
        Offset2D(int32_t x, int32_t y) : X(x), Y(y) {}
        Offset2D(const Offset2D& offset) : X(offset.X), Y(offset.Y) {}
    } Offset2D;

    typedef struct Offset3D
    {
        int32_t X, Y, Z;

        Offset3D() = default;
        Offset3D(int32_t x, int32_t y, int32_t z) : X(x), Y(y), Z(z) {}
        Offset3D(const Offset3D& offset) : X(offset.X), Y(offset.Y), Z(offset.Z) {}

    } Offset3D;

    typedef struct Extent2D
    {
        uint32_t Width, Height;

        Extent2D() = default;
        Extent2D(uint32_t width, uint32_t height) : Width(width), Height(height) {}
        Extent2D(const Extent2D& extent) : Width(extent.Width), Height(extent.Height) {}
    } Extent2D;

    typedef struct Extent3D
    {
        uint32_t Width, Height, Depth;

        Extent3D() = default;

        Extent3D(uint32_t width, uint32_t height, uint32_t depth)
            : Width(width), Height(height), Depth(depth) {}

        Extent3D(const Extent2D& extent)
            : Width(extent.Width), Height(extent.Height), Depth(1) {}

        Extent3D(const Extent3D& extent)
            : Width(extent.Width), Height(extent.Height), Depth(extent.Depth) {}
    } Extent3D;

    typedef struct Rect2D
    {
        Offset2D Offset;
        Extent2D Extent;
    } Rect2D;

    typedef struct Viewport
    {
        float X, Y;
        float Width, Height;
        float MinDepth, MaxDepth;
    } Viewport;
}