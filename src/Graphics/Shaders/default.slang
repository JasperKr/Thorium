// Default shader

module default;

struct VertexStageOutput
{
    float4 sv_position : SV_Position;
    float3 color       : COLOR;
};

struct VertexInput
{
    float3 Position : POSITION;
    float3 Color    : COLOR;
};

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input)
{
    VertexStageOutput out;
    out.sv_position = float4(input.Position, 1.0);
    out.color = input.Color;
    return out;
}

struct FragmentInput
{
    float3 color : COLOR;
};

Sampler2D test;
ConstantBuffer<float[]> BufferA;


[shader("fragment")]
float4 fragmentMain(FragmentInput input)
{
    return float4(input.color, 1.0) * test.Sample(float2(0.5, 0.5)) + BufferA[0];
}
