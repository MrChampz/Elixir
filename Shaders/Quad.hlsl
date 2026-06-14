#define xBits 0x26
#define yBits 0x32

/**
 * Generate quad positions using bit manipulation (in normalized coordinates).
 * Positions: 0,0 -> 1,1 -> 1,0 -> 0,0 -> 0,1 -> 1,1
 * Indices:   0      1      2      3      4      5
 * x: 0, 1, 1, 0, 0, 1 = bits: 100110 = 0x26
 * y: 0, 1, 0, 0, 1, 1 = bits: 110010 = 0x32
 */
float2 CalculateQuadPosition(uint vertexId)
{
    float2 pos;
    pos.x = float((uint(xBits) >> vertexId) & 1);
    pos.y = float((uint(yBits) >> vertexId) & 1);
    return pos;
}