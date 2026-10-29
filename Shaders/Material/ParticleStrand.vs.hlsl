#include "../Aether/ParticleData.hlsl"

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
    uint RadialSegments;
    uint Subdivisions;
    float WidthScale;
    uint MaterialIndex;
    uint Padding;
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

// Catmull-Rom position and tangent for a smooth curve through the control points
void catmullRom(float3 p0, float3 p1, float3 p2, float3 p3, float u, out float3 position, out float3 tangent) {
  float u2 = u * u;
  float u3 = u2 * u;
  position = 0.5 * ((2.0 * p1) + ((-p0 + p2) * u) + (((2.0 * p0) - (5.0 * p1) + (4.0 * p2) - p3) * u2) +
                    ((-p0 + (3.0 * p1) - (3.0 * p2) + p3) * u3));
  tangent = 0.5 * ((-p0 + p2) + (((2.0 * p0) - (5.0 * p1) + (4.0 * p2) - p3) * (2.0 * u)) +
                   ((-p0 + (3.0 * p1) - (3.0 * p2) + p3) * (3.0 * u2)));
}

VSOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID) {
  uint strandIndex = instanceId;
  uint pointCount = max(pc.PointCount, 2u);
  uint baseIndex = pc.PointOffset + (strandIndex * pointCount);

  // decode the tube wall: (pointCount-1)*pc.Subdivisions sample segments, each a ring
  // of pc.RadialSegments quads
  uint verticesPerSegment = pc.RadialSegments * 6u;
  uint sampleSegments = (pointCount - 1u) * pc.Subdivisions;
  uint segmentIndex = vertexId / verticesPerSegment;
  uint local = vertexId % verticesPerSegment;
  uint radialQuad = local / 6u;
  uint corner = local % 6u;

  const uint longitudinal[6] = {0u, 0u, 1u, 0u, 1u, 1u};
  const uint radialOffset[6] = {0u, 1u, 1u, 0u, 1u, 0u};

  uint sampleIndex = min(segmentIndex + longitudinal[corner], sampleSegments);
  uint radialIndex = (radialQuad + radialOffset[corner]) % pc.RadialSegments;
  float angle = kTwoPi * (float(radialQuad + radialOffset[corner]) / float(pc.RadialSegments));

  // spline parameter: which control segment and local u within it
  float t = float(sampleIndex) / float(pc.Subdivisions);
  uint seg = min(uint(floor(t)), pointCount - 2u);
  float u = t - float(seg);

  float3 p0 = strandPointAt(baseIndex, seg > 0u ? seg - 1u : 0u);
  float3 p1 = strandPointAt(baseIndex, seg);
  float3 p2 = strandPointAt(baseIndex, seg + 1u);
  float3 p3 = strandPointAt(baseIndex, min(seg + 2u, pointCount - 1u));

  float3 pointPosition;
  float3 tangentRaw;
  catmullRom(p0, p1, p2, p3, u, pointPosition, tangentRaw);
  float3 tangent = safeDirection3(tangentRaw, float3(0.0, 1.0, 0.0));

  // orient the cross-section ring with the rotation-minimizing frame stored per pointIndex,
  // interpolated along the segment. This keeps adjacent rings aligned so the tube wall
  // never folds/pinches at bends.
  float3 sideAxis = safeDirection3(lerp(strandNormalAt(baseIndex, seg), strandNormalAt(baseIndex, seg + 1u), u),
                                 float3(1.0, 0.0, 0.0));
  sideAxis = safeDirection3(sideAxis - (tangent * dot(sideAxis, tangent)), sideAxis);  // re-orthogonalize
  float3 upAxis = cross(tangent, sideAxis);
  float3 radialNormal = (cos(angle) * sideAxis) + (sin(angle) * upAxis);

  SGPUParticle root = particles[pc.ParticleOffset + strandIndex];
  float alive = root.Alive >= 0.5 ? 1.0 : 0.0;
  float along = float(sampleIndex) / float(sampleSegments);
  float rootConverge = smoothstep(0.0, 0.06, along);
  float alongSq = along * along;
  float alongCu = alongSq * along;
  float ribbonShape = ((2.0 * alongCu) - (3.0 * alongSq) + 1.0) + ((alongCu - alongSq) * -2.633787);
  float taper = rootConverge * clamp(ribbonShape, 0.0, 1.0);
  float radius = root.Size * max(root.Transform.y, 0.0) * pc.WidthScale * taper * kRadiusScale;
  radius *= alive;

  float3 worldPosition = pointPosition + (radialNormal * radius);
  VSOutput output;
  output.ClipPos = mul(ViewProj, float4(worldPosition, 1));
  output.WorldPos = worldPosition;
  output.Normal = radialNormal;
  output.TexCoord = float2(along, float(radialQuad + radialOffset[corner]) / float(pc.RadialSegments));
  output.TexCoord1 = output.TexCoord;
  output.Color = float4(root.Color.rgb, root.Color.a * alive);
  output.Valid = alive;
  return output;
}
