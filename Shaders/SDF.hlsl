/**
 * Signed Distance Function for a rectangle with variable corner radii.
 *
 * @param p The point to evaluate, relative to the center of the rectangle.
 * @param b The half-size of the rectangle (width/2, height/2).
 * @param r The corner radii for each corner (top-left, top-right, bottom-right, bottom-left).
 * @return The signed distance from the point to the rectangle's edge. Negative inside, positive outside.
 */
float sdfRect(float2 p, float2 b, float4 r)
{
    // Default: top-left
    float radius = r.x;

    // Determine corner radius based on quadrant
    if (p.x >= 0.0 && p.y >= 0.0)
    {
        radius = r.y; // top-right
    }
    else if (p.x >= 0.0 && p.y < 0.0)
    {
        radius = r.z; // bottom-right
    }
    else if (p.x < 0.0 && p.y < 0.0)
    {
        radius = r.w; // bottom-left
    }
    else
    {
        radius = r.x; // top-left
    }

    float2 q = abs(p) - b + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}