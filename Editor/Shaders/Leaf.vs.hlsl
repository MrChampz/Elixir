#include "../../Shaders/Aether/ParticleData.hlsl"

[[vk::binding(0, 0)]] cbuffer cbFrame
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float3 CameraPos;
    float Time;
};
[[vk::binding(1, 0)]] StructuredBuffer<SGPUParticle> particles;
[[vk::binding(2, 0)]] StructuredBuffer<float4> strandPoints;
struct PushConstants
{
    uint ParticleOffset;
    uint PointOffset;
    uint PointCount;
    uint BranchCount;
    uint LeafInterval;
    float LeafLength;
    float LeafWidth;
    float MaturationSegments;
    float4 LeafColor;
    uint LeafShape;
};
[[vk::push_constant]] PushConstants pc;
struct VSOutput
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : POSITION0;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float2 TexCoord1 : TEXCOORD1;
    float4 Color : COLOR;
    nointerpolation float Valid : TEXCOORD2;
};
static const float kTwoPi = 6.28318530718;
static const float kRadiusScale = 0.0016;
float3 safeDirection3(float3 direction, float3 fallback) {
  float lengthSquared = dot(direction, direction);
  if (lengthSquared < 0.000001) {
    return fallback;
  }
  return direction * rsqrt(lengthSquared);
}

float3 strandPointAt(uint baseIndex, uint pointIndex) {
  return strandPoints[(baseIndex + pointIndex) * 3u].xyz;
}

float3 strandNormalAt(uint baseIndex, uint pointIndex) {
  return strandPoints[((baseIndex + pointIndex) * 3u) + 2u].xyz;
}

// Smooth cup, raised midrib, and a slight curled tip in normalized leaf coordinates.
float leafCurvature(float2 uv) {
    return .085 * sin(uv.y * 3.14159265) * (1.0 - 2.5 * uv.x * uv.x) + .045 * uv.y * uv.y - .025 * abs(uv.x);
}

// Emits a lobed, folded leaf anchored to the transported strand frame.
VSOutput leafVertex(uint vertexId, uint strandIndex, uint baseIndex) {
  uint leafIndex = vertexId / 444u;
  uint localVertex = vertexId % 444u;
  bool reverse = localVertex >= 222u;
  uint triangleIndex = (localVertex % 222u) / 3u;
  uint corner = localVertex % 3u;
  uint pointIndex = (leafIndex + 1u) * pc.LeafInterval;
  float3 anchor = strandPointAt(baseIndex, pointIndex);
  float3 tangent = safeDirection3(strandPointAt(baseIndex, pointIndex + 1u) -
      strandPointAt(baseIndex, pointIndex - 1u), float3(0, -1, 0));
  float3 side = strandNormalAt(baseIndex, pointIndex);
  side = safeDirection3(side - tangent * dot(side, tangent), float3(1, 0, 0));
  float3 binormal = safeDirection3(cross(tangent, side), float3(0, 0, 1));
  // Alternate leaves while varying the plane smoothly along each vine.
  float angle = float(leafIndex % 2u) * 3.14159265 + float(leafIndex / 2u) * 0.35 + float(strandIndex) * 1.73;
  float3 direction = safeDirection3(side * cos(angle) + binormal * sin(angle) + tangent * 0.15, side);
  float3 widthAxis = safeDirection3(cross(direction, cross(tangent, direction)), tangent);
  float3 normal = safeDirection3(cross(widthAxis, direction), binormal);
  static const float2 outline[24] = {
      float2(0,.16), float2(-.10,.11), float2(-.27,.14), float2(-.48,.28),
      float2(-.37,.33), float2(-.23,.4), float2(-.37,.47), float2(-.52,.65),
      float2(-.34,.69), float2(-.2,.66), float2(-.13,.82), float2(-.06,.94),
      float2(0,1), float2(.06,.94), float2(.13,.82), float2(.2,.66),
      float2(.34,.69), float2(.52,.65), float2(.37,.47), float2(.23,.4),
      float2(.37,.33), float2(.48,.28), float2(.27,.14), float2(.10,.11)
  };
  float2 blade[24];
  for (uint i = 0; i < 24u; ++i) {
      float angle = 6.28318530718 * float(i) / 24.0;
      float v = .16 + .84 * (.5 - .5 * cos(angle));
      float width = pow(max(sin(3.14159265 * (v - .16) / .84),0.0), .8);
      blade[i] = pc.LeafShape == 1u ? float2(-.5 * sin(angle) * width, v) : outline[i];
  }
  float2 center = float2(0,.48);
  float2 coords[3];
  if (triangleIndex < 72u) {
      uint edge = triangleIndex / 3u;
      uint part = triangleIndex % 3u;
      float2 a = blade[edge], b = blade[(edge + 1u) % 24u];
      float2 innerA = lerp(center, a, .48), innerB = lerp(center, b, .48);
      if (part == 0u) { coords[0]=center; coords[1]=innerA; coords[2]=innerB; }
      else if (part == 1u) { coords[0]=innerA; coords[1]=a; coords[2]=b; }
      else { coords[0]=innerA; coords[1]=b; coords[2]=innerB; }
  } else {
      // A short petiole connects the lobed blade to the vine.
      float2 stalk[4] = {float2(-.015,0),float2(-.012,.17),float2(.012,.17),float2(.015,0)};
      coords[0]=stalk[0]; coords[1]=stalk[triangleIndex == 72u ? 1u : 2u]; coords[2]=stalk[triangleIndex == 72u ? 2u : 3u];
  }
  SGPUParticle root = particles[pc.ParticleOffset + strandIndex];
  float grownSegments = strandPoints[baseIndex * 3u + 2u].w;
  float maturationFront = strandPoints[(baseIndex + 1u) * 3u + 2u].w;
  float maturationSegments = pc.MaturationSegments;
  float maturity = grownSegments > float(pointIndex) ? saturate((maturationFront - float(pointIndex)) / max(maturationSegments, .001)) : 0.0;
  float variationScale = .88 + .24 * frac(float(strandIndex * 23u + leafIndex * 37u) * .6180339);
  float scale = max(root.Transform.y, 0.0) * maturity * variationScale;
  float3 positions[3];
  for (uint i = 0; i < 3u; ++i) {
      float fold = pc.LeafLength * leafCurvature(coords[i]);
      positions[i] = anchor + scale * (widthAxis * coords[i].x * pc.LeafWidth +
          direction * coords[i].y * pc.LeafLength + normal * fold);
  }
  uint index = reverse && corner != 0u ? 3u - corner : corner;
  float3 worldPosition = positions[index];
  float2 uv = coords[index];
  float dx = (leafCurvature(uv + float2(.001,0)) - leafCurvature(uv - float2(.001,0))) / .002;
  float dy = (leafCurvature(uv + float2(0,.001)) - leafCurvature(uv - float2(0,.001))) / .002;
  float3 faceNormal = -safeDirection3(normal - widthAxis * dx * pc.LeafLength / max(pc.LeafWidth,.001) - direction * dy, normal);
  float alive = root.Alive >= 0.5 && maturity > 0.001 ? 1.0 : 0.0;
  VSOutput output;
  output.ClipPos = mul(ViewProj, float4(worldPosition, 1));
  output.WorldPos = worldPosition;
  output.Normal = reverse ? -faceNormal : faceNormal;
  output.TexCoord = float2(coords[index].x + 0.5, coords[index].y);
  output.TexCoord1 = output.TexCoord;
  float variation = 0.85 + 0.15 * frac(float(strandIndex * 31u + leafIndex * 17u) * 0.6180339);
  output.Color = float4(pc.LeafColor.rgb * variation, pc.LeafColor.a * alive);
  output.Valid = alive;
  return output;
}

VSOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    uint chainCount = 1u + pc.BranchCount;
    uint strandIndex = instanceId / chainCount;
    uint chainIndex = instanceId % chainCount;
    uint baseIndex = pc.PointOffset + strandIndex * pc.PointCount * chainCount + chainIndex * pc.PointCount;
    return leafVertex(vertexId, strandIndex, baseIndex);
}
