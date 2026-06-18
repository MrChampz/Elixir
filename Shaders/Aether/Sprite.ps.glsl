#version 450

layout(location = 0) in vec4 inColor;
layout(location = 0) out vec4 outColor;

void main() {
  vec2 centered = (gl_PointCoord * 2.0) - 1.0;

  float radiusSquared = dot(centered, centered);
  float alpha = smoothstep(1.08, 0.18, radiusSquared);
  float glow = smoothstep(0.36, 0.0, radiusSquared);

  vec3 color = inColor.rgb + (glow * 0.35);
  outColor = vec4(color, inColor.a * alpha);
}
