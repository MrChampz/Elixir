// Byte layouts of SSkeletalMeshVertex and SStaticMeshVertex on the CPU.
static const uint SKELETAL_VERTEX_STRIDE = 88u;
static const uint STATIC_VERTEX_STRIDE   = 56u;

// Surface attributes have the same byte offsets in both vertex layouts.
static const uint VERTEX_POSITION_OFFSET  = 0u;
static const uint VERTEX_NORMAL_OFFSET    = 12u;
static const uint VERTEX_TANGENT_OFFSET   = 24u;
static const uint VERTEX_TEXCOORD_OFFSET  = 40u;
static const uint VERTEX_TEXCOORD1_OFFSET = 48u;

// Skin influences exist only in SSkeletalMeshVertex.
static const uint SKELETAL_VERTEX_JOINTS_OFFSET  = 56u;
static const uint SKELETAL_VERTEX_WEIGHTS_OFFSET = 72u;

struct SSkinningJob
{
    uint SourceVertexOffset;
    uint OutputVertexOffset;
    uint SBoneMatrixOffset;
    uint VertexCount;
};

// Explicit columns match glm::mat4 without relying on compiler matrix packing.
struct SBoneMatrix
{
    float4 X;
    float4 Y;
    float4 Z;
    float4 W;
};

[[vk::binding(0, 0)]]
ByteAddressBuffer sourceVertices;

[[vk::binding(1, 0)]]
StructuredBuffer<SBoneMatrix> boneMatrices;

[[vk::binding(2, 0)]]
StructuredBuffer<SSkinningJob> jobs;

// .x: JobIndex, .y: FirstVertex
[[vk::binding(3, 0)]]
StructuredBuffer<uint2> blocks;

[[vk::binding(4, 0)]]
RWByteAddressBuffer outputVertices;

struct SPushConstants
{
    uint FirstBlock;
};

[[vk::push_constant]]
SPushConstants pc;

float3 SafeNormalize(float3 value, float3 fallback)
{
    float lengthSquared = dot(value, value);
    return isfinite(lengthSquared) && lengthSquared > 1e-12f
        ? value * rsqrt(lengthSquared)
        : fallback;
}

[numthreads(128, 1, 1)]
void main(uint3 groupId : SV_GroupID, uint lane : SV_GroupIndex)
{
    uint2 block = blocks[pc.FirstBlock + groupId.x];

    uint jobIndex = block.x;
    SSkinningJob job = jobs[jobIndex];

    uint firstVertex = block.y;
    uint vertexIndex = firstVertex + lane;

    if (vertexIndex >= job.VertexCount) return;

    uint source = (job.SourceVertexOffset + vertexIndex) * SKELETAL_VERTEX_STRIDE;
    uint target = (job.OutputVertexOffset + vertexIndex) * STATIC_VERTEX_STRIDE;

    float3 position = asfloat(sourceVertices.Load3(source + VERTEX_POSITION_OFFSET));
    float3 normal   = asfloat(sourceVertices.Load3(source + VERTEX_NORMAL_OFFSET));
    float4 tangent  = asfloat(sourceVertices.Load4(source + VERTEX_TANGENT_OFFSET));

    uint4 joints    = sourceVertices.Load4(source + SKELETAL_VERTEX_JOINTS_OFFSET);
    float4 weights  = asfloat(sourceVertices.Load4(source + SKELETAL_VERTEX_WEIGHTS_OFFSET));

    SBoneMatrix skin = (SBoneMatrix)0;
    [unroll] for (uint i = 0; i < 4; ++i)
    {
        if (weights[i] == 0) continue;

        SBoneMatrix bone = boneMatrices[job.SBoneMatrixOffset + joints[i]];

        skin.X += bone.X * weights[i];
        skin.Y += bone.Y * weights[i];
        skin.Z += bone.Z * weights[i];
        skin.W += bone.W * weights[i];
    }

    float3 skinnedPosition = (skin.X * position.x + skin.Y * position.y +
        skin.Z * position.z + skin.W).xyz;

    float3 cofactorX = cross(skin.Y.xyz, skin.Z.xyz);
    float determinant = dot(skin.X.xyz, cofactorX);
    float handedness = determinant < 0 ? -1.0f : 1.0f;

    float3 skinnedNormal = normal;
    if (isfinite(determinant) && abs(determinant) > 1e-12f)
    {
        skinnedNormal = (cofactorX * normal.x + cross(skin.Z.xyz, skin.X.xyz) * normal.y +
            cross(skin.X.xyz, skin.Y.xyz) * normal.z) / determinant;
    }
    skinnedNormal = SafeNormalize(skinnedNormal, float3(0, 1, 0));

    float3 referenceAxis = abs(skinnedNormal.z) < 0.9f
            ? float3(0, 0, 1)
            : float3(0, 1, 0);

    float3 skinnedTangent = skin.X.xyz * tangent.x + skin.Y.xyz * tangent.y +
        skin.Z.xyz * tangent.z;
    skinnedTangent -= skinnedNormal * dot(skinnedNormal, skinnedTangent);
    skinnedTangent = SafeNormalize(
        skinnedTangent,
        normalize(cross(skinnedNormal, referenceAxis))
    );
    float4 finalTangent = float4(skinnedTangent, tangent.w * handedness);

    float3 finalPosition = all(isfinite(skinnedPosition))
        ? skinnedPosition
        : position;

    float2 texCoord  = sourceVertices.Load2(source + VERTEX_TEXCOORD_OFFSET);
    float2 texCoord1 = sourceVertices.Load2(source + VERTEX_TEXCOORD1_OFFSET);

    outputVertices.Store3(target + VERTEX_POSITION_OFFSET,  asuint(finalPosition));
    outputVertices.Store3(target + VERTEX_NORMAL_OFFSET,    asuint(skinnedNormal));
    outputVertices.Store4(target + VERTEX_TANGENT_OFFSET,   asuint(finalTangent));
    outputVertices.Store2(target + VERTEX_TEXCOORD_OFFSET,  texCoord);
    outputVertices.Store2(target + VERTEX_TEXCOORD1_OFFSET, texCoord1);
}
