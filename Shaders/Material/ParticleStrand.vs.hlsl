#include "../Aether/ParticleData.hlsl"
#include "../Aether/StrandAnchors.hlsl"

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
    uint BranchCount;
    uint Debug;
    uint EmitterIndex;
    uint GeometryPadding;
    float RootWidthScale;
    float TipWidthScale;
    float WidthExponent;
    float BranchWidthScale;
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
    nointerpolation float SurfaceKind : TEXCOORD3;
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

#define safeNormalize3 safeDirection3
#include "../Aether/StrandSurfaces.hlsl"
#undef safeNormalize3

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

// Draws point crosses, analytic surface normals, and the current tip guidance.
VSOutput debugVertex(uint vertexId, uint rootIndex, uint baseIndex) {
  float grown = strandPoints[baseIndex * 3u + 2u].w;
  uint pointIndex = vertexId / 48u;
  bool tipArrow = pointIndex >= pc.PointCount;
  if (tipArrow) pointIndex = min(pc.PointCount - 1u, uint(ceil(grown)));
  float3 position = strandPointAt(baseIndex, pointIndex);
  float3 tangent = pointIndex > 0u ? position - strandPointAt(baseIndex, pointIndex - 1u) : float3(0, 1, 0);
  tangent = safeDirection3(tangent, float3(0, 1, 0));
  SStrandSurfaces surfaces = strandSurfaces[pc.EmitterIndex];
  uint surfaceIndex;
  float3 closest, normal = float3(0, 0, 1);
  bool contact = surfaces.Controls.x > 0 && FindSurface(surfaces, position, surfaceIndex, closest, normal);
  uint marker = asuint(strandPoints[(baseIndex + pointIndex) * 3u + 1u].w);
  bool attached = pointIndex > 0u && marker > 0u && marker <= uint(surfaces.Controls.x);
  if (attached) {
      float distance;
      QuerySurface(surfaces, marker - 1u, position, distance, normal, closest);
      contact = true;
  }
  bool released = pointIndex > 0u && strandPoints[(baseIndex + pointIndex) * 3u + 1u].w < 0;
  uint firstChainBase = pc.PointOffset + rootIndex * pc.PointCount * (1u + pc.BranchCount);
  bool endpointPinned = baseIndex == firstChainBase && pointIndex == pc.PointCount - 1u && strandEndAnchors[pc.EmitterIndex].w > .5;
  float3 color = (attached || endpointPinned || pointIndex == 0u) ? float3(0.1, 1, 0.2) : (released ? float3(1, 0.65, 0.05) : float3(1, 0.15, 0.1));
  uint localVertex = vertexId % 48u;
  uint corner = localVertex % 6u;
  if (localVertex % 12u >= 6u && corner % 3u != 0u) corner += corner % 3u == 1u ? 1u : -1u;
  const float2 corners[6] = {float2(0,-1), float2(1,1), float2(1,-1), float2(0,-1), float2(0,1), float2(1,1)};
  float2 uv = corners[corner];
  float3 viewDirection = safeDirection3(CameraPos - position, float3(0, 0, 1));
  float3 right = safeDirection3(cross(viewDirection, float3(0, 1, 0)), float3(1, 0, 0));
  float3 up = safeDirection3(cross(right, viewDirection), float3(0, 1, 0));
  float valid = particles[pc.ParticleOffset + rootIndex].Alive >= 0.5 && grown > 0 && float(pointIndex) <= ceil(grown) ? 1.0 : 0.0;
  if (tipArrow || localVertex >= 36u) {
      float3 arrow = tipArrow ? safeDirection3(strandPoints[baseIndex * 3u + 1u].xyz, tangent) : normal;
      float3 side = safeDirection3(cross(arrow, viewDirection), right);
      position += arrow * uv.x * 0.1 + side * uv.y * (0.004 * (1.0 - 0.8 * uv.x));
      color = tipArrow ? float3(0.1, 0.8, 1) : float3(1, 0.95, 0.15);
      if (!tipArrow && !contact) valid = 0;
  } else {
      uint axis = localVertex / 12u;
      float3 a = axis == 0u ? right : (axis == 1u ? up : safeDirection3(right + up, right));
      float3 b = safeDirection3(cross(a, viewDirection), up);
      position += viewDirection * 0.006 + a * (uv.x - 0.5) * 0.026 + b * uv.y * 0.003;
  }
  VSOutput output;
  output.ClipPos = mul(ViewProj, float4(position, 1));
  output.WorldPos = position;
  output.Normal = viewDirection;
  output.TexCoord = float2(0, 0);
  output.TexCoord1 = output.TexCoord;
  output.Color = float4(color, 1);
  output.Valid = valid;
  output.SurfaceKind = 2;
  return output;
}

VSOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID) {
  uint strandIndex = instanceId / (pc.BranchCount + 1u);
  uint chainIndex = instanceId % (pc.BranchCount + 1u);
  uint pointCount = max(pc.PointCount, 2u);
  uint baseIndex = pc.PointOffset + instanceId * pointCount;
  uint tubeVertices = (pointCount - 1u) * pc.Subdivisions * pc.RadialSegments * 6u;
  if (pc.Debug != 0u && vertexId >= tubeVertices)
      return debugVertex(vertexId - tubeVertices, strandIndex, baseIndex);

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
  float grownSegments = strandPoints[baseIndex * 3u + 2u].w;
  float t = min(float(sampleIndex) / float(pc.Subdivisions), grownSegments);
  uint activeTip = min(pointCount - 1u, max(1u, uint(ceil(grownSegments))));
  uint seg = min(uint(floor(t)), activeTip - 1u);
  float u = t - float(seg);

  float3 p0 = strandPointAt(baseIndex, seg > 0u ? seg - 1u : 0u);
  float3 p1 = strandPointAt(baseIndex, seg);
  float3 p2 = strandPointAt(baseIndex, seg + 1u);
  float3 p3 = strandPointAt(baseIndex, min(seg + 2u, activeTip));

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
  float alive = root.Alive >= 0.5 && float(segmentIndex) / float(pc.Subdivisions) < grownSegments ? 1.0 : 0.0;
  float along = t / max(grownSegments, 0.001);
  // Endpoint multipliers apply along the currently grown length of each chain.
  float widthBlend = pow(saturate(along), pc.WidthExponent);
  float taper = lerp(pc.RootWidthScale, pc.TipWidthScale, widthBlend);
  float radius = root.Size * max(root.Transform.y, 0.0) * pc.WidthScale * taper * kRadiusScale;
  radius *= alive * (chainIndex > 0u ? pc.BranchWidthScale : 1.0);

  float3 worldPosition = pointPosition + (radialNormal * radius);
  VSOutput output;
  output.ClipPos = mul(ViewProj, float4(worldPosition, 1));
  output.WorldPos = worldPosition;
  output.Normal = radialNormal;
  output.TexCoord = float2(along, float(radialQuad + radialOffset[corner]) / float(pc.RadialSegments));
  output.TexCoord1 = output.TexCoord;
  output.Color = float4(root.Color.rgb, root.Color.a * alive);
  output.Valid = alive;
  output.SurfaceKind = 0;
  return output;
}
