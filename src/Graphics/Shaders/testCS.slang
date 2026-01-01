module testCS;

[[vk::push_constant]]
cbuffer Push
{
    uint count;
    uint increment;
};

StructuredBuffer<uint> Input;
RWStructuredBuffer<uint> Output;

[WaveSize(32)]
[numthreads(64, 1, 1)]
void computeMain(uint3 tid : SV_DispatchThreadID)
{
    uint idx = tid.x;

    uint adjCount = count + increment;
    uint inc      = increment + 1;

    if (idx < adjCount)
    {
        Output[idx] = Input[idx] + inc;
    }
}
