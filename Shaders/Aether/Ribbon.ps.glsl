#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main() {
    float edgeFade = smoothstep(0.0, 0.18, inUV.y) * smoothstep(1.0, 0.82, inUV.y);
    float headGlow = pow(1.0 - inUV.x, 4.0);

    vec3 color = inColor.rgb + headGlow * 0.35;
    outColor = vec4(color, inColor.a * edgeFade);
}
