float LinearizeDepth(float depth, vec2 focalLength)
{
    return (focalLength.x * focalLength.y) / (depth * (focalLength.x - focalLength.y) + focalLength.y);
}

vec4 PixelToProjection(vec2 screenCoord, float depth)
{
    // we use DX depth range [0,1], for GL where depth is [-1,1], we would need depth * 2 - 1 too
    return vec4(screenCoord * 2.0f - 1.0f, depth * 2.0f - 1.0f, 1.0f);
}

vec4 PixelToView(vec2 screenCoord, float depth, mat4 invProjection)
{
    vec4 projectionSpace = PixelToProjection(screenCoord, depth);
    vec4 viewSpace = invProjection * projectionSpace;
    viewSpace /= viewSpace.w;
    return viewSpace;
}

vec4 PixelToWorld(vec2 screenCoord, float depth, mat4 invView, mat4 invProjection)
{
    vec4 viewSpace = PixelToView(screenCoord, depth, invProjection);
    return invView * viewSpace;
}
