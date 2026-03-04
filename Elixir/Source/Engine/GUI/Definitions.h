#pragma once

#include "glm/glm.hpp"

namespace Elixir::GUI
{
    struct SRect {
        glm::vec2 Position;
        glm::vec2 Size;

        bool Contains(const glm::vec2& point) const
        {
            return point.x >= Position.x && point.x <= Position.x + Size.x &&
                   point.y >= Position.y && point.y <= Position.y + Size.y;
        }

        bool IsValid() const
        {
            return Size.x > 0 && Size.y > 0;
        }
    };

    struct SColor
    {
        float R = 0, G = 0, B = 0, A = 0;

        SColor(const float r = 0, const float g = 0, const float b = 0, const float a = 0)
            : R(r), G(g), B(b), A(a) {}

        SColor(const SColor& color) : R(color.R), G(color.G), B(color.B), A(color.A) {}
    };

    struct SOutline
    {
        SColor Color;
        float Thickness = 0.0f;
    };

    enum class ELayoutMode : uint8_t
    {
        Horizontal, Vertical, Overlay
    };

    enum class EHorizontalAlignment : uint8_t
    {
        Left, Center, Right
    };

    enum class EVerticalAlignment : uint8_t
    {
        Top, Center, Bottom
    };

    enum class EVisibility : uint8_t
    {
        Visible, Hidden
    };

    struct SMargin
    {
        float Left = 0.0f;
        float Top = 0.0f;
        float Right = 0.0f;
        float Bottom = 0.0f;

        explicit SMargin(const float all = 0.0f)
            : Left(all), Top(all), Right(all), Bottom(all) {}

        SMargin(const float horizontal, const float vertical)
            : Left(horizontal), Top(vertical), Right(horizontal), Bottom(vertical) {}

        SMargin(const float left, const float top, const float right, const float bottom)
            : Left(left), Top(top), Right(right), Bottom(bottom) {}

        float GetTotalHorizontal() const { return Left + Right; }
        float GetTotalVertical() const { return Top + Bottom; }
    };

    typedef SMargin SPadding;

    struct SAnchors
    {
        // Normalized positions (0-1) relative to parent.
        // (0, 0) = top-left, (1, 1) = bottom-right.
        float MinX, MinY;   // Anchor minimum (top-left anchor point)
        float MaxX, MaxY;   // Anchor maximum (bottom-right anchor point)

        SAnchors() = default;
        SAnchors(
            const float minX,
            const float minY,
            const float maxX,
            const float maxY
        ) : MinX(minX), MinY(minY), MaxX(maxX), MaxY(maxY) {}

        /**
         * Check if this is a non-stretching anchor (min == max).
         * @return boolean indicating if non-stretching.
         */
        bool IsNonStretching() const
        {
            return MinX == MaxX && MinY == MaxY;
        }

        bool IsStretchingHorizontally() const { return MinX != MaxX; }
        bool IsStretchingVertically() const { return MinY != MaxY; }

        // Common presets
        static SAnchors TopLeft() { return { 0.0f, 0.0f, 0.0f, 0.0f }; }
        static SAnchors TopCenter() { return { 0.5f, 0.0f, 0.5f, 0.0f }; }
        static SAnchors TopRight() { return { 1.0f, 0.0f, 1.0f, 0.0f }; }

        static SAnchors MiddleLeft() { return { 0.0f, 0.5f, 0.0f, 0.5f }; }
        static SAnchors MiddleCenter() { return { 0.5f, 0.5f, 0.5f, 0.5f }; }
        static SAnchors MiddleRight() { return { 1.0f, 0.5f, 1.0f, 0.5f }; }

        static SAnchors                                                                                                                              BottomLeft() { return { 0.0f, 1.0f, 0.0f, 1.0f }; }
        static SAnchors BottomCenter() { return { 0.5f, 1.0f, 0.5f, 1.0f }; }
        static SAnchors BottomRight() { return { 1.0f, 1.0f, 1.0f, 1.0f }; }

        // Stretch presets
        static SAnchors StretchHorizontal() { return { 0.0f, 0.5f, 1.0f, 0.5f }; }
        static SAnchors sStretchVertical() { return { 0.5f, 0.0f, 0.5f, 1.0f }; }
        static SAnchors StretchAll() { return { 0.0f, 0.0f, 1.0f, 1.0f }; }
    };

    struct SConstraint
    {
        // When anchors are the same (non-stretching):
        // Position = offset from anchor point.
        // Size = explicit size.
        glm::vec2 Position;
        glm::vec2 Size;

        // When anchors are different (stretching):
        // These become margins from the anchor edges
        // Left offset = distance from left anchor
        // Right offset = distance from right anchor (usually negative)
        // Top offset = distance from top anchor
        // Bottom offset = distance from bottom anchor (usually negative)
        struct SOffsets
        {
            float Left, Top, Right, Bottom;
        } Offsets;

        // Alignment within the anchor space (0-1)
        // 0.0 = left/top, 0.5 = center, 1.0 = right/bottom
        glm::vec2 Alignment{0.0f, 0.0f}; // TODO: Use Horizontal/Vertical alignment!

        SConstraint() :
            Position(0.0f, 0.0f),
            Size(100.0f, 100.0f),
            Offsets{0, 0, 0, 0} {}
    };
}