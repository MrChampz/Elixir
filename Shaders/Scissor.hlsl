static const float4 INVALID_RECT = float4(-1, -1, -1, -1);

bool IsClipRectValid(float4 clipRect)
{
    return any(clipRect != INVALID_RECT);
}