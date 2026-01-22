#pragma once

#include "glm/glm.hpp"

namespace Elixir::GUI
{
    struct SRect {
        glm::vec2 Position;
        glm::vec2 Size;

        bool Contains(const glm::vec2& point) const {
            return point.x >= Position.x && point.x <= Position.x + Size.x &&
                   point.y >= Position.y && point.y <= Position.y + Size.y;
        }
    };

    struct SColor
    {
        float R = 0, G = 0, B = 0, A = 0;

        SColor(const float r = 0, const float g = 0, const float b = 0, const float a = 0)
            : R(r), G(g), B(b), A(a) {}

        SColor(const SColor& color) : R(color.R), G(color.G), B(color.B), A(color.A) {}
    };

    enum class ELayoutMode : uint8_t
    {
        Horizontal, Vertical, Overlay
    };

    enum class EHorizontalAlignment : uint8_t
    {
        Left, Center, Right, Stretch
    };

    enum class EVerticalAlignment : uint8_t
    {
        Top, Center, Bottom, Stretch
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

    struct SAnchor
    {
        // Normalized positions (0-1) relative to parent.
        // (0, 0) = top-left, (1, 1) = bottom-right.
        float MinX, MinY;   // Anchor minimum (top-left anchor point)
        float MaxX, MaxY;   // Anchor maximum (bottom-right anchor point)

        SAnchor() = default;
        SAnchor(
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
        static SAnchor TopLeft() { return { 0.0f, 0.0f, 0.0f, 0.0f }; }
        static SAnchor TopCenter() { return { 0.5f, 0.0f, 0.5f, 0.0f }; }
        static SAnchor TopRight() { return { 1.0f, 0.0f, 1.0f, 0.0f }; }

        static SAnchor MiddleLeft() { return { 0.0f, 0.5f, 0.0f, 0.5f }; }
        static SAnchor MiddleCenter() { return { 0.5f, 0.5f, 0.5f, 0.5f }; }
        static SAnchor MiddleRight() { return { 1.0f, 0.5f, 1.0f, 0.5f }; }

        static SAnchor BottomLeft() { return { 0.0f, 1.0f, 0.0f, 1.0f }; }
        static SAnchor BottomCenter() { return { 0.5f, 1.0f, 0.5f, 1.0f }; }
        static SAnchor BottomRight() { return { 1.0f, 1.0f, 1.0f, 1.0f }; }

        // Stretch presets
        static SAnchor StretchHorizontal() { return { 0.0f, 0.5f, 1.0f, 0.5f }; }
        static SAnchor StretchVertical() { return { 0.5f, 0.0f, 0.5f, 1.0f }; }
        static SAnchor StretchAll() { return { 0.0f, 0.0f, 1.0f, 1.0f }; }
    };

    struct SConstraint
    {
        // When anchors are the same (non-stretching):
        // Position = offset from anchor point.
        // Size = explicit size.
        glm::vec2 Position;     // Also called "offset"
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
        glm::vec2 Alignment{0.0f, 0.0f};

        // Z-order for overlapping widgets
        int ZOrder = 0;

        SConstraint() :
            Position(0.0f, 0.0f),
            Size(100.0f, 100.0f),
            Offsets{0, 0, 0, 0} {}

        static SConstraint FromPositionAndSize(const glm::vec2& pos, const glm::vec2& size)
        {
            SConstraint constraint;
            constraint.Position = pos;
            constraint.Size = size;
            return constraint;
        }

        static SConstraint FromOffsets(
            const float left,
            const float top,
            const float right,
            const float bottom
        )
        {
            SConstraint constraint;
            constraint.Offsets = { left, top, right, bottom };
            return constraint;
        }
    };
}