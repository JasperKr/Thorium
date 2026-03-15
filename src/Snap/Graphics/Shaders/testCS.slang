module testCS;

[[vk::push_constant]]
cbuffer Push
{
    uint count;
    uint increment;
};

StructuredBuffer<uint> Input;
RWStructuredBuffer<uint> Output;

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 tid : SV_DispatchThreadID)
{
    uint idx = tid.x;

    if (idx >= count)
        return;

    Output[idx] = Input[idx] + increment;
}
