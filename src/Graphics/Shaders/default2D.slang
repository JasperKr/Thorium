// Default shader

module default2D;

struct VertexStageOutput
{
    float4 sv_position : SV_Position;
    float2 tex_coord   : TEXCOORD0;
    float4 color       : COLOR;
};

struct VertexInput
{
    float2 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color    : COLOR;
};

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input)
{
    VertexStageOutput out;
    out.sv_position = float4(input.Position, 1.0);
    out.color = input.Color;
    out.tex_coord = input.TexCoord;
    return out;
}

struct FragmentInput
{
    float2 tex_coord : TEXCOORD0;
    float4 color : COLOR;
};

Sampler2D MainTexture;

uniform float test;
uniform float3 testColor;
uniform float4x4 testMatrix;

ConstantBuffer<float4x4> testUBO;

struct PushConstants
{
    float4x4 model;
    float2 aaaaaa;
    float4 b;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pc;

RWStructuredBuffer<PushConstants> outputBuffer;

[shader("fragment")]
float4 fragmentMain(FragmentInput input)
{
  float4 texColor = MainTexture.Sample(input.tex_coord);
    texColor.rgb += testColor * test;
  return input.color * texColor;
}
