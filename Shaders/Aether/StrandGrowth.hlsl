#ifndef AETHER_STRAND_GROWTH_HLSL
#define AETHER_STRAND_GROWTH_HLSL
struct SStrandGrowth {
    float4 Controls; // enabled, state (Growing/Mature/Paused), seed bits, reserved
    float4 Variation; // branch node jitter fraction, maximum extra birth delay, reserved
};
[[vk::binding(9, 0)]] StructuredBuffer<SStrandGrowth> strandGrowth;
uint GrowthHash(uint value) {
    value ^= value >> 16u; value *= 0x7feb352du; value ^= value >> 15u;
    value *= 0x846ca68bu; return value ^ (value >> 16u);
}
uint BranchNode(SStrandGrowth config, uint rootSeed, uint branch, uint interval, uint pointCount) {
    uint node = (branch + 1u) * interval;
    uint radius = uint(float(interval > 2u ? (interval - 1u) / 2u : 0u) * config.Variation.x);
    int offset = radius > 0u ? int(GrowthHash(rootSeed ^ (branch * 101u)) % (radius * 2u + 1u)) - int(radius) : 0;
    return uint(clamp(int(node) + offset, 1, int(pointCount) - 2));
}
float BranchExtraDelay(SStrandGrowth config, uint rootSeed, uint branch) {
    return config.Variation.y * (float(GrowthHash(rootSeed ^ (branch * 307u + 17u)) & 0xffffffu) / 16777215.0);
}
#endif
