#pragma once

#include "glm/glm.hpp"

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