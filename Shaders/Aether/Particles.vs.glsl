#version 450

layout(location = 0) in vec4 inPositionSize;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform FrameData {
    mat4 ViewProj;
} cbFrame;

void main() {
  vec4 pos = vec4(inPositionSize.xy, 0.0, 1.0);
  gl_Position = cbFrame.ViewProj * pos;
  gl_PointSize = inPositionSize.z;

  outColor = inColor;
}
