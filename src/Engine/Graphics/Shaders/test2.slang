// Define push constants
[[vk::push_constant]]
cbuffer PushConstants
{
    uint2 textureSize;   // Width, Height of the texture
    float valueToAdd;    // Value to add to each pixel
};

// RW texture
RWTexture2D<float4> outTexture;

[shader("compute")]
[numthreads(16,16,1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // Return if out of bounds
    uint x = dispatchThreadID.x;
    uint y = dispatchThreadID.y;
    if (x >= textureSize.x || y >= textureSize.y) {
        return;
    }

    // Read, add, write
    float4 current = outTexture[int2(x, y)];
    outTexture[int2(x, y)] = current + valueToAdd;
}