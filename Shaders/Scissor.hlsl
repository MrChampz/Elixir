static const float4 INVALID_RECT = float4(-1, -1, -1, -1);

bool isScissorRectValid(float4 scissorRect)
{
    return any(scissorRect != INVALID_RECT);
}