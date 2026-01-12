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
    float R, G, B, A;

    explicit SColor(const float r = 1, const float g = 1, const float b = 1, const float a = 1)
        : R(r), G(g), B(b), A(a) {}
};